#include <cassert>
#include <cstdlib>
#include <vector>
#include <memory>

#include "glew.h"
#include "wglew.h"

#include "range.h"
using cpp_range::range;

#include "bf_base.h"
#include "bf_game.h"

// NOLINTBEGIN(bugprone-suspicious-include)
#include "bf_strings.cpp"
#include "bf_hash.cpp"
#include "bf_rand.cpp"

#include "bf_game_types.cpp"

#include "bf_memory.cpp"
#include "bf_file.cpp"

#include "bf_gamemap.cpp"

#ifdef BF_CLIENT
#include "bfc_tilemap.cpp"
#include "bfc_renderer.cpp"
#endif  // BF_CLIENT

#if BF_SERVER
#endif  // BF_SERVER
// NOLINTEND(bugprone-suspicious-include)

void ProcessEvents(GameMemory& __restrict memory, u8* __restrict events, size_t input_events_count)
{
    assert(memory.is_initialized);
    auto& state = memory.state;

    while (input_events_count > 0) {
        input_events_count--;
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

extern "C" GAME_LIBRARY_EXPORT inline void Game_UpdateAndRender(
    f32 dt,
    void* memory_ptr,
    size_t memory_size,
    GameBitmap& bitmap,
    void* input_events_bytes_ptr,
    size_t input_events_count)
{
    auto& memory = *((GameMemory*)memory_ptr);
    auto& state = memory.state;

    if (!memory.is_initialized) {
        auto& state = memory.state;
        state.offset_x = 0;
        state.offset_y = 0;

        auto initial_offset = sizeof(GameMemory);
        auto file_loading_arena_size = Megabytes((size_t)1);

        auto& file_loading_arena = state.file_loading_arena;
        file_loading_arena.size = file_loading_arena_size;
        file_loading_arena.base = (u8*)memory_ptr + initial_offset;

        auto& arena = state.memory_arena;
        arena.size = memory_size - initial_offset - file_loading_arena_size;
        arena.base = (u8*)memory_ptr + initial_offset + file_loading_arena_size;

        state.gamemap = {};
        state.gamemap.size = {32, 24};
        auto& dim = state.gamemap.size;

        state.gamemap.terrain_tiles = AllocateArray(arena, TerrainTile, (size_t)dim.x * dim.y);
        RegenerateTerrainTiles(state.gamemap);

        state.renderer_state = InitializeRenderer(arena, file_loading_arena);

        memory.is_initialized = true;
    }

    ProcessEvents(memory, (u8*)input_events_bytes_ptr, input_events_count);

    assert(bitmap.bits_per_pixel == 32);
    auto pixel = (u32*)bitmap.memory;

    auto offset_x = (i32)state.offset_x;
    auto offset_y = (i32)state.offset_y;

    state.offset_y += dt * 24.0f;
    state.offset_x += dt * 32.0f;

    const auto player_radius = 24;

    for (i32 y : range(bitmap.height)) {
        for (i32 x : range(bitmap.width)) {
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
                const auto player_color = 255;
                red = player_color;
                green = player_color;
                blue = player_color;
            }

            (*pixel++) = (blue << 0) | (green << 8) | (red << 0);
        }
    }

    render(state, *state.renderer_state, bitmap);
}
