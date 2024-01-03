#include <cassert>
#include <iostream>
#include "windows.h"
#include "xinput.h"

#define local_persist static
#define global_variable static
#define internal static

struct BFBitmap {
    HBITMAP handle;
    BITMAPINFO info;
    int bits_per_pixel;
    int width;
    int height;
    void* memory;
};

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
    auto library = LoadLibrary("xinput1_4.dll");
    if (!library)
        library = LoadLibrary("xinput1_3.dll");

    if (!library) {
        XInputGetState_ = (XInputGetStateType)GetProcAddress(library, "XInputGetState");
        XInputSetState_ = (XInputSetStateType)GetProcAddress(library, "XInputSetState");
        controller_support_loaded = true;
    }
}
// -- CONTROLLER STUFF END

global_variable bool running = false;

global_variable bool should_recreate_bitmap_after_client_area_resize;
global_variable BFBitmap screen_bitmap;

global_variable int client_width;
global_variable int client_height;

global_variable float Goffset_x = 0;
global_variable float Goffset_y = 0;

void Win32UpdateBitmap(HDC device_context)
{
    assert(client_width >= 0);
    assert(client_height >= 0);

    screen_bitmap.width = client_width;
    screen_bitmap.height = client_height;

    screen_bitmap.bits_per_pixel = 32;
    screen_bitmap.info.bmiHeader.biSize = sizeof(screen_bitmap.info.bmiHeader);
    screen_bitmap.info.bmiHeader.biWidth = client_width;
    screen_bitmap.info.bmiHeader.biHeight = -client_height;
    screen_bitmap.info.bmiHeader.biPlanes = 1;
    screen_bitmap.info.bmiHeader.biBitCount = screen_bitmap.bits_per_pixel;
    screen_bitmap.info.bmiHeader.biCompression = BI_RGB;

    if (screen_bitmap.memory) {
        VirtualFree(screen_bitmap.memory, 0, MEM_RELEASE);
    }

    screen_bitmap.memory = VirtualAlloc(
        0, screen_bitmap.width * screen_bitmap.height * screen_bitmap.bits_per_pixel / 8,
        MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);

    if (screen_bitmap.handle) {
        DeleteObject(screen_bitmap.handle);
    }

    screen_bitmap.handle = CreateDIBitmap(
        device_context, &screen_bitmap.info.bmiHeader, 0, screen_bitmap.memory, &screen_bitmap.info,
        DIB_RGB_COLORS);
}

void Win32RenderWeirdGradient(int offset_x, int offset_y)
{
    auto pixel = (uint32_t*)screen_bitmap.memory;

    for (int y = 0; y < screen_bitmap.height; y++) {
        for (int x = 0; x < screen_bitmap.width; x++) {
            // uint32_t red  = 0;
            uint32_t red = (uint8_t)(x + offset_x);
            // uint32_t red  = (uint8_t)(y + offset_y);
            uint32_t green = 0;
            // uint32_t green = (uint8_t)(x + offset_x);
            // uint32_t green = (uint8_t)(y + offset_y);
            // uint32_t blue = 0;
            // uint32_t blue = (uint8_t)(x + offset_x);
            uint32_t blue = (uint8_t)(y + offset_y);
            (*pixel++) = (blue) | (green << 8) | (red << 16);
        }
    }
}

void Win32BlitBitmapToTheWindow(HDC device_context)
{
    StretchDIBits(
        device_context,  //
        0, 0, client_width, client_height,  //
        0, 0, screen_bitmap.width, screen_bitmap.height,  //
        screen_bitmap.memory, &screen_bitmap.info, DIB_RGB_COLORS, SRCCOPY);
}

void Win32Paint(HWND window_handle, HDC device_context)
{
    if (should_recreate_bitmap_after_client_area_resize) {
        Win32UpdateBitmap(device_context);
    }

    Win32RenderWeirdGradient(Goffset_x, Goffset_y);
    Win32BlitBitmapToTheWindow(device_context);
}

LRESULT WindowEventsHandler(HWND window_handle, UINT messageType, WPARAM wParam, LPARAM lParam)
{
    switch (messageType) {
    case WM_CLOSE: {
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
    } break;

    case WM_PAINT: {
        assert(client_width != 0);
        assert(client_height != 0);

        PAINTSTRUCT paint_struct;
        auto device_context = BeginPaint(window_handle, &paint_struct);

        Win32Paint(window_handle, device_context);
        EndPaint(window_handle, &paint_struct);
    } break;

    case WM_KEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP: {
        bool previous_was_down = lParam & (1 << 30);
        bool is_up = lParam & (1 << 31);
        uint32_t vk_code = wParam;

        if (is_up != previous_was_down) {
            return 0;
        }

        bool alt_is_down = (lParam & (1 << 29)) != 0;

        if (vk_code == 'W') {
        } else if (vk_code == 'A') {
        } else if (vk_code == 'S') {
        } else if (vk_code == 'D') {
        } else if (
            vk_code == VK_ESCAPE  //
            || vk_code == 'Q'  //
            || vk_code == VK_F4 && alt_is_down) {
            running = false;
        }

        if (is_up) {
            // OnKeyReleased
        } else {
            // OnKeyPressed
        }
    } break;

    default: {
        return DefWindowProc(window_handle, messageType, wParam, lParam);
    }
    }
    return 0;
}

int WinMain(
    HINSTANCE application_handle,
    HINSTANCE previous_window_instance_handle,
    LPSTR command_line,
    int show_command)
{
    WNDCLASSA windowClass = {};
    LoadXInputDll();

    // NOTE(hulvdan): Casey says that OWNDC is what makes us able
    // not to ask the OS for a new DC each time we need to draw if I understood correctly.
    windowClass.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    windowClass.lpfnWndProc = *WindowEventsHandler;
    windowClass.lpszClassName = "BFGWindowClass";
    windowClass.hInstance = application_handle;

    // TODO(hulvdan): Icon!
    // HICON     hIcon;

    if (RegisterClassA(&windowClass) == NULL) {
        // TODO(hulvdan): Logging
        return 0;
    }

    auto window_handle = CreateWindowExA(
        0, windowClass.lpszClassName, "The Big Fuken Game", WS_TILEDWINDOW, CW_USEDEFAULT,
        CW_USEDEFAULT, 640, 480,
        NULL,  // [in, optional] HWND      hWndParent,
        NULL,  // [in, optional] HMENU     hMenu,
        application_handle,  // [in, optional] HINSTANCE hInstance,
        NULL  // [in, optional] LPVOID    lpParam
    );

    if (window_handle == NULL) {
        // TODO(hulvdan): Logging
        return 0;
    }

    ShowWindow(window_handle, show_command);
    screen_bitmap = BFBitmap();

    running = true;
    MSG message = {};

    auto device_context = GetDC(window_handle);
    while (running) {
        while (PeekMessageA(&message, NULL, 0, 0, PM_REMOVE) != 0) {
            if (message.message == WM_QUIT) {
                running = false;
                break;
            }

            TranslateMessage(&message);
            DispatchMessage(&message);
        }

        if (running) {
            // CONTROLLER STUFF
            // TODO(hulvdan): Measure latency?
            for (DWORD i = 0; i < XUSER_MAX_COUNT; i++) {
                XINPUT_STATE state;
                ZeroMemory(&state, sizeof(XINPUT_STATE));

                DWORD dwResult = XInputGetState_(i, &state);

                if (dwResult == ERROR_SUCCESS) {
                    // Controller is connected
                    float LX = state.Gamepad.sThumbLX;
                    float LY = state.Gamepad.sThumbLY;
                    float scale = 32000;

                    Goffset_x += LX / scale;
                    Goffset_y -= LY / scale;
                } else {
                    // TODO(hulvdan): handling disconnects
                }
            }
            // CONTROLLER STUFF END

            Win32Paint(window_handle, device_context);
            ReleaseDC(window_handle, device_context);
        }
    }

    return 0;
}
