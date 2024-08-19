#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_win32.h"
#include "imgui_internal.h"
#ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#endif

#include <cmath>
#include <iostream>
#include <source_location>
#include <vector>

#include "windows.h"
#include "glew.h"
#include "wglew.h"
#include "timeapi.h"
#include "xaudio2.h"
#include "xinput.h"

#include "bf_base.h"
#include "bf_game.h"

global_var Library_Integration_Data* global_library_integration_data = {};

// NOLINTBEGIN(bugprone-suspicious-include)
#include "bf_context.cpp"

#include "bf_memory_arena.cpp"
#include "bf_strings.cpp"

#include "bf_file_h.cpp"
#include "bf_log_h.cpp"

#include "bf_math.cpp"
#include "bf_file.cpp"
#include "bf_log.cpp"
#include "bf_memory.cpp"

#include "bfc_opengl.cpp"
// NOLINTEND(bugprone-suspicious-include)

// -- RENDERING STUFF
struct BF_Bitmap {
    Game_Bitmap bitmap;

    HBITMAP    handle;
    BITMAPINFO info;
};
// -- RENDERING STUFF END

// -- GAME STUFF
global_var Context* ctx = {};

global_var Arena initial_game_memory_arena = {};

global_var size_t events_count    = 0;
global_var std::vector<u8> events = {};

template <typename T>
void Push_Event(T& event) {
    auto can_push = events.capacity() >= events.size() + sizeof(T) + 1;
    if (!can_push) {
        INVALID_PATH;

        CTX_LOGGER;
        LOG_ERROR("win32_platform: Push_Event(): skipped");

        return;
    }

    events_count++;
    events.push_back((u8)T::_event_type);

    auto data = (u8*)&event;
    events.insert(events.end(), data, data + sizeof(T));
}

global_var HMODULE game_lib     = nullptr;
global_var bool    hot_reloaded = false;

#if BF_DEBUG
global_var FILETIME last_game_dll_write_time;

struct Peek_Filetime_Result {
    bool     success;
    FILETIME filetime;
};

Peek_Filetime_Result Peek_Filetime(const char* filename) {
    Peek_Filetime_Result res{};

    WIN32_FIND_DATAA find_data;
    auto             handle = FindFirstFileA(filename, &find_data);

    if (handle != INVALID_HANDLE_VALUE) {
        res.success  = true;
        res.filetime = find_data.ftLastWriteTime;
        Assert(FindClose(handle));
    }

    return res;
}
#endif

using Game_Update_And_Render_t = Game_Update_And_Render_function((*));
Game_Update_And_Render_function(Game_Update_And_Render_stub) {}
Game_Update_And_Render_t Game_Update_And_Render_ = Game_Update_And_Render_stub;

void Load_Or_Update_Game_Dll() {
    auto path = "bf_game.dll";

#if BF_DEBUG
    auto filetime = Peek_Filetime(path);
    if (!filetime.success)
        return;
    if (CompareFileTime(&last_game_dll_write_time, &filetime.filetime) == 0)
        return;

    SYSTEMTIME systemtime;
    FileTimeToSystemTime(&filetime.filetime, &systemtime);

    char systemtime_fmt[4096];
    sprintf(
        systemtime_fmt,
        "%04d%02d%02d-%02d%02d%02d",
        (int)systemtime.wYear,
        (int)systemtime.wMonth,
        (int)systemtime.wDay,
        (int)systemtime.wHour,
        (int)systemtime.wMinute,
        (int)systemtime.wSecond
    );

    char temp_path[4096];
    sprintf(temp_path, "game_%s.dll", systemtime_fmt);
    if (CopyFileA(path, temp_path, FALSE) == FALSE)
        return;

    if (game_lib) {
        if (!FreeLibrary(game_lib)) {
            DEBUG_Error("ERROR: Win32: Load_Or_Update_Game_Dll: FreeLibrary failed!");
            INVALID_PATH;
        }

        hot_reloaded            = true;
        game_lib                = nullptr;
        Game_Update_And_Render_ = nullptr;
    }

    path = temp_path;
#endif

    Game_Update_And_Render_ = Game_Update_And_Render_stub;

    HMODULE lib = LoadLibraryA(path);
    if (!lib) {
        DEBUG_Error("ERROR: Win32: Load_Or_Update_Game_Dll: LoadLibraryA failed!");
        INVALID_PATH;
    }

    auto loaded_Game_Update_And_Render
        = (Game_Update_And_Render_t)GetProcAddress(lib, "Game_Update_And_Render");

    bool functions_loaded = loaded_Game_Update_And_Render;
    if (!functions_loaded) {
        DEBUG_Error("ERROR: Win32: Load_Or_Update_Game_Dll: Functions couldn't be loaded!"
        );
        INVALID_PATH;
    }

#if BF_DEBUG
    global_library_integration_data->game_context_set = false;
    last_game_dll_write_time                          = filetime.filetime;
    global_library_integration_data->dll_reloads_count++;
#endif

    game_lib                = lib;
    Game_Update_And_Render_ = loaded_Game_Update_And_Render;
}
// -- GAME STUFF END

// -- CONTROLLER STUFF
using XInputGetStateType = DWORD (*)(DWORD dwUserIndex, XINPUT_STATE* pState);
using XInputSetStateType = DWORD (*)(DWORD dwUserIndex, XINPUT_VIBRATION* pVibration);

// NOTE: These get executed if xinput1_4.dll / xinput1_3.dll could not get loaded
DWORD XInputGetStateStub(DWORD /* dwUserIndex */, XINPUT_STATE* /* pState */) {
    return ERROR_DEVICE_NOT_CONNECTED;
}
DWORD XInputSetStateStub(DWORD /* dwUserIndex */, XINPUT_VIBRATION* /* pVibration */) {
    return ERROR_DEVICE_NOT_CONNECTED;
}

XInputGetStateType XInputGetState_ = XInputGetStateStub;
XInputSetStateType XInputSetState_ = XInputSetStateStub;

void Load_XInput_Dll() {
    auto library = LoadLibraryA("xinput1_4.dll");
    if (!library)
        library = LoadLibraryA("xinput1_3.dll");

    if (library) {
        XInputGetState_ = (XInputGetStateType)GetProcAddress(library, "XInputGetState");
        XInputSetState_ = (XInputSetStateType)GetProcAddress(library, "XInputSetState");
    }
}
// -- CONTROLLER STUFF END

// -- XAUDIO STUFF
const u32 SAMPLES_HZ           = 48000;
bool      audio_support_loaded = false;

using XAudio2CreateType = HRESULT (*)(IXAudio2**, UINT32, XAUDIO2_PROCESSOR);

HRESULT
XAudio2CreateStub(
    IXAudio2** /* ppXAudio2 */,
    UINT32 /* Flags */,
    XAUDIO2_PROCESSOR /* XAudio2Processor */
) {
    // TODO: Diagnostic
    return XAUDIO2_E_INVALID_CALL;
}

XAudio2CreateType XAudio2Create_ = XAudio2CreateStub;

void Load_XAudio_Dll() {
    auto library = LoadLibraryA("xaudio2_9.dll");
    if (!library)
        library = LoadLibraryA("xaudio2_8.dll");
    if (!library)
        library = LoadLibraryA("xaudio2_7.dll");

    if (!library) {
        // TODO: Diagnostic
        return;
    }

    XAudio2Create_ = (XAudio2CreateType)GetProcAddress(library, "XAudio2Create");
}

f32 Fill_Samples(
    i16* samples,
    i32  samples_count_per_second,
    i32  samples_count_per_channel,
    i8   channels,
    f32  frequency,
    f32  last_angle
) {
    Assert(samples_count_per_second > 0);
    Assert(samples_count_per_channel > 0);
    Assert(channels > 0);
    Assert(frequency > 0);
    Assert(last_angle >= 0);

#if 0
    const i16 volume = 6400;
#else
    const i16 volume = 0;
#endif

    f32 samples_per_oscillation = (f32)samples_count_per_second / frequency;
    f32 angle_step              = BF_2PI / samples_per_oscillation;

    for (int i = 0; i < samples_count_per_channel; i++) {
        last_angle += angle_step;

        // TODO: Implement our own sin function
        for (int k = 0; k < channels; k++) {
            auto val                  = volume * sinf(last_angle * (f32)(k + 1));
            samples[i * channels + k] = (i16)val;
        }
    }

    while (last_angle > BF_2PI)
        last_angle -= BF_2PI;

    return last_angle;
}

struct Create_Buffer_Res {
    XAUDIO2_BUFFER* buffer;
    u8*             samples;
};

Create_Buffer_Res
Create_Buffer(i32 samples_per_channel, i32 channels, i32 bytes_per_sample) {
    Assert(channels > 0);
    Assert(bytes_per_sample > 0);

    auto  b      = new XAUDIO2_BUFFER();
    auto& buffer = *b;

    i64  total_bytes = (i64)samples_per_channel * channels * bytes_per_sample;
    auto samples     = new u8[total_bytes]();

    buffer.Flags      = XAUDIO2_END_OF_STREAM;
    buffer.AudioBytes = total_bytes;
    buffer.pAudioData = samples;
    buffer.PlayBegin  = 0;
    buffer.PlayLength = samples_per_channel;
    buffer.LoopBegin  = 0;
    buffer.LoopLength = 0;
    buffer.LoopCount  = 0;
    buffer.pContext   = nullptr;

    return {b, samples};
}
// -- XAUDIO STUFF END

global_var bool running = false;

global_var bool      should_recreate_bitmap_after_client_area_resize;
global_var BF_Bitmap screen_bitmap;

global_var int client_width  = -1;
global_var int client_height = -1;

void Win32UpdateBitmap(HDC device_context) {
    Assert(client_width >= 0);
    Assert(client_height >= 0);

    auto& game_bitmap  = screen_bitmap.bitmap;
    game_bitmap.width  = client_width;
    game_bitmap.height = client_height;

    game_bitmap.bits_per_pixel                 = 32;
    screen_bitmap.info.bmiHeader.biSize        = sizeof(screen_bitmap.info.bmiHeader);
    screen_bitmap.info.bmiHeader.biWidth       = client_width;
    screen_bitmap.info.bmiHeader.biHeight      = -client_height;
    screen_bitmap.info.bmiHeader.biPlanes      = 1;
    screen_bitmap.info.bmiHeader.biBitCount    = game_bitmap.bits_per_pixel;
    screen_bitmap.info.bmiHeader.biCompression = BI_RGB;

    if (game_bitmap.memory)
        VirtualFree(game_bitmap.memory, 0, MEM_RELEASE);

    game_bitmap.memory = VirtualAlloc(
        nullptr,
        game_bitmap.width * screen_bitmap.bitmap.height * game_bitmap.bits_per_pixel / 8,
        MEM_RESERVE | MEM_COMMIT,
        PAGE_EXECUTE_READWRITE
    );

    if (screen_bitmap.handle)
        DeleteObject(screen_bitmap.handle);

    screen_bitmap.handle = CreateDIBitmap(
        device_context,
        &screen_bitmap.info.bmiHeader,
        0,
        game_bitmap.memory,
        &screen_bitmap.info,
        DIB_RGB_COLORS
    );
}

void Win32Paint(f32 dt, HWND /* window_handle */, HDC device_context) {
    if (should_recreate_bitmap_after_client_area_resize)
        Win32UpdateBitmap(device_context);

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();

    Game_Update_And_Render_(
        dt,
        initial_game_memory_arena.base + initial_game_memory_arena.used,
        initial_game_memory_arena.size - initial_game_memory_arena.used,
        screen_bitmap.bitmap,
        (void*)events.data(),
        events_count,
        *global_library_integration_data,
        hot_reloaded
    );

    ImGui::Render();
    // glClear(GL_COLOR_BUFFER_BIT);
    // glViewport(ImGui::GetMainViewport());
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    SwapBuffers(device_context);
    BFGL_Check_Errors();

    events_count = 0;
    events.clear();

#if BF_DEBUG
    hot_reloaded = false;
    Load_Or_Update_Game_Dll();
#endif
}

void Win32GLResize() {
    glViewport(0, 0, client_width, client_height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, 1, 1, 0, -1, 1);
}

extern IMGUI_IMPL_API LRESULT
ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

#define BF_MOUSE_POS                            \
    STATEMENT({                                 \
        POINT p;                                \
        GetCursorPos(&p);                       \
        ScreenToClient(window_handle, &p);      \
        event.position.x = p.x;                 \
        event.position.y = client_height - p.y; \
    })

void* Win32_Open_File(const char* filename) noexcept {
    FILE* file_handle = nullptr;
    auto  error       = fopen_s(&file_handle, filename, "w");
    Assert_False(error);
    Assert(file_handle != nullptr);
    return file_handle;
}

void Win32_Write_To_File(void* file_handle, const char* text) noexcept {
    fprintf((FILE*)file_handle, "%s\n", text);
    fflush((FILE*)file_handle);
    OutputDebugStringA(Text_Format("%s\n", text));
}

void* Win32_Open_Binary_File(const char* filename) noexcept {
    FILE* file_handle = nullptr;
    auto  error       = fopen_s(&file_handle, filename, "wb");
    Assert_False(error);
    Assert(file_handle != nullptr);
    return file_handle;
}

void Win32_Write_To_Binary_File(void* file_handle, void* data, i32 bytes) noexcept {
    Assert(bytes != 0);
    auto result = fwrite((void*)data, bytes, 1, (FILE*)file_handle);
    Assert(result != 0);
}

bool Win32_File_Exists(const char* filepath) {
    DWORD dwAttrib = GetFileAttributes(filepath);
    auto  result
        = (dwAttrib != INVALID_FILE_ATTRIBUTES) && !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY);

    return (bool)result;
}

void Win32_Die() noexcept {
    exit(-1);
}

struct Window_Info : public Equatable<Window_Info> {
    i32 width;
    i32 height;
    i32 pos_x;
    i32 pos_y;
    i32 maximized;

    [[nodiscard]] bool Equal_To(const Window_Info& other) const {
        auto result = (width == other.width)       //
                      && (height == other.height)  //
                      && (pos_x == other.pos_x)    //
                      && (pos_y == other.pos_y)    //
                      && (maximized == other.maximized);
        return result;
    }
};

global_var Window_Info window_info = {};

inline static const char* window_info_filepath = "window_info.dat";

constexpr double window_info_save_time_seconds = 0.5;
global_var bool  window_info_needs_saving      = false;
global_var f64   window_info_saving_time       = -f64_inf;

bool Restore_Window_Info() {
    CTX_LOGGER;
    u8 buffer[sizeof(Window_Info) + 1] = {};

    Arena trash_arena = {};
    trash_arena.base  = buffer;
    trash_arena.size  = sizeof(Window_Info) + 1;

    if (Win32_File_Exists(window_info_filepath)) {
        auto load_result
            = Debug_Load_File_To_Arena(window_info_filepath, trash_arena, ctx);
        if (!load_result.success) {
            LOG_ERROR("could not load %s", window_info_filepath);
            return false;
        }

        auto wi = (Window_Info*)load_result.output;

        auto is_valid = (load_result.size == sizeof(Window_Info))  //
                        && (wi->width > 0)                         //
                        && (wi->height > 0);

        if (!is_valid) {
            LOG_ERROR("%s is corrupted. Ignoring it", window_info_filepath);
            return false;
        }

        window_info = *wi;
        LOG_INFO("Loaded %s!", window_info_filepath);
    }

    return true;
}

void Debug_Save_Binary_File(const char* filepath, void* data, i32 bytes) {
    auto fp = Win32_Open_Binary_File(filepath);
    if (!fp)
        exit(-1);

    Win32_Write_To_Binary_File(fp, data, bytes);
    auto close_result = fclose((FILE*)fp);
    Assert(close_result == 0);
}

void Save_Window_Info() {
    CTX_LOGGER;

    SCOPED_LOG_INIT("Save_Window_Info");
    u8 buffer[sizeof(Window_Info)] = {};

    *(Window_Info*)buffer = window_info;

    Debug_Save_Binary_File(window_info_filepath, (void*)buffer, sizeof(Window_Info));

    window_info_needs_saving = false;
}

u64 Win32Clock() {
    LARGE_INTEGER res;
    QueryPerformanceCounter(&res);
    return res.QuadPart;
}

u64 Win32Frequency() {
    LARGE_INTEGER res;
    QueryPerformanceFrequency(&res);
    return res.QuadPart;
}

global_var u64 perf_counter_frequency  = 0;
global_var u64 perf_counter_started_at = 0;

f64 Win32_Get_Time() noexcept {
    Assert(perf_counter_started_at != 0);
    Assert(perf_counter_frequency != 0);

    auto result
        = (f64)(Win32Clock() - perf_counter_started_at) / (f64)perf_counter_frequency;
    return result;
}

global_var bool window_launched = false;

LRESULT
WindowEventsHandler(HWND window_handle, UINT messageType, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(window_handle, messageType, wParam, lParam))
        return 1;

    auto mouse_captured = false;
    if (global_library_integration_data->imgui_context != nullptr)
        mouse_captured
            = global_library_integration_data->imgui_context->IO.WantCaptureMouse;

    switch (messageType) {
    case WM_CLOSE: {  // NOLINT(bugprone-branch-clone)
        running = false;
    } break;

    case WM_DESTROY: {
        // TODO: It was an error. Should we try to recreate the window?
        running = false;
    } break;

    case WM_WINDOWPOSCHANGED: {
        auto window_pos = *(tagWINDOWPOS*)lParam;
        if (window_launched) {
            auto old_window_info = window_info;

            window_info.pos_x  = window_pos.x;
            window_info.pos_y  = window_pos.y;
            window_info.width  = window_pos.cx;
            window_info.height = window_pos.cy;

            window_info_needs_saving = (old_window_info != window_info);
            if (window_info_needs_saving)
                window_info_saving_time
                    = Win32_Get_Time() + window_info_save_time_seconds;
        }

        return DefWindowProc(window_handle, messageType, wParam, lParam);
    } break;

    case WM_SIZE: {
        client_width                                    = LOWORD(lParam);
        client_height                                   = HIWORD(lParam);
        should_recreate_bitmap_after_client_area_resize = true;
        Win32GLResize();

        if (window_launched) {
            b32 was_maximized    = (wParam == SIZE_MAXIMIZED);
            b32 was_just_resized = (wParam == SIZE_RESTORED);

            auto old_window_info = window_info;

            if (was_maximized)
                window_info.maximized = 1;
            if (was_just_resized)
                window_info.maximized = 0;

            if (was_maximized || was_just_resized) {
                window_info_needs_saving = (old_window_info != window_info);
                if (window_info_needs_saving)
                    window_info_saving_time
                        = Win32_Get_Time() + window_info_save_time_seconds;
            }
        }
    } break;

    case WM_LBUTTONDOWN: {
        if (!mouse_captured) {
            Mouse_Pressed event{};
            event.type = Mouse_Button_Type::Left;
            BF_MOUSE_POS;
            Push_Event(event);
        }
    } break;

    case WM_LBUTTONUP: {
        if (!mouse_captured) {
            Mouse_Released event{};
            event.type = Mouse_Button_Type::Left;
            BF_MOUSE_POS;
            Push_Event(event);
        }
    } break;

    case WM_RBUTTONDOWN: {
        if (!mouse_captured) {
            Mouse_Pressed event{};
            event.type = Mouse_Button_Type::Right;
            BF_MOUSE_POS;
            Push_Event(event);
        }
    } break;

    case WM_RBUTTONUP: {
        if (!mouse_captured) {
            Mouse_Released event{};
            event.type = Mouse_Button_Type::Right;
            BF_MOUSE_POS;
            Push_Event(event);
        }
    } break;

    case WM_MBUTTONDOWN: {
        if (!mouse_captured) {
            Mouse_Pressed event{};
            event.type = Mouse_Button_Type::Middle;
            BF_MOUSE_POS;
            Push_Event(event);
        }
    } break;

    case WM_MBUTTONUP: {
        if (!mouse_captured) {
            Mouse_Released event{};
            event.type = Mouse_Button_Type::Middle;
            BF_MOUSE_POS;
            Push_Event(event);
        }
    } break;

    case WM_MOUSEWHEEL: {
        if (!mouse_captured) {
            Mouse_Scrolled event{};
            i16            scroll = (short)HIWORD(wParam);
            event.value           = scroll;
            Push_Event(event);
        }
    } break;

    case WM_MOUSEMOVE: {
        if (!mouse_captured) {
            Mouse_Moved event{};
            BF_MOUSE_POS;
            Push_Event(event);
        }
    } break;

    case WM_KEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP: {
        bool previous_was_down = lParam & (1 << 30);
        bool is_up             = lParam & (1 << 31);
        bool alt_is_down       = lParam & (1 << 29);
        u32  vk_code           = wParam;

        if (is_up != previous_was_down)
            return 0;

        // if (vk_code == 'W') {
        // } else if (vk_code == 'A') {
        // } else if (vk_code == 'S') {
        // } else if (vk_code == 'D') {
        // } else
        if (vk_code == VK_ESCAPE  //
            || vk_code == 'Q'     //
            || vk_code == VK_F4 && alt_is_down)
        {
            running = false;
        }

        // if (is_up) {
        //     // OnKeyReleased
        // } else {
        //     // OnKeyPressed
        // }
    } break;

    default: {
        return DefWindowProc(window_handle, messageType, wParam, lParam);
    }
    }
    return 0;
}

class BFVoiceCallback  // NOLINT(cppcoreguidelines-virtual-class-destructor)
    : public IXAudio2VoiceCallback {
public:
    f32 frequency                 = -1;
    i32 samples_count_per_channel = -1;
    i8  channels                  = -1;

    XAUDIO2_BUFFER* b1 = nullptr;
    XAUDIO2_BUFFER* b2 = nullptr;
    u8*             s1 = nullptr;
    u8*             s2 = nullptr;

    IXAudio2SourceVoice* voice = nullptr;

    void RecalculateAndSwap() {
        Validate();

        auto& samples = s1;
        auto& buffer  = b1;

        last_angle = Fill_Samples(
            (i16*)samples,
            SAMPLES_HZ,
            samples_count_per_channel,
            channels,
            frequency,
            last_angle
        );

        auto res = voice->SubmitSourceBuffer(buffer);
        if (FAILED(res)) {
            // TODO: Diagnostic
        }

        std::swap(b1, b2);
        std::swap(s1, s2);
    }

    void OnStreamEnd() noexcept override {
        RecalculateAndSwap();
    }

    void OnBufferEnd(void* pBufferContext) noexcept override {}
    void OnBufferStart(void* pBufferContext) noexcept override {}
    void OnLoopEnd(void* pBufferContext) noexcept override {}
    void OnVoiceError(void* pBufferContext, HRESULT Error) noexcept override {}
    void OnVoiceProcessingPassEnd() noexcept override {}
    void OnVoiceProcessingPassStart(UINT32 BytesRequired) noexcept override {}

    void Validate() {
        Assert(samples_count_per_channel > 0);
        Assert(channels > 0);
        Assert(last_angle >= 0);

        Assert(b1 != nullptr);
        Assert(b2 != nullptr);
        Assert(s1 != nullptr);
        Assert(s2 != nullptr);
        Assert(voice != nullptr);
    }

private:
    f32 last_angle = 0;
};

//-----------------------------------------------------------------------------------
// Logging.
//-----------------------------------------------------------------------------------
#if 1
using Root_Logger_Type = Tracing_Logger;

#    define SET_LOGGER                                                               \
        Root_Logger_Type logger{};                                                   \
        Initialize_Tracing_Logger(logger, initial_game_memory_arena);                \
        ctx_.logger_data          = (void*)&logger;                                  \
        ctx_.logger_routine       = (void_func)Tracing_Logger_Routine;               \
        ctx_.logger_scope_routine = (void_func)Tracing_Logger_Scope_Routine;         \
        global_library_integration_data->logger_data          = ctx_.logger_data;    \
        global_library_integration_data->logger_routine       = ctx_.logger_routine; \
        global_library_integration_data->logger_scope_routine = ctx_.logger_scope_routine;

#else
// NOTE: Отключение логирования
using Root_Logger_Type = void*;
#    define SET_LOGGER ((void*)0)
#endif

// NOLINTBEGIN(clang-analyzer-core.StackAddressEscape)
static int WinMain(
    HINSTANCE application_handle,
    HINSTANCE /* previous_window_instance_handle */,
    LPSTR /* command_line */,
    int show_command
) {
    Library_Integration_Data l{};
    global_library_integration_data = &l;

    global_library_integration_data->Open_File     = Win32_Open_File;
    global_library_integration_data->Write_To_File = Win32_Write_To_File;
    global_library_integration_data->Get_Time      = Win32_Get_Time;
    global_library_integration_data->Die           = Win32_Die;

    initial_game_memory_arena.size = Megabytes(64LL);
    initial_game_memory_arena.base = (u8*)VirtualAlloc(
        nullptr,
        initial_game_memory_arena.size,
        MEM_RESERVE | MEM_COMMIT,
        PAGE_EXECUTE_READWRITE
    );

    if (!initial_game_memory_arena.base) {
        // TODO: Diagnostic
        return -1;
    }

    Context ctx_{};
    ctx = &ctx_;
    SET_LOGGER;
    CTX_LOGGER;

    perf_counter_frequency  = Win32Frequency();
    perf_counter_started_at = Win32Clock();

    Load_Or_Update_Game_Dll();
    Load_XInput_Dll();
    Load_XAudio_Dll();

    events.reserve(Kilobytes(64LL));

    const auto SLEEP_MSEC_GRANULARITY = 1;
    timeBeginPeriod(SLEEP_MSEC_GRANULARITY);

    // --- XAudio stuff ---
    IXAudio2*               xaudio       = nullptr;
    IXAudio2MasteringVoice* master_voice = nullptr;
    IXAudio2SourceVoice*    source_voice = nullptr;

    const u32 duration_msec    = 20;
    const u32 bytes_per_sample = 2;
    const u32 channels         = 2;

    const f32 starting_frequency = 523.25f / 2;

    BFVoiceCallback voice_callback{};
    voice_callback.frequency                 = starting_frequency;
    voice_callback.samples_count_per_channel = -1;
    voice_callback.channels                  = channels;

    XAUDIO2_BUFFER* buffer1  = nullptr;
    XAUDIO2_BUFFER* buffer2  = nullptr;
    u8*             samples1 = nullptr;
    u8*             samples2 = nullptr;

    // TODO: Am I supposed to dynamically load xaudio2.dll?
    // What about targeting different versions of xaudio on different OS?
    if (SUCCEEDED(CoInitializeEx(nullptr, COINIT_MULTITHREADED))) {
        if (SUCCEEDED(XAudio2Create_(&xaudio, 0, XAUDIO2_DEFAULT_PROCESSOR))) {
            if (SUCCEEDED(xaudio->CreateMasteringVoice(&master_voice))) {
                WAVEFORMATEX voice_struct{};
                voice_struct.wFormatTag      = WAVE_FORMAT_PCM;
                voice_struct.nChannels       = channels;
                voice_struct.nSamplesPerSec  = SAMPLES_HZ;
                voice_struct.nAvgBytesPerSec = channels * SAMPLES_HZ * bytes_per_sample;
                voice_struct.nBlockAlign     = channels * bytes_per_sample;
                voice_struct.wBitsPerSample  = bytes_per_sample * 8;
                voice_struct.cbSize          = 0;

                auto res = xaudio->CreateSourceVoice(
                    &source_voice,
                    &voice_struct,
                    0,
                    // TODO: Revise max frequency ratio
                    // https://learn.microsoft.com/en-us/windows/win32/api/xaudio2/nf-xaudio2-ixaudio2-createsourcevoice
                    XAUDIO2_DEFAULT_FREQ_RATIO,
                    &voice_callback,
                    nullptr,
                    nullptr
                );

                if (SUCCEEDED(res)) {
                    Assert((SAMPLES_HZ * duration_msec) % 1000 == 0);

                    i32 samples_count_per_channel = SAMPLES_HZ * duration_msec / 1000;
                    Assert(samples_count_per_channel > 0);

                    auto r1 = Create_Buffer(
                        samples_count_per_channel, channels, bytes_per_sample
                    );
                    auto r2 = Create_Buffer(
                        samples_count_per_channel, channels, bytes_per_sample
                    );

                    buffer1  = r1.buffer;
                    samples1 = r1.samples;
                    buffer2  = r2.buffer;
                    samples2 = r2.samples;

                    voice_callback.b1                        = buffer1;
                    voice_callback.b2                        = buffer2;
                    voice_callback.s1                        = samples1;
                    voice_callback.s2                        = samples2;
                    voice_callback.voice                     = source_voice;
                    voice_callback.samples_count_per_channel = samples_count_per_channel;

                    voice_callback.RecalculateAndSwap();
                    voice_callback.RecalculateAndSwap();

                    // DURING RUNTIME!
                    // auto res1 = source_voice->SubmitSourceBuffer(buffer1);
                    // voice_callback.recalculate();
                    // source_voice->SubmitSourceBuffer(buffer1);
                    // if (SUCCEEDED(res1)) {
                    //     audio_support_loaded = true;
                    // } else {
                    //     delete samples1;
                    //     delete samples2;
                    //     samples1 = nullptr;
                    //     samples2 = nullptr;
                    //     delete buffer1;
                    //     delete buffer2;
                    //     buffer1 = nullptr;
                    //     buffer2 = nullptr;
                    //     voice_callback.b1 = nullptr;
                    //     voice_callback.b2 = nullptr;
                    //     voice_callback.s1 = nullptr;
                    //     voice_callback.s2 = nullptr;
                    //
                    //     // TODO: Diagnostic
                    // }
                    //
                }
                else {
                    // TODO: Diagnostic
                    Assert(!source_voice);
                }
            }
            else {
                // TODO: Diagnostic
                Assert(!master_voice);
            }
        }
        else {
            // TODO: Diagnostic
            Assert(!xaudio);
        }
    }
    // --- XAudio stuff end ---

    WNDCLASSA windowClass{};
    // NOTE: Casey says that OWNDC is what makes us able
    // not to ask the OS for a new DC each time we need to draw if I understood correctly.
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    // windowClass.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    windowClass.lpfnWndProc   = *WindowEventsHandler;
    windowClass.lpszClassName = "BFGWindowClass";
    windowClass.hInstance     = application_handle;

    // TODO: Icon!
    // HICON     hIcon;

    if (RegisterClassA(&windowClass) == NULL) {
        // TODO: Diagnostic
        return -1;
    }

    if (source_voice != nullptr) {
        auto res = source_voice->Start(0);
        // TODO: Diagnostic
        Assert(SUCCEEDED(res));
    }

    auto window_handle = CreateWindowExA(
        0,
        windowClass.lpszClassName,
#if BF_DEBUG
        "Roads of Horses - DEBUG",
#else
        "Roads of Horses",
#endif
        WS_TILEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        640,
        480,
        nullptr,             // [in, optional] HWND      hWndParent,
        nullptr,             // [in, optional] HMENU     hMenu,
        application_handle,  // [in, optional] HINSTANCE hInstance,
        nullptr              // [in, optional] LPVOID    lpParam
    );

    if (!window_handle) {
        // TODO: Diagnostic
        return -1;
    }

    {
        auto restored = Restore_Window_Info();

        ShowWindow(window_handle, show_command);
        UpdateWindow(window_handle);

        window_launched = true;
        auto maximized  = restored && window_info.maximized;

        if (restored && (window_info.width > 0) && (window_info.height > 0)) {
            SetWindowPos(
                window_handle,
                nullptr,
                window_info.pos_x,
                window_info.pos_y,
                window_info.width,
                window_info.height,
                0
            );
        }

        if (maximized)
            SendMessage(window_handle, WM_SYSCOMMAND, SC_MAXIMIZE, 0);
    }

    Assert(client_width >= 0);
    Assert(client_height >= 0);
    // --- Initializing OpenGL Start ---
    {
        auto hdc = GetDC(window_handle);

        // --- Setting up pixel format start ---
        PIXELFORMATDESCRIPTOR pfd{};
        pfd.nSize       = sizeof(PIXELFORMATDESCRIPTOR);
        pfd.nVersion    = 1;
        pfd.dwFlags     = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
        pfd.dwLayerMask = PFD_MAIN_PLANE;
        pfd.iPixelType  = PFD_TYPE_RGBA;
        pfd.cColorBits  = 24;
        pfd.cAlphaBits  = 8;
        // pfd.cDepthBits = 0;
        // pfd.cAccumBits = 0;
        // pfd.cStencilBits = 0;

        auto pixelformat = ChoosePixelFormat(hdc, &pfd);
        if (!pixelformat) {
            // TODO: Diagnostic
            return -1;
        }

        DescribePixelFormat(hdc, pixelformat, pfd.nSize, &pfd);

        if (SetPixelFormat(hdc, pixelformat, &pfd) == FALSE) {
            // TODO: Diagnostic
            return -1;
        }
        // --- Setting up pixel format end ---

        auto ghRC = wglCreateContext(hdc);
        wglMakeCurrent(hdc, ghRC);

        // glewExperimental = GL_TRUE;
        if (glewInit() != GLEW_OK) {
            // TODO: Diagnostic
            // fprintf(stderr, "Error: %s\n", glewGetErrorString(err));
            INVALID_PATH;
            return -1;
        }

        // NOTE: Enabling VSync
        // https://registry.khronos.org/OpenGL/extensions/EXT/WGL_EXT_swap_control.txt
        // https://registry.khronos.org/OpenGL/extensions/EXT/WGL_EXT_swap_control_tear.txt
        if (WGLEW_EXT_swap_control_tear)
            wglSwapIntervalEXT(-1);
        else if (WGLEW_EXT_swap_control)
            wglSwapIntervalEXT(1);

        glEnable(GL_BLEND);
        glClearColor(1, 0, 1, 1);
        BFGL_Check_Errors();

        glShadeModel(GL_SMOOTH);
        BFGL_Check_Errors();

        ReleaseDC(window_handle, hdc);
        Win32GLResize();
    }
    // --- Initializing OpenGL End ---

    // --- ImGui Stuff ---
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

    ImGui::StyleColorsDark();

    global_library_integration_data->imgui_context = ImGui::GetCurrentContext();

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_InitForOpenGL(window_handle);
    ImGui_ImplOpenGL3_Init();
    // --- ImGui Stuff End ---

    screen_bitmap = BF_Bitmap();

    running = true;

    u64 perf_counter_current = Win32Clock();

    f32       last_frame_dt = 0;
    const f32 MAX_FRAME_DT  = 1.0f / 10.0f;

    f32 REFRESH_RATE       = 60.0f;
    i64 frames_before_flip = (i64)((f32)(perf_counter_frequency) / REFRESH_RATE);

    while (running) {
        u64 next_frame_expected_perf_counter = perf_counter_current + frames_before_flip;

        MSG message{};

        while (PeekMessageA(&message, nullptr, 0, 0, PM_REMOVE) != 0) {
            if (message.message == WM_QUIT) {
                running = false;
                break;
            }

            TranslateMessage(&message);
            DispatchMessage(&message);

            if (window_info_needs_saving) {
                if (window_info_saving_time < Win32_Get_Time())
                    Save_Window_Info();
            }
        }

        if (!running)
            break;

        // CONTROLLER STUFF
        // TODO: Improve on latency?
        for (DWORD i = 0; i < XUSER_MAX_COUNT; i++) {
            XINPUT_STATE state{};

            DWORD dwResult = XInputGetState_(i, &state);

            if (dwResult == ERROR_SUCCESS) {
                // Controller is connected
                const f32 scale              = 32768;
                f32       stick_x_normalized = (f32)state.Gamepad.sThumbLX / scale;
                f32       stick_y_normalized = (f32)state.Gamepad.sThumbLY / scale;

                Controller_Axis_Changed event{};
                event.axis  = 0;
                event.value = stick_x_normalized;
                Push_Event(event);

                event.axis  = 1;
                event.value = stick_y_normalized;
                Push_Event(event);

                voice_callback.frequency
                    = starting_frequency * powf(2, stick_y_normalized);
            }
            else {
                // TODO: Handling disconnects
            }
        }
        // CONTROLLER STUFF END

        auto device_context = GetDC(window_handle);

        auto capped_dt = MIN(last_frame_dt, MAX_FRAME_DT);
        Win32Paint(capped_dt, window_handle, device_context);
        ReleaseDC(window_handle, device_context);

        FrameMark;

        u64 perf_counter_new = Win32Clock();
        last_frame_dt        = (f32)(perf_counter_new - perf_counter_current)
                        / (f32)perf_counter_frequency;
        Assert(last_frame_dt >= 0);

        if (perf_counter_new < next_frame_expected_perf_counter) {
            while (perf_counter_new < next_frame_expected_perf_counter) {
                i32 msec_to_sleep
                    = (i32)((f32)(next_frame_expected_perf_counter - perf_counter_new)
                            * 1000.0f / (f32)perf_counter_frequency);
                Assert(msec_to_sleep >= 0);

                if (msec_to_sleep >= 2 * SLEEP_MSEC_GRANULARITY) {
                    Sleep(msec_to_sleep - SLEEP_MSEC_GRANULARITY);
                }

                perf_counter_new = Win32Clock();
            }
        }
        else {
            // TODO: There go your frameskips...
        }

        last_frame_dt = (f32)(perf_counter_new - perf_counter_current)
                        / (f32)perf_counter_frequency;
        perf_counter_current = perf_counter_new;
    }

    // if (samples1 != nullptr)
    //     delete[] samples1;
    // if (samples2 != nullptr)
    //     delete[] samples2;
    // if (buffer1 != nullptr)
    //     delete buffer1;
    // if (buffer2 != nullptr)
    //     delete buffer2;
    //
    // TODO: How am I supposed to release it?
    // source_voice
    //
    // if (master_voice != nullptr) {
    //     // TODO: How am I supposed to release it?
    //     //
    //     //
    //     https://learn.microsoft.com/en-us/windows/win32/xaudio2/how-to--initialize-xaudio2
    //     // > Ensure that all XAUDIO2 child objects are fully released
    //     // > before you release the IXAudio2 object.
    //
    //     // master_voice->Release();
    // }
    //

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    if (xaudio)
        xaudio->Release();

    return 0;
}
// NOLINTEND(clang-analyzer-core.StackAddressEscape)
