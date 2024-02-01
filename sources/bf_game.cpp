#include <cassert>
#include <cstdlib>
#include <vector>
#include <memory>

#include "glew.h"
#include "wglew.h"

#include "bf_base.h"
#include "bf_game.h"

// NOLINTBEGIN(bugprone-suspicious-include)
#include "bf_game_types.cpp"
#include "bf_strings.cpp"
#include "bf_hash.cpp"
#include "bf_memory.cpp"
#include "bf_rand.cpp"
#include "bf_file.cpp"
#include "bf_game_map.cpp"

#ifdef BF_CLIENT
#include "bfc_tilemap.cpp"
#include "bfc_renderer.cpp"
#endif  // BF_CLIENT

#if BF_SERVER
#endif  // BF_SERVER
// NOLINTEND(bugprone-suspicious-include)

void Process_Events(Game_Memory& memory, u8* events, size_t input_events_count) {
    assert(memory.is_initialized);
    auto& state = memory.state;

    while (input_events_count > 0) {
        input_events_count--;
        auto type = (Event_Type)*events;
        events++;  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)

        size_t s = 0;

        switch (type) {
        case Event_Type::Mouse_Pressed: {
            Mouse_Pressed event = {};
            memcpy(&event, events, sizeof(Mouse_Pressed));
            s = sizeof(Mouse_Pressed);
        } break;

        case Event_Type::Mouse_Released: {
            Mouse_Released event = {};
            memcpy(&event, events, sizeof(Mouse_Released));
            s = sizeof(Mouse_Released);
        } break;

        case Event_Type::Mouse_Moved: {
            Mouse_Moved event = {};
            memcpy(&event, events, sizeof(Mouse_Moved));
            s = sizeof(Mouse_Moved);
        } break;

        case Event_Type::Keyboard_Pressed: {
            Keyboard_Pressed event = {};
            memcpy(&event, events, sizeof(Keyboard_Pressed));
            s = sizeof(Keyboard_Pressed);
        } break;

        case Event_Type::Keyboard_Released: {
            Keyboard_Released event = {};
            memcpy(&event, events, sizeof(Keyboard_Released));
            s = sizeof(Keyboard_Released);
        } break;

        case Event_Type::Controller_Button_Pressed: {
            Controller_Button_Pressed event = {};
            memcpy(&event, events, sizeof(Controller_Button_Pressed));
            s = sizeof(Controller_Button_Pressed);
        } break;

        case Event_Type::Controller_Button_Released: {
            Controller_Button_Released event = {};
            memcpy(&event, events, sizeof(Controller_Button_Released));
            s = sizeof(Controller_Button_Released);
        } break;

        case Event_Type::Controller_Axis_Changed: {
            Controller_Axis_Changed event = {};
            memcpy(&event, events, sizeof(Controller_Axis_Changed));
            s = sizeof(Controller_Axis_Changed);

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

        events += s;  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)
    }
}

extern "C" GAME_LIBRARY_EXPORT inline void Game_Update_And_Render(
    f32 dt,
    void* memory_ptr,
    size_t memory_size,
    Game_Bitmap& bitmap,
    void* input_events_bytes_ptr,
    size_t input_events_count  //
) {
    auto& memory = *(Game_Memory*)memory_ptr;
    auto& state = memory.state;

    if (!memory.is_initialized) {
        auto& state = memory.state;
        state.offset_x = 0;
        state.offset_y = 0;

        auto forest_string = "forest";
        state.DEBUG_forest.name = forest_string;
        state.DEBUG_forest.id = Hash32_String(forest_string);

        auto initial_offset = sizeof(Game_Memory);
        auto file_loading_arena_size = Megabytes((size_t)1);

        auto& file_loading_arena = state.file_loading_arena;
        file_loading_arena.size = file_loading_arena_size;
        file_loading_arena.base = (u8*)memory_ptr + initial_offset;

        auto& arena = state.memory_arena;
        arena.size = memory_size - initial_offset - file_loading_arena_size;
        arena.base = (u8*)memory_ptr + initial_offset + file_loading_arena_size;

        state.game_map = {};
        state.game_map.size = {32, 24};
        auto& dim = state.game_map.size;

        auto tiles_count = (size_t)dim.x * dim.y;
        state.game_map.terrain_tiles = Allocate_Array(arena, Terrain_Tile, tiles_count);
        state.game_map.terrain_resources = Allocate_Array(arena, Terrain_Resource, tiles_count);
        Regenerate_Terrain_Tiles(state, state.game_map, arena, 0);

        state.renderer_state = Initialize_Renderer(state.game_map, arena, file_loading_arena);

        memory.is_initialized = true;
    }

    Process_Events(memory, (u8*)input_events_bytes_ptr, input_events_count);

    assert(bitmap.bits_per_pixel == 32);
    auto pixel = (u32*)bitmap.memory;

    auto offset_x = (i32)state.offset_x;
    auto offset_y = (i32)state.offset_y;

    state.offset_y += dt * 24.0f;
    state.offset_x += dt * 32.0f;

    const auto player_radius = 24;

    FOR_RANGE(i32, y, bitmap.height) {
        FOR_RANGE(i32, x, bitmap.width) {
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

    Render(state, *state.renderer_state, bitmap);
}
