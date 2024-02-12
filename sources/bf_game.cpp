#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_win32.h"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <cassert>
#include <cstdlib>
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
#include "bf_math.cpp"
#include "bf_game_map.cpp"

#ifdef BF_CLIENT
#include "bfc_tilemap.cpp"
#include "bfc_renderer.cpp"
#endif  // BF_CLIENT

#if BF_SERVER
#endif  // BF_SERVER
// NOLINTEND(bugprone-suspicious-include)

#define PROCESS_EVENTS__CONSUME(event_type_, variable_name_) \
    event_type_ variable_name_ = {};                         \
    memcpy(&variable_name_, events, sizeof(event_type_));    \
    events += sizeof(event_type_);  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)

void Process_Events(Game_Memory& memory, u8* events, size_t input_events_count, float dt) {
    assert(memory.is_initialized);
    auto& state = memory.state;
    auto& rstate = *state.renderer_state;

    while (input_events_count > 0) {
        input_events_count--;
        auto type = (Event_Type)*events;
        events++;  // NOLINT(cppcoreguidelines-pro-bounds-pointer-arithmetic)

        switch (type) {
        case Event_Type::Mouse_Pressed: {
            PROCESS_EVENTS__CONSUME(Mouse_Pressed, event);

            rstate.mouse_pos = event.position;

            if (event.type == Mouse_Button_Type::Left) {
                auto tile_pos = World_Pos_To_Tile(Screen_To_World(state, rstate.mouse_pos));
                if (Pos_Is_In_Bounds(tile_pos, state.game_map.size)) {
                    Try_Build(state, tile_pos, Item_To_Build::Flag);
                    Try_Build(state, tile_pos, Item_To_Build::Road);
                }
            } else if (event.type == Mouse_Button_Type::Right) {
                rstate.panning = true;
                rstate.pan_start_pos = event.position;
                rstate.pan_offset = v2i(0, 0);
            }
        } break;

        case Event_Type::Mouse_Released: {
            PROCESS_EVENTS__CONSUME(Mouse_Released, event);

            rstate.mouse_pos = event.position;

            if (event.type == Mouse_Button_Type::Left) {
            } else if (event.type == Mouse_Button_Type::Right) {
                rstate.panning = false;
                rstate.pan_pos = (v2i)rstate.pan_pos + (v2i)rstate.pan_offset;
                rstate.pan_offset = v2i(0, 0);
            }
        } break;

        case Event_Type::Mouse_Moved: {
            PROCESS_EVENTS__CONSUME(Mouse_Moved, event);

            if (rstate.panning)
                rstate.pan_offset = event.position - rstate.pan_start_pos;

            rstate.mouse_pos = event.position;
        } break;

        case Event_Type::Mouse_Scrolled: {
            PROCESS_EVENTS__CONSUME(Mouse_Scrolled, event);

            if (event.value > 0) {
                rstate.zoom_target *= 2.0f;
            } else if (event.value < 0) {
                rstate.zoom_target /= 2.0f;
            } else
                assert(false);

            rstate.zoom_target = MAX(0.5f, MIN(8.0f, rstate.zoom_target));
        } break;

        case Event_Type::Keyboard_Pressed: {
            PROCESS_EVENTS__CONSUME(Keyboard_Pressed, event);
        } break;

        case Event_Type::Keyboard_Released: {
            PROCESS_EVENTS__CONSUME(Keyboard_Released, event);
        } break;

        case Event_Type::Controller_Button_Pressed: {
            PROCESS_EVENTS__CONSUME(Controller_Button_Pressed, event);
        } break;

        case Event_Type::Controller_Button_Released: {
            PROCESS_EVENTS__CONSUME(Controller_Button_Released, event);
        } break;

        case Event_Type::Controller_Axis_Changed: {
            PROCESS_EVENTS__CONSUME(Controller_Axis_Changed, event);

            assert(event.axis >= 0 && event.axis <= 1);
            if (event.axis == 0)
                state.player_pos.x += event.value;
            else
                state.player_pos.y -= event.value;
        } break;

        default:
            // TODO(hulvdan): Diagnostic
            UNREACHABLE;
        }
    }
}

extern "C" GAME_LIBRARY_EXPORT inline void Game_Update_And_Render(
    f32 dt,
    void* memory_ptr,
    size_t memory_size,
    Game_Bitmap& bitmap,
    void* input_events_bytes_ptr,
    size_t input_events_count,
    Editor_Data& editor_data,
    OS_Data& os_data  //
) {
    auto& memory = *(Game_Memory*)memory_ptr;
    auto& state = memory.state;

    if (!editor_data.game_context_set) {
        ImGui::SetCurrentContext(editor_data.context);
        editor_data.game_context_set = true;
    }

    // --- IMGUI ---
    if (memory.is_initialized) {
        if (state.renderer_state != nullptr) {
            auto& rstate = *state.renderer_state;
            ImGui::Text("Mouse %d.%d", rstate.mouse_pos.x, rstate.mouse_pos.y);
        }

        if (ImGui::SliderInt("Terrain Octaves", &editor_data.terrain_perlin.octaves, 1, 9)) {
            editor_data.changed = true;
        }
        if (ImGui::SliderFloat(
                "Terrain Scaling Bias", &editor_data.terrain_perlin.scaling_bias, 0.001f, 2.0f)) {
            editor_data.changed = true;
        }
        if (ImGui::Button("New Terrain Seed")) {
            editor_data.changed = true;
            editor_data.terrain_perlin.seed++;
        }
        if (ImGui::SliderInt("Terrain Max Height", &editor_data.terrain_max_height, 1, 35)) {
            editor_data.changed = true;
        }

        if (ImGui::SliderInt("Forest Octaves", &editor_data.forest_perlin.octaves, 1, 9)) {
            editor_data.changed = true;
        }
        if (ImGui::SliderFloat(
                "Forest Scaling Bias", &editor_data.forest_perlin.scaling_bias, 0.001f, 2.0f)) {
            editor_data.changed = true;
        }
        if (ImGui::Button("New Forest Seed")) {
            editor_data.changed = true;
            editor_data.forest_perlin.seed++;
        }
        if (ImGui::SliderFloat("Forest Threshold", &editor_data.forest_threshold, 0.0f, 1.0f)) {
            editor_data.changed = true;
        }
        if (ImGui::SliderInt("Forest MaxAmount", &editor_data.forest_max_amount, 1, 35)) {
            editor_data.changed = true;
        }
    }
    // --- IMGUI END ---

    if (!memory.is_initialized || editor_data.changed) {
        state.os_data = &os_data;
        editor_data.changed = false;

        state.offset_x = 0;
        state.offset_y = 0;

        auto initial_offset = sizeof(Game_Memory);
        auto file_loading_arena_size = Megabytes((size_t)1);

        auto& file_loading_arena = state.file_loading_arena;
        file_loading_arena.base = (u8*)memory_ptr + initial_offset;
        file_loading_arena.size = file_loading_arena_size;
        file_loading_arena.used = 0;

        auto& arena = state.memory_arena;
        arena.base = (u8*)memory_ptr + initial_offset + file_loading_arena_size;
        arena.size = memory_size - initial_offset - file_loading_arena_size;
        arena.used = 0;

        auto pages_count_that_fit_2GB = (size_t)2 * 1024 * 1024 * 1024 / os_data.page_size;
        state.pages.base = Allocate_Zeros_Array(arena, Page, pages_count_that_fit_2GB);
        state.pages.in_use = Allocate_Zeros_Array(arena, bool, pages_count_that_fit_2GB);
        state.pages.total_pages_count_cap = pages_count_that_fit_2GB;

        state.game_map = {};
        state.game_map.size = {32, 24};
        auto& gsize = state.game_map.size;

        auto tiles_count = (size_t)gsize.x * gsize.y;
        state.game_map.terrain_tiles = Allocate_Zeros_Array(arena, Terrain_Tile, tiles_count);
        state.game_map.terrain_resources =
            Allocate_Zeros_Array(arena, Terrain_Resource, tiles_count);
        state.game_map.element_tiles = Allocate_Zeros_Array(arena, Element_Tile, tiles_count);

        state.scriptable_resources_count = 1;
        state.scriptable_resources = Allocate_Zeros_Array(arena, Scriptable_Resource, 1);
        {
            auto r_ = Get_Scriptable_Resource(state, 1);
            assert(r_ != nullptr);
            auto r = *r_;

            r.name = "forest";
        }

        state.scriptable_buildings_count = 2;
        state.scriptable_buildings = Allocate_Zeros_Array(arena, Scriptable_Building, 2);
        {
            auto b_ = Get_Scriptable_Building(state, 1);
            assert(b_ != nullptr);
            auto b = *b_;

            b.name = "City Hall";
            b.type = Building_Type::City_Hall;
        }
        {
            auto b_ = Get_Scriptable_Building(state, 2);
            assert(b_ != nullptr);
            auto b = *b_;

            b.name = "Lumberjack's Hut";
            b.type = Building_Type::Harvest;
        }

        Regenerate_Terrain_Tiles(state, state.game_map, arena, 0, editor_data);
        Regenerate_Element_Tiles(state, state.game_map, arena, 0, editor_data);
        state.renderer_state = Initialize_Renderer(state, arena, file_loading_arena);

        auto max_hypothetical_count_of_building_pages =
            Ceil_To_i32((f32)tiles_count * sizeof(Building) / os_data.page_size);

        assert(max_hypothetical_count_of_building_pages < 100);
        assert(max_hypothetical_count_of_building_pages > 0);

        state.game_map.building_pages_total = max_hypothetical_count_of_building_pages;
        state.game_map.building_pages_used = 0;
        state.game_map.building_pages =
            Allocate_Zeros_Array(arena, Page, state.game_map.building_pages_total);

        Place_Building(state, {2, 2}, 1);
        Place_Building(state, {4, 2}, 2);

        {
            On_Item_Built__Function((*callbacks[])) = {
                Renderer__On_Item_Built,
            };
            INITIALIZE_OBSERVER_WITH_CALLBACKS(state.On_Item_Built, callbacks, arena);
        }

        memory.is_initialized = true;
    }

    // TODO(hulvdan): Оно ругается на null-pointer dereference. Если так написать, норм?
    if (state.renderer_state != nullptr)
        state.renderer_state->bitmap = &bitmap;
    else
        UNREACHABLE;

    Process_Events(memory, (u8*)input_events_bytes_ptr, input_events_count, dt);

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

    Render(state, dt);
}
