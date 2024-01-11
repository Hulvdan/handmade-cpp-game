#pragma once
#include "bftypes.h"

// NOTE(hulvdan): This importing nonsense was hastily taken from
// https://blog.shaduri.dev/easily-create-shared-libraries-with-cmake-part-1
// I just wanted to enable dll builds as fast as possible.
// TODO(hulvdan): We need to find a better way of writing this BS.
// Is it possible to write this
#ifdef GAME_LIBRARY_BUILD
    // Building the library
    #ifdef _WIN32
        // Use the Windows-specific export attribute
        #define GAME_LIBRARY_EXPORT __declspec(dllexport)
    #elif __GNUC__ >= 4
        // Use the GCC-specific export attribute
        #define GAME_LIBRARY_EXPORT __attribute__((visibility("default")))
    #else
        // Assume that no export attributes are needed
        #define GAME_LIBRARY_EXPORT
    #endif
#else
    // Using (including) the library
    #ifdef _WIN32
        // Use the Windows-specific import attribute
        #define GAME_LIBRARY_EXPORT __declspec(dllimport)
    #else
        // Assume that no import attributes are needed
        #define GAME_LIBRARY_EXPORT
    #endif
#endif

struct GAME_LIBRARY_EXPORT GameBitmap {
    i32 width;
    i32 height;

    i32 bits_per_pixel;
    void* memory;
};

extern "C" GAME_LIBRARY_EXPORT inline void Game_UpdateAndRender(void* memory_, GameBitmap& bitmap);
