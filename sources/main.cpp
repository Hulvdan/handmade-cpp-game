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
            DestroyWindow(hInstance);
        } break;

        case WM_DESTROY: {
            PostQuitMessage(0);
        } break;

        // case WM_SIZE: {
        // } break;
        //
        // case WM_MOVE: {
        // } break;
        //
        // case WM_MOVING: {
        // } break;

        case WM_PAINT: {
            static bool white = true;
            static HBRUSH WHITENESSS = CreateSolidBrush(RGB(255, 255, 255));
            static HBRUSH BLACKNESSS = CreateSolidBrush(RGB(0, 0, 0));

            PAINTSTRUCT ps;
            auto hdc = BeginPaint(hInstance, &ps);

            HBRUSH brush;
            if (white) {
                brush = WHITENESSS;
            } else {
                brush = BLACKNESSS;
            }
            FillRect(hdc, &ps.rcPaint, brush);
            EndPaint(hInstance, &ps);

            white = !white;
        } break;

        // case WM_SIZE: {
        // } break;
        //
        // case WM_SIZE: {
        // } break;

        default:
            return DefWindowProc(hInstance, messageType, wParam, lParam);
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
    wc.style = CS_OWNDC;
    wc.lpfnWndProc = *WindowEventsHandler;
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
