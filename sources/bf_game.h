#pragma once
#include "bf_base.h"

#define Kilobytes(value) ((value) * 1024)
#define Megabytes(value) (Kilobytes(value) * 1024)
#define Gigabytes(value) (Megabytes(value) * 1024)
#define Terabytes(value) (Gigabytes(value) * 1024)

// NOTE(hulvdan): This importing nonsense was hastily taken from
// https://blog.shaduri.dev/easily-create-shared-libraries-with-cmake-part-1
// I just wanted to enable dll builds as fast as possible.
// TODO(hulvdan): We need to find a better way of writing this BS.
// Is it possible to write this
#if GAME_LIBRARY_BUILD
// Building the library
#if _WIN32
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
#if _WIN32
// Use the Windows-specific import attribute
#define GAME_LIBRARY_EXPORT __declspec(dllimport)
#else
// Assume that no import attributes are needed
#define GAME_LIBRARY_EXPORT
#endif
#endif

struct GAME_LIBRARY_EXPORT Game_Bitmap {
    i32 width;
    i32 height;

    i32 bits_per_pixel;
    void* memory;
};
struct Perlin_Params {
    u8 octaves;
    f32 scaling_bias;
    uint seed;
};

struct GAME_LIBRARY_EXPORT Editor_Data {
    bool changed;

    Perlin_Params terrain_perlin;
    i8 terrain_max_height;

    Perlin_Params forest_perlin;
    f32 forest_threshold;
    i16 forest_max_amount;
};

Editor_Data Default_Editor_Data() {
    Editor_Data res = {};

    res.changed = true;

    res.terrain_perlin.octaves = 9;
    res.terrain_perlin.scaling_bias = 2.0f;
    res.terrain_perlin.seed = 0;
    res.terrain_max_height = 6;

    res.forest_perlin.octaves = 7;
    res.forest_perlin.scaling_bias = 0.7f;
    res.forest_perlin.seed = 0;
    res.forest_threshold = 0.47f;
    res.forest_max_amount = 5;

    return res;
}

// --- EVENTS START ---
enum class Event_Type {
    Mouse_Pressed,
    Mouse_Released,
    Mouse_Moved,
    Mouse_Scrolled,
    Keyboard_Pressed,
    Keyboard_Released,
    Controller_Button_Pressed,
    Controller_Button_Released,
    Controller_Axis_Changed,
};

// MOUSE
enum class Mouse_Button_Type {
    Left,
    Right,
    Middle,
    XButton1,
    XButton2,
};

using Mouse_ID = u8;

struct Mouse_Pressed {
    const static Event_Type _event_type = Event_Type::Mouse_Pressed;

    Mouse_ID id;
    Mouse_Button_Type type;
    v2f position;
};

struct Mouse_Released {
    const static Event_Type _event_type = Event_Type::Mouse_Released;

    Mouse_ID id;
    Mouse_Button_Type type;
    v2f position;
};

struct Mouse_Moved {
    const static Event_Type _event_type = Event_Type::Mouse_Moved;

    Mouse_ID id;
    v2f position;
};

// KEYBOARD
using Keyboard_ID = u8;
using Keyboard_Key = u32;

struct Keyboard_Pressed {
    const static Event_Type _event_type = Event_Type::Keyboard_Pressed;

    Keyboard_ID id;
    Keyboard_Key key;
};
struct Keyboard_Released {
    const static Event_Type _event_type = Event_Type::Keyboard_Released;

    Keyboard_ID id;
    Keyboard_Key key;
};

// CONTROLLER
using Controller_ID = u8;
using Controller_Button = u32;
using Controller_Axis = u32;

struct Controller_Button_Pressed {
    const static Event_Type _event_type = Event_Type::Controller_Button_Pressed;

    Controller_ID id;
    Controller_Button button;
};

struct Controller_Button_Released {
    const static Event_Type _event_type = Event_Type::Controller_Button_Released;

    Controller_ID id;
    Controller_Button button;
};

struct Controller_Axis_Changed {
    const static Event_Type _event_type = Event_Type::Controller_Axis_Changed;

    Controller_ID id;
    Controller_Axis axis;
    f32 value;
};
// --- EVENTS END ---

// --- EXPORTED FUNCTIONS ---
extern "C" GAME_LIBRARY_EXPORT inline void Game_Update_And_Render(
    f32 dt,
    void* __restrict memory_ptr,
    size_t memory_size,
    Game_Bitmap& __restrict bitmap,
    void* __restrict input_events_bytes_ptr,
    size_t input_events_count,
    Editor_Data& editor_data  //
);
// --- EXPORTED FUNCTIONS END ---
