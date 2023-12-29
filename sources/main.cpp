#include <iostream>
#include <assert.h>
#include "windows.h"

#define local_persist static
#define global_variable static

global_variable bool running = false;

const auto bits_per_pixel = 32;

global_variable HDC main_window_device_context;
global_variable HBITMAP independent_bitmap_handle;
global_variable BITMAPINFO bitmap_info;
global_variable void *bitmap_memory;
global_variable int client_width;
global_variable int client_height;

global_variable int bitmap_width = 800;
global_variable int bitmap_height = 600;

const auto BFG_CLASS_NAME = "BFGWindowClass";

void Win32UpdateWindow() {
    // bitmap_info.bmiHeader.biSize = bits_per_pixel / 8 * bitmap_width * bitmap_height;
    bitmap_info.bmiHeader.biSize = sizeof(bitmap_info.bmiHeader);
    bitmap_info.bmiHeader.biWidth = bitmap_width;
    bitmap_info.bmiHeader.biHeight = bitmap_height;
    bitmap_info.bmiHeader.biPlanes = 1;
    bitmap_info.bmiHeader.biBitCount = bits_per_pixel;
    bitmap_info.bmiHeader.biCompression = BI_RGB;
    // bitmap_info.bmiHeader.biSizeImage = 0;
    // bitmap_info.bmiHeader.biXPelsPerMeter = 0;
    // bitmap_info.bmiHeader.biYPelsPerMeter = 0;
    // bitmap_info.bmiHeader.biClrUsed = 0;
    // bitmap_info.bmiHeader.biClrImportant = 0;

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
        // HDC                    hdc,
        main_window_device_context,
        // const BITMAPINFOHEADER *pbmih,
        &bitmap_info.bmiHeader,
        // DWORD                  flInit,
        0,
        // const VOID             *pjBits,
        bitmap_memory,
        // const BITMAPINFO       *pbmi,
        &bitmap_info,
        // UINT                   iUsage
        DIB_RGB_COLORS
    );
}

void Win32PaintWindow() {
    assert(bitmap_memory != 0);
    auto pixelc = (uint8_t *)bitmap_memory;

    for (int y = 0; y < bitmap_height; y++) {
        for (int x = 0; x < bitmap_width; x++) {
            (*pixelc++) = (uint8_t)x;
            (*pixelc++) = (uint8_t)y;
            (*pixelc++) = 0;
            (*pixelc++) = 0;
        }
    }

    StretchDIBits(
        // HDC              hdc,
        main_window_device_context,
        // int              xDest,
        0,
        // int              yDest,
        0,
        // int              DestWidth,
        client_width,
        // int              DestHeight,
        client_height,
        // int              xSrc,
        0,
        // int              ySrc,
        0,
        // int              SrcWidth,
        bitmap_width,
        // int              SrcHeight,
        bitmap_height,
        // const VOID       *lpBits,
        bitmap_memory,
        // const BITMAPINFO *lpbmi,
        &bitmap_info,
        // UINT             iUsage,
        DIB_RGB_COLORS,
        // DWORD            rop
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
            Win32PaintWindow();
        } break;

        case WM_PAINT: {
            PAINTSTRUCT paint_struct;
            main_window_device_context = BeginPaint(hInstance, &paint_struct);

            // auto x = paint_struct.rcPaint.left;
            // auto y = paint_struct.rcPaint.top;
            // auto client_width = paint_struct.rcPaint.right - x;
            // auto client_height = paint_struct.rcPaint.bottom - y;

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

    // TODO(hulvdan): Learn more about these styles. Are they relevant nowadays?
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
    main_window_device_context = GetDC(window_handle);

    running = true;
    MSG message = {};
    while (running && GetMessage(&message, NULL, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessage(&message);
    }

    return 0;
}
