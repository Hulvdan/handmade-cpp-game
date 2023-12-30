#include <iostream>
#include <assert.h>
#include "windows.h"

#define local_persist static
#define global_variable static

struct BFBitmap {
    HBITMAP handle;
    BITMAPINFO info;

    int bits_per_pixel;
    int width;
    int height;
    void *memory;
};

global_variable bool running = false;

global_variable bool should_recreate_bitmap_after_client_area_resize;
global_variable BFBitmap screen_bitmap;

global_variable int client_width;
global_variable int client_height;

global_variable int Goffset_x = 0;


void Win32UpdateBitmap(HDC device_context) {
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
        0,
        screen_bitmap.width * screen_bitmap.height * screen_bitmap.bits_per_pixel / 8,
        MEM_COMMIT,
        PAGE_EXECUTE_READWRITE
    );

    if (screen_bitmap.handle) {
        DeleteObject(screen_bitmap.handle);
    }

    screen_bitmap.handle = CreateDIBitmap(
        device_context,
        &screen_bitmap.info.bmiHeader,
        0,
        screen_bitmap.memory,
        &screen_bitmap.info,
        DIB_RGB_COLORS
    );
}

void Win32RenderWeirdGradient(int offset_x, int offset_y) {
    auto pixelc = (uint8_t *)screen_bitmap.memory;

    for (int y = 0; y < screen_bitmap.height; y++) {
        for (int x = 0; x < screen_bitmap.width; x++) {
            // Blue
            (*pixelc++) = (uint8_t)(y + offset_y);
            // Green
            (*pixelc++) = 0;
            // Red
            (*pixelc++) = (uint8_t)(x + offset_x);
            // XX
            (*pixelc++) = 0;
        }
    }
}

void Win32BlitBitmapToTheWindow(HDC device_context) {
    StretchDIBits(
        device_context,
        0, 0, client_width, client_height,
        0, 0, screen_bitmap.width, screen_bitmap.height,
        screen_bitmap.memory,
        &screen_bitmap.info,
        DIB_RGB_COLORS,
        SRCCOPY
    );
}

void Win32Paint(HWND window_handle, HDC device_context) {
    if (should_recreate_bitmap_after_client_area_resize) {
        Win32UpdateBitmap(device_context);
    }

    Win32RenderWeirdGradient(Goffset_x, Goffset_x);
    Win32BlitBitmapToTheWindow(device_context);

    Goffset_x++;
}

LRESULT WindowEventsHandler(
    HWND   window_handle,
    UINT   messageType,
    WPARAM wParam,
    LPARAM lParam
) {
    switch (messageType) {
        case WM_CLOSE:
        {
            running = false;
        } break;

        case WM_DESTROY:
        {
            // TODO(hulvdan): It was an error. Should we try to recreate the window?
            running = false;
        } break;

        case WM_SIZE:
        {
            client_width = LOWORD(lParam);
            client_height = HIWORD(lParam);
            should_recreate_bitmap_after_client_area_resize = true;
        } break;

        case WM_PAINT:
        {
            assert(client_width != 0);
            assert(client_height != 0);

            PAINTSTRUCT paint_struct;
            auto device_context = BeginPaint(window_handle, &paint_struct);

            Win32Paint(window_handle, device_context);
            EndPaint(window_handle, &paint_struct);
        } break;

        default:
        {
            return DefWindowProc(window_handle, messageType, wParam, lParam);
        }
    }
    return 0;
}

int WinMain(
    HINSTANCE application_handle,
    HINSTANCE previous_window_instance_handle,
    LPSTR     command_line,
    int       show_command
) {
    WNDCLASSA windowClass = {};

    // TODO(hulvdan): Learn more about these styles. Are they even relevant nowadays?
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
        0,
        windowClass.lpszClassName,
        "The Big Fuken Game",
        WS_TILEDWINDOW,
        CW_USEDEFAULT, CW_USEDEFAULT, 640, 480,
        NULL,       // [in, optional] HWND      hWndParent,
        NULL,       // [in, optional] HMENU     hMenu,
        application_handle,  // [in, optional] HINSTANCE hInstance,
        NULL        // [in, optional] LPVOID    lpParam
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
            Win32Paint(window_handle, device_context);
            ReleaseDC(window_handle, device_context);
        }
    }

    return 0;
}
