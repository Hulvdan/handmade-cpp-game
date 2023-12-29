#include <iostream>
#include "windows.h"

const auto BFG_CLASS_NAME = "BFGWindowClass";

LRESULT WindowEventsHandler(
    HWND   hInstance,
    UINT   messageType,
    WPARAM wParam,
    LPARAM lParam
) {
    switch (messageType) {
        case WM_CLOSE: {
            PostQuitMessage(0);
        } break;

        case WM_PAINT: {
            static DWORD operation = WHITENESS;

            PAINTSTRUCT paint_struct;
            HDC device_context = BeginPaint(hInstance, &paint_struct);

            auto x = paint_struct.rcPaint.left;
            auto y = paint_struct.rcPaint.top;
            auto width = paint_struct.rcPaint.right - x;
            auto height = paint_struct.rcPaint.bottom - y;

            PatBlt(device_context, x, y, width, height, operation);

            EndPaint(hInstance, &paint_struct);
            if (operation == WHITENESS) {
                operation = BLACKNESS;
            } else {
                operation = WHITENESS;
            }
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

    // TODO(Hulvdan): Learn more about these styles. Are they relevant nowadays?
    windowClass.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = *WindowEventsHandler;
    windowClass.lpszClassName = BFG_CLASS_NAME;
    // int       cbClsExtra;
    // int       cbWndExtra;
    windowClass.hInstance = hInstance; //  HINSTANCE hInstance;
    // HICON     hIcon;
    // HCURSOR   hCursor;
    // HBRUSH    hbrBackground;
    // LPCSTR    lpszMenuName;
    // LPCSTR    lpszClassName;

    if (RegisterClassA(&windowClass) == NULL) {
        // TODO(Hulvdan): Logging
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
        // TODO(Hulvdan): Logging
        return 0;
    }
    ShowWindow(window_handle, nShowCmd);

    MSG message = {};
    while (GetMessage(&message, NULL, 0, 0) > 0) {
        TranslateMessage(&message);
        DispatchMessage(&message);
    }

    return 0;
}
