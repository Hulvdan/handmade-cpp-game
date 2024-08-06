#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_win32.h"
#ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#endif

#include "windows.h"
#include "glew.h"
#include "wglew.h"
#include "timeapi.h"

// #include <cassert>
#include <cmath>
#include <iostream>
#include <vector>

#include "bf_game.cpp"

// -- RENDERING STUFF
struct BF_Bitmap {
    Game_Bitmap bitmap;

    HBITMAP    handle;
    BITMAPINFO info;
};
// -- RENDERING STUFF END

// -- GAME STUFF
global_var HMODULE game_lib = nullptr;
global_var size_t  game_memory_size;
global_var void*   game_memory = nullptr;

global_var size_t events_count    = 0;
global_var std::vector<u8> events = {};

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
        0,
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

void Win32Paint(f32 dt, HWND window_handle, HDC device_context) {
    if (should_recreate_bitmap_after_client_area_resize)
        Win32UpdateBitmap(device_context);

    SwapBuffers(device_context);
    Check_OpenGL_Errors();

    events_count = 0;
    events.clear();
}

void Win32GLResize() {
    glViewport(0, 0, client_width, client_height);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, 1, 1, 0, -1, 1);
}

extern IMGUI_IMPL_API LRESULT
ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

LRESULT
WindowEventsHandler(HWND window_handle, UINT messageType, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(window_handle, messageType, wParam, lParam))
        return true;

    switch (messageType) {
    case WM_CLOSE: {  // NOLINT(bugprone-branch-clone)
        running = false;
    } break;

    case WM_DESTROY: {
        // TODO: It was an error. Should we try to recreate the window?
        running = false;
    } break;

    case WM_SIZE: {
        client_width                                    = LOWORD(lParam);
        client_height                                   = HIWORD(lParam);
        should_recreate_bitmap_after_client_area_resize = true;
        Win32GLResize();
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

void Update_GUI(Arena& arena, Loaded_Texture& tex) {
    local_persist int octaves      = -1;
    local_persist f32 scaling_bias = 2;
    local_persist int seed         = 0;

    auto texture_display_size = 256;

    auto new_total = octaves;
    bool regen     = false;

    if (ImGui::SliderInt("Octaves Count", &new_total, 1, 9, "", 0))
        regen = true;

    if (ImGui::Button("New Seed")) {
        seed  = Win32Clock();
        regen = true;
    }

    if (ImGui::SliderFloat("Scaling Bias", &scaling_bias, 0.01f, 4.0f, "", 0))
        regen = true;

    new_total = MIN(9, new_total);
    new_total = MAX(1, new_total);
    if (new_total != octaves) {
        octaves = new_total;
        regen   = true;
    }

    if (regen) {
        // Perlin(tex, arena.base + arena.used, arena.size - arena.used, octaves,
        // scaling_bias, seed);
        Perlin_2D(
            tex,
            arena.base + arena.used,
            arena.size - arena.used,
            octaves,
            scaling_bias,
            seed
        );

        glBindTexture(GL_TEXTURE_2D, tex.id);
        Check_OpenGL_Errors();

        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
        Check_OpenGL_Errors();

        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            GL_RGBA8,
            tex.size.x,
            tex.size.y,
            0,
            GL_BGRA_EXT,
            GL_UNSIGNED_BYTE,
            tex.base
        );
    }

    ImGui::Image(
        (ImTextureID)tex.id,
        ImVec2(texture_display_size, texture_display_size),
        {0, 0},
        {1, 1},
        ImVec4(1.0f, 1.0f, 1.0f, 1.0f),
        ImGui::GetStyleColorVec4(ImGuiCol_Border)
    );
}

// NOLINTBEGIN(clang-analyzer-core.StackAddressEscape)
int main(int, char**) {
    auto application_handle = GetModuleHandle(nullptr);

    events.reserve(Kilobytes(64LL));

    game_memory_size = Megabytes(64LL);
    game_memory      = VirtualAlloc(
        0, game_memory_size, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE
    );
    if (!game_memory) {
        // TODO: Diagnostic
        return -1;
    }

    const i32 SLEEP_MSEC_GRANULARITY = 1;
    timeBeginPeriod(SLEEP_MSEC_GRANULARITY);

    WNDCLASSA windowClass = {};
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
        return 0;
    }

    Arena& arena = *(Arena*)game_memory;
    arena.size   = game_memory_size - sizeof(Arena);
    arena.used   = 0;
    arena.base   = (u8*)(game_memory) + sizeof(Arena);

    auto window_handle = CreateWindowExA(
        0,
        windowClass.lpszClassName,
        "The Big Fuken Game",
        WS_TILEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        640,
        480,
        NULL,                // [in, optional] HWND      hWndParent,
        NULL,                // [in, optional] HMENU     hMenu,
        application_handle,  // [in, optional] HINSTANCE hInstance,
        NULL                 // [in, optional] LPVOID    lpParam
    );

    if (!window_handle) {
        // TODO: Diagnostic
        return -1;
    }

    ShowWindow(window_handle, SW_SHOWDEFAULT);
    UpdateWindow(window_handle);

    Loaded_Texture tex;
    tex.id   = 1771;
    tex.size = {512, 512};
    tex.base = (u8*)Allocate_Array(arena, u32, tex.size.x * tex.size.y);

    Assert(client_width >= 0);
    Assert(client_height >= 0);
    // --- Initializing OpenGL Start ---
    {
        auto hdc = GetDC(window_handle);

        // --- Setting up pixel format start ---
        PIXELFORMATDESCRIPTOR pfd = {};
        pfd.nSize                 = sizeof(PIXELFORMATDESCRIPTOR);
        pfd.nVersion              = 1;
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

        if (glewInit() != GLEW_OK) {
            // TODO: Diagnostic
            // fprintf(stderr, "Error: %s\n", glewGetErrorString(err));
            INVALID_PATH;
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
        Check_OpenGL_Errors();

        glShadeModel(GL_SMOOTH);
        Check_OpenGL_Errors();

        ReleaseDC(window_handle, hdc);
        Win32GLResize();
    }
    // --- Initializing OpenGL End ---

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;  // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;   // Enable Gamepad Controls

    ImGui::StyleColorsDark();

    // Setup Platform/Renderer backends
    ImGui_ImplWin32_InitForOpenGL(window_handle);
    ImGui_ImplOpenGL3_Init();

    // Our state
    screen_bitmap = BF_Bitmap();

    running = true;

    u64 perf_counter_current   = Win32Clock();
    u64 perf_counter_frequency = Win32Frequency();

    f32       last_frame_dt = 0;
    const f32 MAX_FRAME_DT  = 1.0f / 10.0f;
    // TODO: Use DirectX / OpenGL to calculate refresh_rate and rework this whole
    // mess
    f32 REFRESH_RATE       = 60.0f;
    i64 frames_before_flip = (i64)((f32)(perf_counter_frequency) / REFRESH_RATE);

    glEnable(GL_TEXTURE_2D);

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

        auto device_context = GetDC(window_handle);
        auto capped_dt      = MIN(last_frame_dt, MAX_FRAME_DT);

        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();

        Update_GUI(arena, tex);

        // Rendering
        ImGui::Render();
        // glViewport(0, 0, g_Width, g_Height);
        // glClearColor(clear_color.x, clear_color.y, clear_color.z, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        // Present
        // ::SwapBuffers(device_context);

        Win32Paint(capped_dt, window_handle, device_context);
        ReleaseDC(window_handle, device_context);

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

                if (msec_to_sleep >= 2 * SLEEP_MSEC_GRANULARITY)
                    Sleep(msec_to_sleep - SLEEP_MSEC_GRANULARITY);

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

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();

    return 0;
}
// NOLINTEND(clang-analyzer-core.StackAddressEscape)
