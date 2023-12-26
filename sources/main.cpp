#include <iostream>
#include "windows.h"

int WinMain(HINSTANCE hInstance,
            HINSTANCE hPrevInstance,
            LPSTR     lpCmdLine,
            int       nShowCmd
) {
    MessageBox(NULL, "Super pupa aboba!", "Caption!",
               MB_OK | MB_OKCANCEL);

    return 0;
}

