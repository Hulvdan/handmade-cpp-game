#include "windows.h"
#include "xaudio2.h"
#include "xinput.h"
#include <cassert>
#include <cmath>
#include <iostream>
#include <vector>
#include "glm/glm.hpp"
#include "glm/ext.hpp"
#include "glm/gtx/matrix_transform_2d.hpp"
#include "glew.h"
#include "wglew.h"
#include "bftypes.h"
#include "game.h"

#define local_persist static
#define global_variable static
#define internal static

#define Kilobytes(value) ((value) * 1024)
#define Megabytes(value) (Kilobytes(value) * 1024)
#define Gigabytes(value) (Megabytes(value) * 1024)
#define Terabytes(value) (Gigabytes(value) * 1024)

static constexpr f32 BF_PI = 3.14159265359f;
static constexpr f32 BF_2PI = 6.28318530718f;

// -- RENDERING STUFF
struct BFBitmap {
    GameBitmap bitmap;

    HBITMAP handle;
    BITMAPINFO info;
};
// -- RENDERING STUFF END

// -- GAME STUFF
global_variable HMODULE game_lib = nullptr;
global_variable void* game_memory = nullptr;

global_variable size_t events_count = 0;
global_variable std::vector<u8> events = {};

// TODO(hulvdan): Is there any way to restrict T
// to be only one of event structs specified in game.h?
template <typename T>
void push_event(T& event)
{
    auto can_push = events.capacity() >= events.size() + sizeof(T) + 1;
    if (!can_push) {
        // TODO(hulvdan): Diagnostic
        assert(false);
    }

    events_count++;
    events.push_back((u8)T::_event_type);

    auto data = (u8*)&event;
    events.insert(events.end(), data, data + sizeof(T));
}

#if BFG_INTERNAL
global_variable FILETIME last_game_dll_write_time;

struct PeekFiletimeRes {
    bool success;
    FILETIME filetime;
};

PeekFiletimeRes PeekFiletime(const char* filename)
{
    PeekFiletimeRes res = {};

    WIN32_FIND_DATAA find_data;
    auto handle = FindFirstFileA(filename, &find_data);

    if (handle != INVALID_HANDLE_VALUE) {
        res.success = true;
        res.filetime = find_data.ftLastWriteTime;
        assert(FindClose(handle));
    }

    return res;
}
#endif

using Game_UpdateAndRender_Type = void (*)(f32, void*, GameBitmap&, void*, size_t);
void Game_UpdateAndRender_Stub(f32 dt, void* memory_, GameBitmap& bitmap, void*, size_t) {}
Game_UpdateAndRender_Type Game_UpdateAndRender_ = Game_UpdateAndRender_Stub;

void LoadOrUpdateGameDll()
{
    auto path = "game.dll";

#if BFG_INTERNAL
    auto filetime = PeekFiletime(path);
    if (!filetime.success)
        return;
    if (CompareFileTime(&last_game_dll_write_time, &filetime.filetime) == 0)
        return;

    SYSTEMTIME systemtime;
    FileTimeToSystemTime(&filetime.filetime, &systemtime);

    char systemtime_fmt[4096];
    sprintf(
        systemtime_fmt, "%04d%02d%02d-%02d%02d%02d", (int)systemtime.wYear, (int)systemtime.wMonth,
        (int)systemtime.wDay, (int)systemtime.wHour, (int)systemtime.wMinute,
        (int)systemtime.wSecond);

    char temp_path[4096];
    sprintf(temp_path, "game_%s.dll", systemtime_fmt);
    if (CopyFileA(path, temp_path, FALSE) == FALSE)
        return;

    if (game_lib) {
        assert(FreeLibrary(game_lib));
        game_lib = 0;
    }

    path = temp_path;
#endif

    Game_UpdateAndRender_ = Game_UpdateAndRender_Stub;

    HMODULE lib = LoadLibraryA(path);
    if (!lib) {
        // TODO(hulvdan): Diagnostic
        return;
    }

    auto loaded_Game_UpdateAndRender =
        (Game_UpdateAndRender_Type)GetProcAddress(lib, "Game_UpdateAndRender");

    bool functions_loaded = loaded_Game_UpdateAndRender;
    if (!functions_loaded) {
        // TODO(hulvdan): Diagnostic
        return;
    }

#if BFG_INTERNAL
    last_game_dll_write_time = filetime.filetime;
#endif

    game_lib = lib;
    Game_UpdateAndRender_ = loaded_Game_UpdateAndRender;
}
// -- GAME STUFF END

// -- CONTROLLER STUFF
using XInputGetStateType = DWORD (*)(DWORD dwUserIndex, XINPUT_STATE* pState);
using XInputSetStateType = DWORD (*)(DWORD dwUserIndex, XINPUT_VIBRATION* pVibration);

// NOTE(hulvdan): These get executed if xinput1_4.dll / xinput1_3.dll could not get loaded
DWORD XInputGetStateStub(DWORD dwUserIndex, XINPUT_STATE* pState)
{
    return ERROR_DEVICE_NOT_CONNECTED;
}
DWORD XInputSetStateStub(DWORD dwUserIndex, XINPUT_VIBRATION* pVibration)
{
    return ERROR_DEVICE_NOT_CONNECTED;
}

bool controller_support_loaded = false;
XInputGetStateType XInputGetState_ = XInputGetStateStub;
XInputSetStateType XInputSetState_ = XInputSetStateStub;

void LoadXInputDll()
{
    auto library = LoadLibraryA("xinput1_4.dll");
    if (!library)
        library = LoadLibraryA("xinput1_3.dll");

    if (library) {
        XInputGetState_ = (XInputGetStateType)GetProcAddress(library, "XInputGetState");
        XInputSetState_ = (XInputSetStateType)GetProcAddress(library, "XInputSetState");

        controller_support_loaded = true;
    }
}
// -- CONTROLLER STUFF END

// -- XAUDIO STUFF
const u32 SAMPLES_HZ = 48000;
bool audio_support_loaded = false;

using XAudio2CreateType = HRESULT (*)(IXAudio2**, UINT32, XAUDIO2_PROCESSOR);

HRESULT XAudio2CreateStub(IXAudio2** ppXAudio2, UINT32 Flags, XAUDIO2_PROCESSOR XAudio2Processor)
{
    // TODO(hulvdan): Diagnostic
    return XAUDIO2_E_INVALID_CALL;
}

XAudio2CreateType XAudio2Create_ = XAudio2CreateStub;

void LoadXAudioDll()
{
    auto library = LoadLibraryA("xaudio2_9.dll");
    if (!library)
        library = LoadLibraryA("xaudio2_8.dll");
    if (!library)
        library = LoadLibraryA("xaudio2_7.dll");

    if (!library) {
        // TODO(hulvdan): Diagnostic
        return;
    }

    XAudio2Create_ = (XAudio2CreateType)GetProcAddress(library, "XAudio2Create");
}

f32 FillSamples(
    i16* samples,
    i32 samples_count_per_second,
    i32 samples_count_per_channel,
    i8 channels,
    f32 frequency,
    f32 last_angle)
{
    assert(samples_count_per_second > 0);
    assert(samples_count_per_channel > 0);
    assert(channels > 0);
    assert(frequency > 0);
    assert(last_angle >= 0);

#if 0
    const i16 volume = 6400;
#else
    const i16 volume = 0;
#endif

    f32 samples_per_oscillation = (f32)samples_count_per_second / frequency;
    f32 angle_step = BF_2PI / samples_per_oscillation;

    for (int i = 0; i < samples_count_per_channel; i++) {
        last_angle += angle_step;

        // TODO(hulvdan): Implement our own sin function
        for (int k = 0; k < channels; k++) {
            auto val = volume * sinf(last_angle * (f32)(k + 1));
            samples[i * channels + k] = (i16)val;
        }
    }

    while (last_angle > BF_2PI)
        last_angle -= BF_2PI;

    return last_angle;
}

struct CreateBufferRes {
    XAUDIO2_BUFFER* buffer;
    u8* samples;
};

CreateBufferRes CreateBuffer(i32 samples_per_channel, i32 channels, i32 bytes_per_sample)
{
    assert(channels > 0);
    assert(bytes_per_sample > 0);

    auto b = new XAUDIO2_BUFFER();
    auto& buffer = *b;

    i64 total_bytes = (i64)samples_per_channel * channels * bytes_per_sample;
    auto samples = new u8[total_bytes]();

    buffer.Flags = XAUDIO2_END_OF_STREAM;
    buffer.AudioBytes = total_bytes;
    buffer.pAudioData = samples;
    buffer.PlayBegin = 0;
    buffer.PlayLength = samples_per_channel;
    buffer.LoopBegin = 0;
    buffer.LoopLength = 0;
    buffer.LoopCount = 0;
    buffer.pContext = nullptr;

    return {b, samples};
}
// -- XAUDIO STUFF END

global_variable bool running = false;

global_variable bool should_recreate_bitmap_after_client_area_resize;
global_variable BFBitmap screen_bitmap;

global_variable int client_width = -1;
global_variable int client_height = -1;

void Win32UpdateBitmap(HDC device_context)
{
    assert(client_width >= 0);
    assert(client_height >= 0);

    auto& game_bitmap = screen_bitmap.bitmap;
    game_bitmap.width = client_width;
    game_bitmap.height = client_height;

    game_bitmap.bits_per_pixel = 32;
    screen_bitmap.info.bmiHeader.biSize = sizeof(screen_bitmap.info.bmiHeader);
    screen_bitmap.info.bmiHeader.biWidth = client_width;
    screen_bitmap.info.bmiHeader.biHeight = -client_height;
    screen_bitmap.info.bmiHeader.biPlanes = 1;
    screen_bitmap.info.bmiHeader.biBitCount = game_bitmap.bits_per_pixel;
    screen_bitmap.info.bmiHeader.biCompression = BI_RGB;

    if (game_bitmap.memory)
        VirtualFree(game_bitmap.memory, 0, MEM_RELEASE);

    game_bitmap.memory = VirtualAlloc(
        0, game_bitmap.width * screen_bitmap.bitmap.height * game_bitmap.bits_per_pixel / 8,
        MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);

    if (screen_bitmap.handle)
        DeleteObject(screen_bitmap.handle);

    screen_bitmap.handle = CreateDIBitmap(
        device_context, &screen_bitmap.info.bmiHeader, 0, game_bitmap.memory, &screen_bitmap.info,
        DIB_RGB_COLORS);
}

void* human_sprite = nullptr;
i32 human_sprite_width = 0;
i32 human_sprite_height = 0;

void DrawSprite(
    GLuint texture_name,
    glm::vec2 pos,
    glm::vec2 size,
    float rotation,
    const glm::mat3& projection)
{
    auto model = glm::mat3(1);
    model = glm::translate(model, pos);
    model = glm::scale(model, size / 2.0f);
    model = glm::rotate(model, rotation);

    auto matrix = projection * model;
    // TODO(hulvdan): How bad is it that there are vec3, but not vec2?
    glm::vec3 vertices[] = {{-1, -1, 1}, {-1, 1, 1}, {1, -1, 1}, {1, 1, 1}};
    for (auto& vertex : vertices)
        vertex = matrix * vertex;

    glm::ivec2 texture_vertices[] = {{0, 0}, {0, 1}, {1, 0}, {1, 1}};
    for (auto i : {0, 1, 2, 2, 1, 3}) {
        // TODO(hulvdan): How bad is that there are 2 vertices duplicated?
        glTexCoord2f(texture_vertices[i].x, texture_vertices[i].y);
        glVertex2f(vertices[i].x, vertices[i].y);
    }
};

void Win32BlitBitmapToTheWindow(HDC device_context)
{
    glClear(GL_COLOR_BUFFER_BIT);
    assert(!glGetError());

    GLuint texture_name = 1;
    // glGenTextures(1, &texture_name);
    glBindTexture(GL_TEXTURE_2D, texture_name);
    assert(!glGetError());

    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    assert(!glGetError());

    glTexImage2D(
        GL_TEXTURE_2D, 0, GL_RGBA8, screen_bitmap.bitmap.width, screen_bitmap.bitmap.height, 0,
        GL_BGRA_EXT, GL_UNSIGNED_BYTE, screen_bitmap.bitmap.memory);
    assert(!glGetError());

    GLuint human_texture = 2;
    if (human_sprite) {
        glBindTexture(GL_TEXTURE_2D, human_texture);
        assert(!glGetError());

        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
        assert(!glGetError());

        glTexImage2D(
            GL_TEXTURE_2D, 0, GL_RGBA8, human_sprite_width, human_sprite_height, 0, GL_BGRA_EXT,
            GL_UNSIGNED_BYTE, human_sprite);
    }

    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    assert(!glGetError());

    glEnable(GL_TEXTURE_2D);
    assert(!glGetError());
    {
        glBlendFunc(GL_ONE, GL_ZERO);
        glBindTexture(GL_TEXTURE_2D, texture_name);
        glBegin(GL_TRIANGLES);

        glm::ivec2 vertices[] = {{0, 0}, {0, 1}, {1, 0}, {1, 1}};
        for (auto i : {0, 1, 2, 2, 1, 3}) {
            auto& v = vertices[i];
            glTexCoord2f(v.x, v.y);
            glVertex2f(v.x, v.y);
        }

        glEnd();
    }

    if (human_sprite) {
        glBindTexture(GL_TEXTURE_2D, human_texture);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glBegin(GL_TRIANGLES);

        auto angle = BF_PI / 8;

        auto sprite_pos = glm::vec2(200, 200);
        auto sprite_size = glm::vec2(64, 64);

        auto swidth = (f32)screen_bitmap.bitmap.width;
        auto sheight = (f32)screen_bitmap.bitmap.height;

        auto projection = glm::mat3(1);
        projection = glm::translate(projection, glm::vec2(0, 1));
        projection = glm::scale(projection, glm::vec2(1 / swidth, -1 / sheight));

        DrawSprite(human_texture, sprite_pos, sprite_size, angle, projection);

        glEnd();
    }

    glDisable(GL_TEXTURE_2D);
    assert(!glGetError());

    SwapBuffers(device_context);
    assert(!glGetError());

    glDeleteTextures(1, (GLuint*)&texture_name);
    if (human_sprite)
        glDeleteTextures(1, (GLuint*)&human_texture);
    assert(!glGetError());
}

void Win32Paint(f32 dt, HWND window_handle, HDC device_context)
{
    if (should_recreate_bitmap_after_client_area_resize)
        Win32UpdateBitmap(device_context);

    Game_UpdateAndRender_(
        dt, game_memory, screen_bitmap.bitmap, (void*)events.data(), events_count);
    Win32BlitBitmapToTheWindow(device_context);

    events_count = 0;
    events.clear();

#if BFG_INTERNAL
    LoadOrUpdateGameDll();
#endif
}

void Win32GLResize()
{
    glViewport(0, 0, client_width, client_height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, 1, 1, 0, -1, 1);
}

LRESULT WindowEventsHandler(HWND window_handle, UINT messageType, WPARAM wParam, LPARAM lParam)
{
    switch (messageType) {
    case WM_CLOSE: {  // NOLINT(bugprone-branch-clone)
        running = false;
    } break;

    case WM_DESTROY: {
        // TODO(hulvdan): It was an error. Should we try to recreate the window?
        running = false;
    } break;

    case WM_SIZE: {
        client_width = LOWORD(lParam);
        client_height = HIWORD(lParam);
        should_recreate_bitmap_after_client_area_resize = true;
        Win32GLResize();
    } break;

    case WM_KEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP: {
        bool previous_was_down = lParam & (1 << 30);
        bool is_up = lParam & (1 << 31);
        bool alt_is_down = lParam & (1 << 29);
        u32 vk_code = wParam;

        if (is_up != previous_was_down)
            return 0;

        // if (vk_code == 'W') {
        // } else if (vk_code == 'A') {
        // } else if (vk_code == 'S') {
        // } else if (vk_code == 'D') {
        // } else
        if (vk_code == VK_ESCAPE  //
            || vk_code == 'Q'  //
            || vk_code == VK_F4 && alt_is_down) {
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

class BFVoiceCallback : public IXAudio2VoiceCallback
{
public:
    f32 frequency = -1;
    i32 samples_count_per_channel = -1;
    i8 channels = -1;

    XAUDIO2_BUFFER* b1 = nullptr;
    XAUDIO2_BUFFER* b2 = nullptr;
    u8* s1 = nullptr;
    u8* s2 = nullptr;

    IXAudio2SourceVoice* voice = nullptr;

    void RecalculateAndSwap()
    {
        Validate();

        auto& samples = s1;
        auto& buffer = b1;

        last_angle = FillSamples(
            (i16*)samples, SAMPLES_HZ, samples_count_per_channel, channels, frequency, last_angle);

        auto res = voice->SubmitSourceBuffer(buffer);
        if (FAILED(res)) {
            // TODO(hulvdan): Diagnostic
        }

        std::swap(b1, b2);
        std::swap(s1, s2);
    }

    void OnStreamEnd() noexcept { RecalculateAndSwap(); }

    void OnBufferEnd(void* pBufferContext) noexcept {}
    void OnBufferStart(void* pBufferContext) noexcept {}
    void OnLoopEnd(void* pBufferContext) noexcept {}
    void OnVoiceError(void* pBufferContext, HRESULT Error) noexcept {}
    void OnVoiceProcessingPassEnd() noexcept {}
    void OnVoiceProcessingPassStart(UINT32 BytesRequired) noexcept {}

    void Validate()
    {
        assert(samples_count_per_channel > 0);
        assert(channels > 0);
        assert(last_angle >= 0);

        assert(b1 != nullptr);
        assert(b2 != nullptr);
        assert(s1 != nullptr);
        assert(s2 != nullptr);
        assert(voice != nullptr);
    }

private:
    f32 last_angle = 0;
};

u64 Win32Clock()
{
    LARGE_INTEGER res;
    QueryPerformanceCounter(&res);
    return res.QuadPart;
}

u64 Win32Frequency()
{
    LARGE_INTEGER res;
    QueryPerformanceFrequency(&res);
    return res.QuadPart;
}

struct LoadBMPFile_RGBA_Result {
    bool success;
    u16 width;
    u16 height;
};

LoadBMPFile_RGBA_Result LoadBMPFile_RGBA(const char* filename, u8* output, size_t output_max_bytes)
{
    LoadBMPFile_RGBA_Result res = {};

    auto file = fopen(filename, "r");
    assert(file);

    // Reading the header
    // https://en.wikipedia.org/wiki/BMP_file_format#Bitmap_file_header
    const auto BMP_HEADER_SIZE = 12;

    // NOTE(hulvdan): Adding 6 to get the size of the DIB header
    const auto offset = 6;
    const auto header_bytes_to_read = BMP_HEADER_SIZE + offset;
    u8 header[header_bytes_to_read];

    auto read_bytes = fread(header, 1, header_bytes_to_read, file);
    if (read_bytes != header_bytes_to_read) {
        // TODO(hulvdan): Diagnostic
        assert(fclose(file) != 0);
        return res;
    }

    auto is_bmp = header[0] == 'B' && header[1] == 'M';
    if (!is_bmp) {
        // TODO(hulvdan): Diagnostic. Not a BMP file
        assert(false);
        return res;
    }

    auto file_size_from_header = *(u32*)(header + 2);
    auto pixel_array_starting_address = *(u32*)(header + 10);

    const auto DIB_HEADER_MAX_SIZE = 124;
    u8 dib_header[DIB_HEADER_MAX_SIZE];
    // NOTE(hulvdan): Copying size of dib_header from the end of header
    memcpy(dib_header, header + BMP_HEADER_SIZE, offset);

    auto dib_header_size = *(u32*)(header + 14);
    if (dib_header_size == DIB_HEADER_MAX_SIZE || dib_header_size == 56) {
        auto dib_header_size2 = *(u32*)(dib_header + 2);
        assert(dib_header_size == dib_header_size2);

        const auto DIB_HEADER_OFFSET = 12;
        const auto bytes_to_read = dib_header_size - offset;
        read_bytes = fread(dib_header + offset, 1, bytes_to_read, file);
        assert(read_bytes == bytes_to_read);

        const auto dib_header_shifted = dib_header - DIB_HEADER_OFFSET;

        res.width = *(u16*)(dib_header_shifted + 18);
        res.height = *(u16*)(dib_header_shifted + 22);
        if ((size_t)res.width * res.height * 4 > output_max_bytes) {
            // TODO(hulvdan): Diagnostic
            assert(false);
            return res;
        }

        auto number_of_color_planes = *(u16*)(dib_header_shifted + 26);
        assert(number_of_color_planes == 1);

        auto bits_per_pixel = *(u16*)(dib_header_shifted + 28);
        auto compression_method = *(u32*)(dib_header_shifted + 30);

        // TODO(hulvdan): Even though it's Huffman's compression,
        // why weren't color palette used?
        assert(compression_method == 3);  // 3 => Huffman 1D compression

        // NOTE(hulvdan): palette_colors_number = 2**n, if it's == 0
        // (where `n` is the number of bits per pixel)
        auto palette_colors_number = *(u32*)(dib_header_shifted + 46);
        auto important_colors_number = *(u32*)(dib_header_shifted + 50);

        assert(bits_per_pixel == 32);
        auto pixels_count = (u32)res.width * res.height;
        assert(file_size_from_header - pixel_array_starting_address == pixels_count * 4);

        fseek(file, (long)pixel_array_starting_address, SEEK_SET);
        // read_bytes = fread(dib_header + offset, 1, bytes_to_read, file);
        auto read_pixels = fread((void*)output, 4, pixels_count, file);
        assert(read_pixels == pixels_count);
    } else {
        // TODO(hulvdan): Is not yet implemented algorithm
        assert(false);
    }

    assert(fclose(file) == 0);
    res.success = true;

    return res;
}

static int WinMain(
    HINSTANCE application_handle,
    HINSTANCE previous_window_instance_handle,
    LPSTR command_line,
    int show_command)
{
    WNDCLASSA windowClass = {};
    LoadOrUpdateGameDll();
    LoadXInputDll();

    events.reserve(Kilobytes(64LL));

    game_memory =
        VirtualAlloc(0, Megabytes(64LL), MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    if (!game_memory) {
        // TODO(hulvdan): Diagnostic
        return -1;
    }

    const i32 SLEEP_MSEC_GRANULARITY = 1;
    timeBeginPeriod(SLEEP_MSEC_GRANULARITY);

    // --- XAudio stuff ---
    LoadXAudioDll();

    IXAudio2* xaudio = nullptr;
    IXAudio2MasteringVoice* master_voice = nullptr;
    IXAudio2SourceVoice* source_voice = nullptr;

    const u32 duration_msec = 20;
    const u32 bytes_per_sample = 2;
    const u32 channels = 2;

    const f32 starting_frequency = 523.25f / 2;

    BFVoiceCallback voice_callback = {};
    voice_callback.frequency = starting_frequency;
    voice_callback.samples_count_per_channel = -1;
    voice_callback.channels = channels;

    XAUDIO2_BUFFER* buffer1 = nullptr;
    XAUDIO2_BUFFER* buffer2 = nullptr;
    u8* samples1 = nullptr;
    u8* samples2 = nullptr;

    // TODO(hulvdan): Am I supposed to dynamically load xaudio2.dll?
    // What about targeting different versions of xaudio on different OS?
    if (SUCCEEDED(CoInitializeEx(nullptr, COINIT_MULTITHREADED))) {
        if (SUCCEEDED(XAudio2Create_(&xaudio, 0, XAUDIO2_DEFAULT_PROCESSOR))) {
            if (SUCCEEDED(xaudio->CreateMasteringVoice(&master_voice))) {
                WAVEFORMATEX voice_struct = {};
                voice_struct.wFormatTag = WAVE_FORMAT_PCM;
                voice_struct.nChannels = channels;
                voice_struct.nSamplesPerSec = SAMPLES_HZ;
                voice_struct.nAvgBytesPerSec = channels * SAMPLES_HZ * bytes_per_sample;
                voice_struct.nBlockAlign = channels * bytes_per_sample;
                voice_struct.wBitsPerSample = bytes_per_sample * 8;
                voice_struct.cbSize = 0;

                auto res = xaudio->CreateSourceVoice(
                    &source_voice, &voice_struct, 0,
                    // TODO(hulvdan): Revise max frequency ratio
                    // https://learn.microsoft.com/en-us/windows/win32/api/xaudio2/nf-xaudio2-ixaudio2-createsourcevoice
                    XAUDIO2_DEFAULT_FREQ_RATIO, &voice_callback, nullptr, nullptr);

                if (SUCCEEDED(res)) {
                    assert((SAMPLES_HZ * duration_msec) % 1000 == 0);

                    i32 samples_count_per_channel = SAMPLES_HZ * duration_msec / 1000;
                    assert(samples_count_per_channel > 0);

                    auto r1 = CreateBuffer(samples_count_per_channel, channels, bytes_per_sample);
                    auto r2 = CreateBuffer(samples_count_per_channel, channels, bytes_per_sample);

                    buffer1 = r1.buffer;
                    samples1 = r1.samples;
                    buffer2 = r2.buffer;
                    samples2 = r2.samples;

                    voice_callback.b1 = buffer1;
                    voice_callback.b2 = buffer2;
                    voice_callback.s1 = samples1;
                    voice_callback.s2 = samples2;
                    voice_callback.voice = source_voice;
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
                    //     // TODO(hulvdan): Diagnostic
                    // }
                    //
                } else {
                    // TODO(hulvdan): Diagnostic
                    assert(!source_voice);
                }
            } else {
                // TODO(hulvdan): Diagnostic
                assert(!master_voice);
            }
        } else {
            // TODO(hulvdan): Diagnostic
            assert(!xaudio);
        }
    }
    // --- XAudio stuff end ---

    // NOTE(hulvdan): Casey says that OWNDC is what makes us able
    // not to ask the OS for a new DC each time we need to draw if I understood correctly.
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    // windowClass.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    windowClass.lpfnWndProc = *WindowEventsHandler;
    windowClass.lpszClassName = "BFGWindowClass";
    windowClass.hInstance = application_handle;

    // TODO(hulvdan): Icon!
    // HICON     hIcon;

    if (RegisterClassA(&windowClass) == NULL) {
        // TODO(hulvdan): Diagnostic
        return 0;
    }

    if (source_voice != nullptr) {
        auto res = source_voice->Start(0);
        // TODO(hulvdan): Diagnostic
        assert(SUCCEEDED(res));
    }

    const auto bmp_size = 64 * 4;
    u8 bmp_bytes[bmp_size] = {};
    auto loading_result = LoadBMPFile_RGBA(
        R"PATH(c:\Users\user\dev\home\handmade-cpp-game\assets\art\sprites\human.bmp)PATH",
        bmp_bytes, bmp_size);

    if (loading_result.success) {
        human_sprite = bmp_bytes;
        human_sprite_width = loading_result.width;
        human_sprite_height = loading_result.height;
    }

    auto window_handle = CreateWindowExA(
        0, windowClass.lpszClassName, "The Big Fuken Game", WS_TILEDWINDOW, CW_USEDEFAULT,
        CW_USEDEFAULT, 640, 480,
        NULL,  // [in, optional] HWND      hWndParent,
        NULL,  // [in, optional] HMENU     hMenu,
        application_handle,  // [in, optional] HINSTANCE hInstance,
        NULL  // [in, optional] LPVOID    lpParam
    );

    if (!window_handle) {
        // TODO(hulvdan): Diagnostic
        return FALSE;
    }

    ShowWindow(window_handle, show_command);
    UpdateWindow(window_handle);

    assert(client_width >= 0);
    assert(client_height >= 0);
    // --- Initializing OpenGL Start ---
    {
        auto hdc = GetDC(window_handle);

        // --- Setting up pixel format start ---
        PIXELFORMATDESCRIPTOR pfd = {};
        pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
        pfd.nVersion = 1;
        pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
        pfd.dwLayerMask = PFD_MAIN_PLANE;
        pfd.iPixelType = PFD_TYPE_RGBA;
        pfd.cColorBits = 24;
        pfd.cAlphaBits = 8;
        // pfd.cDepthBits = 0;
        // pfd.cAccumBits = 0;
        // pfd.cStencilBits = 0;

        auto pixelformat = ChoosePixelFormat(hdc, &pfd);
        if (!pixelformat) {
            // TODO(hulvdan): Diagnostic
            return FALSE;
        }

        DescribePixelFormat(hdc, pixelformat, pfd.nSize, &pfd);

        if (SetPixelFormat(hdc, pixelformat, &pfd) == FALSE) {
            // TODO(hulvdan): Diagnostic
            return FALSE;
        }
        // --- Setting up pixel format end ---

        auto ghRC = wglCreateContext(hdc);
        wglMakeCurrent(hdc, ghRC);

        if (glewInit() != GLEW_OK) {
            // TODO(hulvdan): Diagnostic
            // fprintf(stderr, "Error: %s\n", glewGetErrorString(err));
            assert(false);
        }

        // NOTE(hulvdan): Enabling VSync
        // https://registry.khronos.org/OpenGL/extensions/EXT/WGL_EXT_swap_control.txt
        // https://registry.khronos.org/OpenGL/extensions/EXT/WGL_EXT_swap_control_tear.txt
        if (WGLEW_EXT_swap_control_tear)
            wglSwapIntervalEXT(-1);
        else if (WGLEW_EXT_swap_control)
            wglSwapIntervalEXT(1);

        glEnable(GL_BLEND);
        glClearColor(1, 0, 1, 1);
        assert(!glGetError());

        glShadeModel(GL_SMOOTH);
        assert(!glGetError());

        ReleaseDC(window_handle, hdc);
        Win32GLResize();
    }
    // --- Initializing OpenGL End ---

    screen_bitmap = BFBitmap();

    running = true;

    u64 perf_counter_current = Win32Clock();
    u64 perf_counter_frequency = Win32Frequency();

    f32 last_frame_dt = 0;
    const f32 MAX_FRAME_DT = 1.0f / 10.0f;
    // TODO(hulvdan): Use DirectX / OpenGL to calculate refresh_rate and rework this whole mess
    f32 REFRESH_RATE = 60.0f;
    i64 frames_before_flip = (i64)((f32)(perf_counter_frequency) / REFRESH_RATE);

    while (running) {
        u64 next_frame_expected_perf_counter = perf_counter_current + frames_before_flip;

        MSG message = {};
        while (PeekMessageA(&message, NULL, 0, 0, PM_REMOVE) != 0) {
            if (message.message == WM_QUIT) {
                running = false;
                break;
            }

            TranslateMessage(&message);
            DispatchMessage(&message);
        }

        if (!running)
            break;

        // CONTROLLER STUFF
        // TODO(hulvdan): Improve on latency?
        for (DWORD i = 0; i < XUSER_MAX_COUNT; i++) {
            XINPUT_STATE state = {};

            DWORD dwResult = XInputGetState_(i, &state);

            if (dwResult == ERROR_SUCCESS) {
                // Controller is connected
                const f32 scale = 32768;
                f32 stick_x_normalized = (f32)state.Gamepad.sThumbLX / scale;
                f32 stick_y_normalized = (f32)state.Gamepad.sThumbLY / scale;

                ControllerAxisChanged event = {};
                event.axis = 0;
                event.value = stick_x_normalized;
                push_event<ControllerAxisChanged>(event);

                event.axis = 1;
                event.value = stick_y_normalized;
                push_event<ControllerAxisChanged>(event);

                voice_callback.frequency = starting_frequency * powf(2, stick_y_normalized);
            } else {
                // TODO(hulvdan): Handling disconnects
            }
        }
        // CONTROLLER STUFF END

        auto device_context = GetDC(window_handle);

        auto capped_dt = last_frame_dt > MAX_FRAME_DT ? MAX_FRAME_DT : last_frame_dt;
        Win32Paint(capped_dt, window_handle, device_context);
        ReleaseDC(window_handle, device_context);

        u64 perf_counter_new = Win32Clock();
        last_frame_dt =
            (f32)(perf_counter_new - perf_counter_current) / (f32)perf_counter_frequency;
        assert(last_frame_dt >= 0);

        if (perf_counter_new < next_frame_expected_perf_counter) {
            while (perf_counter_new < next_frame_expected_perf_counter) {
                i32 msec_to_sleep =
                    (i32)((f32)(next_frame_expected_perf_counter - perf_counter_new) * 1000.0f /
                          (f32)perf_counter_frequency);
                assert(msec_to_sleep >= 0);

                if (msec_to_sleep >= 2 * SLEEP_MSEC_GRANULARITY) {
                    Sleep(msec_to_sleep - SLEEP_MSEC_GRANULARITY);
                }

                perf_counter_new = Win32Clock();
            }
        } else {
            // TODO(hulvdan): There go your frameskips...
        }

        last_frame_dt =
            (f32)(perf_counter_new - perf_counter_current) / (f32)perf_counter_frequency;
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
    // TODO(hulvdan): How am I supposed to release it?
    // source_voice
    //
    // if (master_voice != nullptr) {
    //     // TODO(hulvdan): How am I supposed to release it?
    //     //
    //     // https://learn.microsoft.com/en-us/windows/win32/xaudio2/how-to--initialize-xaudio2
    //     // > Ensure that all XAUDIO2 child objects are fully released
    //     // > before you release the IXAudio2 object.
    //
    //     // master_voice->Release();
    // }

    if (xaudio)
        xaudio->Release();

    return 0;
}
