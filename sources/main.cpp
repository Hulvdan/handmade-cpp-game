#include <iostream>
#include <assert.h>
#include "windows.h"

#define local_persist static
#define global_variable static

global_variable bool running = false;

const auto bits_per_pixel = 32;

global_variable HDC device_context;
global_variable HBITMAP independent_bitmap_handle;
global_variable BITMAPINFO bitmap_info;
global_variable void *bitmap_memory;
global_variable int client_width;
global_variable int client_height;

global_variable int bitmap_width = 800;
global_variable int bitmap_height = 600;

const auto BFG_CLASS_NAME = "BFGWindowClass";

void Win32UpdateWindow() {
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

    if (independent_bitmap_handle) {
        DeleteObject(independent_bitmap_handle);
    }

    independent_bitmap_handle = CreateDIBitmap(
        device_context,
        &bitmap_info.bmiHeader,
        0,
        bitmap_memory,
        &bitmap_info,
        DIB_RGB_COLORS
    );
}

void Win32PaintWindow() {
    auto pixelc = (uint8_t *)bitmap_memory;

    for (int y = 0; y < bitmap_height; y++) {
        for (int x = 0; x < bitmap_width; x++) {
            // Blue
            (*pixelc++) = (uint8_t)x;
            // Green
            (*pixelc++) = (uint8_t)y;
            // Red
            (*pixelc++) = 0;
            // XX
            (*pixelc++) = 0;
        }
    }

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

LRESULT WindowEventsHandler(
    HWND   hInstance,
    UINT   messageType,
    WPARAM wParam,
    LPARAM lParam
) {
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
            Win32UpdateWindow();

            PAINTSTRUCT paint_struct;
            device_context = BeginPaint(hInstance, &paint_struct);
            Win32PaintWindow();
            EndPaint(hInstance, &paint_struct);
        } break;

        case WM_PAINT: {
            assert(client_width != 0);
            assert(client_height != 0);

            PAINTSTRUCT paint_struct;
            device_context = BeginPaint(hInstance, &paint_struct);
            Win32PaintWindow();
            EndPaint(hInstance, &paint_struct);
        } break;

        default: {
            return DefWindowProc(hInstance, messageType, wParam, lParam);
        }
    }
    return 0;
}

int WinMain(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPSTR     lpCmdLine,
    int       nShowCmd
) {
    WNDCLASSA windowClass = {};

    // TODO(hulvdan): Learn more about these styles. Are they even relevant nowadays?
    windowClass.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = *WindowEventsHandler;
    windowClass.lpszClassName = BFG_CLASS_NAME;
    windowClass.hInstance = hInstance;

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
        hInstance,  // [in, optional] HINSTANCE hInstance,
        NULL        // [in, optional] LPVOID    lpParam
    );

    if (window_handle == NULL) {
        // TODO(hulvdan): Logging
        return 0;
    }

    ShowWindow(window_handle, nShowCmd);
    bitmap_info = BITMAPINFO();
    device_context = GetDC(window_handle);

    running = true;
    MSG message = {};
    while (running && GetMessage(&message, NULL, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessage(&message);
    }

    return 0;
}
