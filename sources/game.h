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

struct v2f {
    f32 x;
    f32 y;
};

// --- EVENTS ---
enum class EventType {
    MousePressed,
    MouseReleased,
    MouseMoved,
    MouseScrolled,
    KeyboardPressed,
    KeyboardReleased,
    ControllerButtonPressed,
    ControllerButtonReleased,
    ControllerAxisChanged,
};

// MOUSE
enum class MouseButtonType {
    Left,
    Right,
    Middle,
    XButton1,
    XButton2,
};

using MouseID = u8;

struct MousePressed {
    const static EventType _event_type = EventType::MousePressed;

    MouseID id;
    MouseButtonType type;
    v2f position;
};

struct MouseReleased {
    const static EventType _event_type = EventType::MouseReleased;

    MouseID id;
    MouseButtonType type;
    v2f position;
};

struct MouseMoved {
    const static EventType _event_type = EventType::MouseMoved;

    MouseID id;
    v2f position;
};

// KEYBOARD
using KeyboardID = u8;
using KeyboardKey = u32;

struct KeyboardPressed {
    const static EventType _event_type = EventType::KeyboardPressed;

    KeyboardID id;
    KeyboardKey key;
};
struct KeyboardReleased {
    const static EventType _event_type = EventType::KeyboardReleased;

    KeyboardID id;
    KeyboardKey key;
};

// CONTROLLER
using ControllerID = u8;
using ControllerButton = u32;
using ControllerAxis = u32;

struct ControllerButtonPressed {
    const static EventType _event_type = EventType::ControllerButtonPressed;

    ControllerID id;
    ControllerButton button;
};

struct ControllerButtonReleased {
    const static EventType _event_type = EventType::ControllerButtonReleased;

    ControllerID id;
    ControllerButton button;
};

struct ControllerAxisChanged {
    const static EventType _event_type = EventType::ControllerAxisChanged;

    ControllerID id;
    ControllerAxis axis;
    f32 value;
};
// --- EVENTS END ---

// --- EXPORTED FUNCTIONS ---
extern "C" GAME_LIBRARY_EXPORT inline void Game_UpdateAndRender(
    f32 dt,
    void* memory_ptr,
    GameBitmap& bitmap,
    void* input_events_bytes_ptr,
    size_t input_events_count);
// --- EXPORTED FUNCTIONS END ---
