#include <iostream>
#include <assert.h>
#include "windows.h"

#define local_persist static
#define global_variable static

global_variable bool running = false;

const auto bits_per_pixel = 32;

global_variable bool should_recreate_bitmap;
global_variable int client_width;
global_variable int client_height;
global_variable int bitmap_width;
global_variable int bitmap_height;

global_variable HBITMAP bitmap_handle;
global_variable BITMAPINFO bitmap_info;
global_variable void *bitmap_memory;

global_variable int Goffset_x = 0;

const auto BFG_CLASS_NAME = "BFGWindowClass";

void Win32UpdateBitmap(HDC device_context) {
    assert(client_width >= 0);
    assert(client_height >= 0);

    bitmap_width = client_width;
    bitmap_height = client_height;

    bitmap_info.bmiHeader.biSize = sizeof(bitmap_info.bmiHeader);
    bitmap_info.bmiHeader.biWidth = client_width;
    bitmap_info.bmiHeader.biHeight = -client_height;
    bitmap_info.bmiHeader.biPlanes = 1;
    bitmap_info.bmiHeader.biBitCount = bits_per_pixel;
    bitmap_info.bmiHeader.biCompression = BI_RGB;

    if (bitmap_memory) {
        VirtualFree(bitmap_memory, 0, MEM_RELEASE);
    }

    bitmap_memory = VirtualAlloc(
        0,
        bitmap_width * bitmap_height * bits_per_pixel / 8,
        MEM_COMMIT,
        PAGE_EXECUTE_READWRITE
    );

    if (bitmap_handle) {
        DeleteObject(bitmap_handle);
    }

    bitmap_handle = CreateDIBitmap(
        device_context,
        &bitmap_info.bmiHeader,
        0,
        bitmap_memory,
        &bitmap_info,
        DIB_RGB_COLORS
    );
}

void Win32RenderWeirdGradient(int offset_x, int offset_y) {
    auto pixelc = (uint8_t *)bitmap_memory;

    for (int y = 0; y < bitmap_height; y++) {
        for (int x = 0; x < bitmap_width; x++) {
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
        0, 0, bitmap_width, bitmap_height,
        bitmap_memory,
        &bitmap_info,
        DIB_RGB_COLORS,
        SRCCOPY
    );
}

void Win32Paint(HWND window_handle, HDC device_context) {
    Win32RenderWeirdGradient(Goffset_x++, 0);
    Win32BlitBitmapToTheWindow(device_context);
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
            should_recreate_bitmap = true;

            auto device_context = GetDC(window_handle);
            Win32UpdateBitmap(device_context);
            ReleaseDC(window_handle, device_context);
        } break;

        case WM_PAINT: 
        {
            assert(client_width != 0);
            assert(client_height != 0);

            PAINTSTRUCT paint_struct;
            auto device_context = BeginPaint(window_handle, &paint_struct);

            if (should_recreate_bitmap) {
                Win32UpdateBitmap(device_context);
            }

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
    windowClass.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = *WindowEventsHandler;
    windowClass.lpszClassName = BFG_CLASS_NAME;
    windowClass.hInstance = application_handle;

    // TODO(hulvdan): Icon!
    // HICON     hIcon;

    if (RegisterClassA(&windowClass) == NULL) {
        // TODO(hulvdan): Logging
        return 0;
    }

    auto window_handle = CreateWindowExA(
        0,
        BFG_CLASS_NAME,
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
    bitmap_info = BITMAPINFO();

    running = true;
    MSG message = {};
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
            auto device_context = GetDC(window_handle);

            Win32RenderWeirdGradient(Goffset_x++, 0);
            Win32BlitBitmapToTheWindow(device_context);

            ReleaseDC(window_handle, device_context);
        }
    }

    return 0;
}
