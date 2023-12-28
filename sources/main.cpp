#include <iostream>
#include "windows.h"

const auto BFG_CLASS_NAME = "BFGWindowClass";


LRESULT windowEventsHandler(
    HWND   hInstance,
    UINT   message_type,
    WPARAM wParam,
    LPARAM lParam
) {
    switch (message_type) {
        case (WM_CLOSE): {
            return DefWindowProc(hInstance, message_type, wParam, lParam);
        } break;

        case (WM_SIZE): {
        } break;

        // case WM_SIZE: {
        // } break;
        //
        // case WM_SIZE: {
        // } break;

        default:
            return DefWindowProc(hInstance, message_type, wParam, lParam);
    }
    return 0;
}

int WinMain(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPSTR     lpCmdLine,
    int       nShowCmd
) {
    WNDCLASSA wc = {};
    wc.style = CS_OWNDC; // UINT      style;
    wc.lpfnWndProc = *windowEventsHandler; // WNDPROC   lpfnWndProc;
    wc.lpszClassName = BFG_CLASS_NAME;
    // int       cbClsExtra;
    // int       cbWndExtra;
    wc.hInstance = hInstance; //  HINSTANCE hInstance;
    // HICON     hIcon;
    // HCURSOR   hCursor;
    // HBRUSH    hbrBackground;
    // LPCSTR    lpszMenuName;
    // LPCSTR    lpszClassName;

    RegisterClassA(&wc);

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
