#include "bf_base.h"

#define Kilobytes(value) ((value) * 1024)
#define Megabytes(value) (Kilobytes(value) * 1024)
#define Gigabytes(value) (Megabytes(value) * 1024)
#define Terabytes(value) (Gigabytes(value) * 1024)

// NOTE: This importing nonsense was hastily taken from
// TODO: https://blog.shaduri.dev/easily-create-shared-libraries-with-cmake-part-1
// I just wanted to enable dll builds as fast as possible.
// SHIT: Rewrite this shiet in a better way
//
// Is it possible to write this

#ifdef GAME_LIBRARY_BUILD
// #error "We are compiling the library"
// Building the library
#    if _WIN32
// Use the Windows-specific export attribute
#        define GAME_LIBRARY_EXPORT __declspec(dllexport)
#    elif __GNUC__ >= 4
// Use the GCC-specific export attribute
#        define GAME_LIBRARY_EXPORT __attribute__((visibility("default")))
#    else
// Assume that no export attributes are needed
#        define GAME_LIBRARY_EXPORT
#    endif
#else  // GAME_LIBRARY_BUILD
// #error "We are including the library"
// Using (including) the library
#    if _WIN32
// Use the Windows-specific import attribute
#        define GAME_LIBRARY_EXPORT __declspec(dllimport)
#    else
// Assume that no import attributes are needed
#        define GAME_LIBRARY_EXPORT
#    endif
#endif  // GAME_LIBRARY_BUILD

struct GAME_LIBRARY_EXPORT Game_Bitmap {
    i32 width;
    i32 height;

    i32   bits_per_pixel;
    void* memory;
};

struct Perlin_Params {
    int  octaves;
    f32  scaling_bias;
    uint seed;
};

struct ImGuiContext;
struct GAME_LIBRARY_EXPORT Editor_Data {
    bool          changed;
    bool          game_context_set;
    ImGuiContext* context;

    Perlin_Params terrain_perlin;
    int           terrain_max_height;

    Perlin_Params forest_perlin;
    f32           forest_threshold;
    int           forest_max_amount;

    int arena_numbers[2];

    int dll_reloads_count;
};

Editor_Data Default_Editor_Data() {
    Editor_Data res = {};

    res.changed          = false;
    res.game_context_set = false;
    res.context          = nullptr;

    res.terrain_perlin.octaves      = 9;
    res.terrain_perlin.scaling_bias = 2.0f;
    res.terrain_perlin.seed         = 0;
    res.terrain_max_height          = 6;

    res.forest_perlin.octaves      = 7;
    res.forest_perlin.scaling_bias = 0.38f;
    res.forest_perlin.seed         = 0;
    res.forest_threshold           = 0.54f;
    res.forest_max_amount          = 5;

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

    Mouse_ID          id;
    Mouse_Button_Type type;
    v2i               position;
};

struct Mouse_Released {
    const static Event_Type _event_type = Event_Type::Mouse_Released;

    Mouse_ID          id;
    Mouse_Button_Type type;
    v2i               position;
};

struct Mouse_Moved {
    const static Event_Type _event_type = Event_Type::Mouse_Moved;

    Mouse_ID id;
    v2i      position;
};

struct Mouse_Scrolled {
    const static Event_Type _event_type = Event_Type::Mouse_Scrolled;

    Mouse_ID id;
    i32      value;
};

// KEYBOARD
using Keyboard_ID  = u8;
using Keyboard_Key = u32;

struct Keyboard_Pressed {
    const static Event_Type _event_type = Event_Type::Keyboard_Pressed;

    Keyboard_ID  id;
    Keyboard_Key key;
};
struct Keyboard_Released {
    const static Event_Type _event_type = Event_Type::Keyboard_Released;

    Keyboard_ID  id;
    Keyboard_Key key;
};

// CONTROLLER
using Controller_ID     = u8;
using Controller_Button = u32;
using Controller_Axis   = u32;

struct Controller_Button_Pressed {
    const static Event_Type _event_type = Event_Type::Controller_Button_Pressed;

    Controller_ID     id;
    Controller_Button button;
};

struct Controller_Button_Released {
    const static Event_Type _event_type = Event_Type::Controller_Button_Released;

    Controller_ID     id;
    Controller_Button button;
};

struct Controller_Axis_Changed {
    const static Event_Type _event_type = Event_Type::Controller_Axis_Changed;

    Controller_ID   id;
    Controller_Axis axis;
    f32             value;
};
// --- EVENTS END ---

// --- EXPORTED FUNCTIONS ---
#define GameUpdateAndRender_Function(name_)  \
    void name_(                              \
        f32          dt,                     \
        void*        memory_ptr,             \
        size_t       memory_size,            \
        Game_Bitmap& bitmap,                 \
        void*        input_events_bytes_ptr, \
        size_t       input_events_count,     \
        Editor_Data& editor_data,            \
        bool         hot_reloaded            \
    ) noexcept

extern "C" GAME_LIBRARY_EXPORT GameUpdateAndRender_Function(Game_Update_And_Render);
// --- EXPORTED FUNCTIONS END ---
