#include <iostream>
#include "windows.h"

const auto BFG_CLASS_NAME = "BFGWindowClass";

LRESULT windowEventsHandler(
    HWND hInstance,
    UINT    message_type,
    WPARAM  wParam,
    LPARAM  lParam
) {
    return 0;
}


int WinMain(
    HINSTANCE hInstance,
    HINSTANCE hPrevInstance,
    LPSTR     lpCmdLine,
    int       nShowCmd
) {

    WNDCLASSA wc = { };
    wc.style = CS_OWNDC; // UINT      style;
    wc.lpfnWndProc = *windowEventsHandler; // WNDPROC   lpfnWndProc;
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
        WS_VISIBLE | WS_BORDER, // TODO(Hulvdan): WS_MAXIMIZE, WS_MAXIMIZEBOX, WS_MINIMIZE, WS_MINIMIZE_BOX
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

    return 0;
}
