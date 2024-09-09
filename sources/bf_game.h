#pragma once
#include "bf_base.h"

// NOLINTBEGIN(bugprone-suspicious-include)
#include "bf_keys.cpp"
// NOLINTEND(bugprone-suspicious-include)

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

#if GAME_LIBRARY_BUILD
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
#else
// #error "We are including the library"
// Using (including) the library
#    if _WIN32
// Use the Windows-specific import attribute
#        define GAME_LIBRARY_EXPORT __declspec(dllimport)
#    else
// Assume that no import attributes are needed
#        define GAME_LIBRARY_EXPORT
#    endif
#endif

struct GAME_LIBRARY_EXPORT Game_Bitmap {
    i32 width;
    i32 height;

    i32   bits_per_pixel;
    void* memory;
};

struct ImGuiContext;

#define OS_Open_File_function(name_) void* name_(const char* filename) noexcept
#define OS_Write_To_File_function(name_) void name_(void* file, const char* text) noexcept
#define OS_Get_Time_function(name_) f64 name_() noexcept
#define OS_Die_function(name_) void name_() noexcept

struct GAME_LIBRARY_EXPORT Platform {
    bool          game_context_set  = {};
    ImGuiContext* imgui_context     = {};
    int           dll_reloads_count = {};

    void*     logger_data          = {};
    void_func logger_routine       = {};
    void_func logger_scope_routine = {};

    OS_Open_File_function((*Open_File))         = {};
    OS_Write_To_File_function((*Write_To_File)) = {};
    OS_Get_Time_function((*Get_Time))           = {};
    OS_Die_function((*Die))                     = {};
};

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
#define Game_Update_And_Render_function(name_) \
    void name_(                                \
        f32          dt,                       \
        void*        memory_ptr,               \
        size_t       memory_size,              \
        Game_Bitmap& bitmap,                   \
        void*        input_events_bytes_ptr,   \
        size_t       input_events_count,       \
        Platform&    platform,                 \
        bool         hot_reloaded              \
    ) noexcept

extern "C" GAME_LIBRARY_EXPORT Game_Update_And_Render_function(Game_Update_And_Render);
// --- EXPORTED FUNCTIONS END ---
