#include <cassert>

#include "bftypes.h"
#include "game.h"

struct GameState {
    f32 offset_x;
    f32 offset_y;

    v2f player_pos;
};

struct GameMemory {
    bool is_initialized;
    GameState state;
};

// --- EVENTS START
extern "C" GAME_LIBRARY_EXPORT inline void
Game_ProcessEvents(void* memory_, void* events_, size_t n)
{
    auto& memory = *((GameMemory*)memory_);
    assert(memory.is_initialized);
    auto& state = memory.state;

    auto events = (u8*)events_;
    while (n > 0) {
        n--;
        auto type = (EventType)(*events++);

        switch (type) {
        case EventType::MousePressed: {
            auto& event = *(MousePressed*)events;
            events += sizeof(MousePressed);
        } break;

        case EventType::MouseReleased: {
            auto& event = *(MouseReleased*)events;
            events += sizeof(MouseReleased);
        } break;

        case EventType::MouseMoved: {
            auto& event = *(MouseMoved*)events;
            events += sizeof(MouseMoved);
        } break;

        case EventType::KeyboardPressed: {
            auto& event = *(KeyboardPressed*)events;
            events += sizeof(KeyboardPressed);
        } break;

        case EventType::KeyboardReleased: {
            auto& event = *(KeyboardReleased*)events;
            events += sizeof(KeyboardReleased);
        } break;

        case EventType::ControllerButtonPressed: {
            auto& event = *(ControllerButtonPressed*)events;
            events += sizeof(ControllerButtonPressed);
        } break;

        case EventType::ControllerButtonReleased: {
            auto& event = *(ControllerButtonReleased*)events;
            events += sizeof(ControllerButtonReleased);
        } break;

        case EventType::ControllerAxisChanged: {
            auto& event = *(ControllerAxisChanged*)events;
            events += sizeof(ControllerAxisChanged);

            assert(event.axis >= 0 && event.axis <= 1);
            if (event.axis == 0)
                state.player_pos.x += event.value;
            else
                state.player_pos.y -= event.value;
        } break;

        default:
            // TODO(hulvdan): Diagnostic
            assert(false);
        }
    }
}
// --- EVENTS END

extern "C" GAME_LIBRARY_EXPORT inline void
Game_UpdateAndRender(f32 dt, void* memory_, GameBitmap& bitmap)
{
    auto& memory = *((GameMemory*)memory_);
    auto& state = memory.state;

    if (!memory.is_initialized) {
        state.offset_x = 0;
        state.offset_y = 0;
        memory.is_initialized = true;
    }

    assert(bitmap.bits_per_pixel == 32);
    auto pixel = (u32*)bitmap.memory;

    auto offset_x = (i32)state.offset_x;
    auto offset_y = (i32)state.offset_y;

    state.offset_y += dt * 256.0;
    state.offset_x += dt * 256.0;

    const auto player_radius = 20;

    for (i32 y = 0; y < bitmap.height; y++) {
        for (i32 x = 0; x < bitmap.width; x++) {
            // u32 red  = 0;
            u32 red = (u8)(x + offset_x);
            // u32 red  = (u8)(y + offset_y);
            u32 green = (u8)0;
            // u32 green = (u8)(x + offset_x);
            // u32 green = (u8)(y + offset_y);
            // u32 blue = 0;
            // u32 blue = (u8)(x + offset_x);
            u32 blue = (u8)(y + offset_y);

            auto dx = x - (i32)state.player_pos.x;
            auto dy = y - (i32)state.player_pos.y;
            if (dx * dx + dy * dy < player_radius * player_radius) {
                red = 255;
                green = 255;
                blue = 255;
            }

            (*pixel++) = (blue << 0) | (green << 8) | (red << 16);
        }
    }
}
