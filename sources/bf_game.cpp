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

bool UI_Clicked(Game_State& state) {
    auto& game_map = state.game_map;
    auto& rstate = *state.renderer_state;
    auto& ui_state = *rstate.ui_state;

    assert(rstate.bitmap != nullptr);
    Game_Bitmap& bitmap = *rstate.bitmap;

    auto gsize = game_map.size;
    auto swidth = (f32)bitmap.width;
    auto sheight = (f32)bitmap.height;

    auto sprite_params = ui_state.buildables_panel_params;
    auto& pad_h = sprite_params.stretch_paddings_h;
    auto& pad_v = sprite_params.stretch_paddings_v;

    auto texture = ui_state.buildables_panel_background;
    auto placeholder_texture = ui_state.buildables_placeholder_background;
    auto& psize = placeholder_texture.size;

    auto scale = ui_state.scale;
    v2f sprite_anchor = ui_state.buildables_panel_sprite_anchor;

    v2f padding = ui_state.padding;
    f32 placeholders_gap = ui_state.placeholders_gap;
    auto placeholders = ui_state.placeholders;
    auto panel_size =
        v2f(psize.x + 2 * padding.x,
            2 * padding.y + placeholders_gap * (placeholders - 1) + placeholders * psize.y);

    auto outer_anchor = ui_state.buildables_panel_container_anchor;
    auto outer_container_size = v2i(swidth, sheight);
    auto outer_x = outer_container_size.x * outer_anchor.x;
    auto outer_y = outer_container_size.y * outer_anchor.y;

    auto projection = glm::mat3(1);
    // projection = glm::translate(projection, v2f(0, 1));
    // projection = glm::scale(projection, v2f(1 / swidth, -1 / sheight));
    projection = glm::translate(projection, v2f((int)outer_x, (int)outer_y));
    projection = glm::scale(projection, v2f(scale));

    i8 clicked_buildable_index = -1;

    {
        auto model = glm::mat3(1);
        model = glm::scale(model, v2f(panel_size));
        auto p0_local = model * v3f(v2f_zero - sprite_anchor, 1);
        auto p1_local = model * v3f(v2f_one - sprite_anchor, 1);
        auto origin = (p1_local + p0_local) / 2.0f;

        // Aligning items in a column
        // justify-content: center
        FOR_RANGE(i8, i, placeholders) {
            auto drawing_point = origin;
            drawing_point.y -= (placeholders - 1) * (psize.y + placeholders_gap) / 2;
            drawing_point.y += i * (placeholders_gap + psize.y);

            auto p = projection * drawing_point;
            auto s = projection * v3f(psize, 0);
            auto p2 = v2f(p) - v2f(s) * 0.5f;  // anchor
            auto off = v2f(rstate.mouse_pos) - p2;
            if (Pos_Is_In_Bounds(off, s)) {
                clicked_buildable_index = i;
                break;
            }
        }
    }

    if (clicked_buildable_index != -1)
        ui_state.selected_buildable_index = clicked_buildable_index;

    return clicked_buildable_index != -1;
}

#define PROCESS_EVENTS_CONSUME(event_type_, variable_name_) \
    event_type_ variable_name_ = {};                        \
    memcpy(&(variable_name_), events, sizeof(event_type_)); \
    events += sizeof(event_type_);

void Process_Events(
    Game_Memory& memory,
    const u8* events,
    size_t input_events_count,
    float dt  //
) {
    assert(memory.is_initialized);
    auto& state = memory.state;
    auto& rstate = *state.renderer_state;

    while (input_events_count > 0) {
        input_events_count--;
        auto type = (Event_Type)*events;
        events++;

        switch (type) {
        case Event_Type::Mouse_Pressed: {
            PROCESS_EVENTS_CONSUME(Mouse_Pressed, event);

            rstate.mouse_pos = event.position;

            if (!UI_Clicked(state)) {
                if (event.type == Mouse_Button_Type::Left) {
                    auto tile_pos = World_Pos_To_Tile(Screen_To_World(state, rstate.mouse_pos));
                    if (Pos_Is_In_Bounds(tile_pos, state.game_map.size)) {
                        Try_Build(state, tile_pos, Item_To_Build_Flag);
                        Try_Build(state, tile_pos, Item_To_Build_Road);
                    }
                } else if (event.type == Mouse_Button_Type::Right) {
                    rstate.panning = true;
                    rstate.pan_start_pos = event.position;
                    rstate.pan_offset = v2i(0, 0);
                }
            }
        } break;

        case Event_Type::Mouse_Released: {
            PROCESS_EVENTS_CONSUME(Mouse_Released, event);

            rstate.mouse_pos = event.position;

            if (event.type == Mouse_Button_Type::Left) {
            } else if (event.type == Mouse_Button_Type::Right) {
                rstate.panning = false;
                rstate.pan_pos = (v2i)rstate.pan_pos + (v2i)rstate.pan_offset;
                rstate.pan_offset = v2i(0, 0);
            }
        } break;

        case Event_Type::Mouse_Moved: {
            PROCESS_EVENTS_CONSUME(Mouse_Moved, event);

            if (rstate.panning)
                rstate.pan_offset = event.position - rstate.pan_start_pos;

            rstate.mouse_pos = event.position;
        } break;

        case Event_Type::Mouse_Scrolled: {
            PROCESS_EVENTS_CONSUME(Mouse_Scrolled, event);

            if (event.value > 0) {
                rstate.zoom_target *= 2.0f;
            } else if (event.value < 0) {
                rstate.zoom_target /= 2.0f;
            } else
                assert(false);

            rstate.zoom_target = MAX(0.5f, MIN(8.0f, rstate.zoom_target));
        } break;

        case Event_Type::Keyboard_Pressed: {
            PROCESS_EVENTS_CONSUME(Keyboard_Pressed, event);
        } break;

        case Event_Type::Keyboard_Released: {
            PROCESS_EVENTS_CONSUME(Keyboard_Released, event);
        } break;

        case Event_Type::Controller_Button_Pressed: {
            PROCESS_EVENTS_CONSUME(Controller_Button_Pressed, event);
        } break;

        case Event_Type::Controller_Button_Released: {
            PROCESS_EVENTS_CONSUME(Controller_Button_Released, event);
        } break;

        case Event_Type::Controller_Axis_Changed: {
            PROCESS_EVENTS_CONSUME(Controller_Axis_Changed, event);

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

void Map_Arena(Arena& arena_where_to_map, Arena& arena_to_map, size_t size) {
    arena_to_map.base = Allocate_Array(arena_where_to_map, u8, size);
    arena_to_map.size = size;
}

void Reset_Arena(Arena& arena) {
    memset(arena.base, 0, arena.size);
    arena.used = 0;
}

extern "C" GAME_LIBRARY_EXPORT Game_Update_And_Render__Function(Game_Update_And_Render) {
    ZoneScoped;

    Arena root_arena = {};
    root_arena.base = (u8*)memory_ptr;
    root_arena.size = memory_size;
    root_arena.used = 0;

    auto& memory = *Allocate_For(root_arena, Game_Memory);
    auto& state = memory.state;
    state.hot_reloaded = hot_reloaded;
    if (hot_reloaded)
        BREAKPOINT;

    if (!editor_data.game_context_set) {
        ImGui::SetCurrentContext(editor_data.context);
        editor_data.game_context_set = true;
    }

    auto first_time_initializing = !memory.is_initialized;

    // --- IMGUI ---
    if (!first_time_initializing) {
        assert(state.renderer_state != nullptr);
        auto& rstate = *state.renderer_state;
        ImGui::Text("Mouse %d.%d", rstate.mouse_pos.x, rstate.mouse_pos.y);

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

    if (first_time_initializing || editor_data.changed || state.hot_reloaded) {
        state.os_data = &os_data;
        editor_data.changed = false;

        // state.offset_x = 0;
        // state.offset_y = 0;

        auto trash_arena_size = Megabytes((size_t)1);
        auto non_persistent_arena_size = Megabytes((size_t)1);
        auto arena_size =
            root_arena.size - root_arena.used - non_persistent_arena_size - trash_arena_size;

        // NOTE(hulvdan): `arena` remains the same on DLL reloads
        auto& arena = state.arena;
        Map_Arena(root_arena, arena, arena_size);

        auto& non_persistent_arena = state.non_persistent_arena;
        Map_Arena(root_arena, non_persistent_arena, non_persistent_arena_size);
        Reset_Arena(non_persistent_arena);

        auto& trash_arena = state.trash_arena;
        Map_Arena(root_arena, trash_arena, trash_arena_size);
        Reset_Arena(trash_arena);

        if (first_time_initializing) {
            auto pages_count_that_fit_2GB = (size_t)2 * 1024 * 1024 * 1024 / os_data.page_size;
            state.pages.base = Allocate_Zeros_Array(arena, Page, pages_count_that_fit_2GB);
            state.pages.in_use = Allocate_Zeros_Array(arena, bool, pages_count_that_fit_2GB);
            state.pages.total_count_cap = pages_count_that_fit_2GB;
        }

        Initialize_As_Zeros<Game_Map>(state.game_map);
        state.game_map.size = {32, 24};
        auto& gsize = state.game_map.size;

        auto tiles_count = (size_t)gsize.x * gsize.y;
        state.game_map.terrain_tiles =
            Allocate_Zeros_Array(non_persistent_arena, Terrain_Tile, tiles_count);
        state.game_map.terrain_resources =
            Allocate_Zeros_Array(non_persistent_arena, Terrain_Resource, tiles_count);
        state.game_map.element_tiles =
            Allocate_Zeros_Array(non_persistent_arena, Element_Tile, tiles_count);

        state.scriptable_resources_count = 1;
        state.scriptable_resources =
            Allocate_Zeros_Array(non_persistent_arena, Scriptable_Resource, 1);
        {
            auto r_ = Get_Scriptable_Resource(state, 1);
            assert(r_ != nullptr);
            auto r = *r_;

            r.name = "forest";
        }

        state.scriptable_buildings_count = 2;
        state.scriptable_buildings =
            Allocate_Zeros_Array(non_persistent_arena, Scriptable_Building, 2);
        {
            auto b_ = Get_Scriptable_Building(state, global_city_hall_building_id);
            assert(b_ != nullptr);
            auto b = *b_;

            b.name = "City Hall";
            b.type = Building_Type::City_Hall;
        }
        {
            auto b_ = Get_Scriptable_Building(state, global_lumberjacks_hut_building_id);
            assert(b_ != nullptr);
            auto b = *b_;

            b.name = "Lumberjack's Hut";
            b.type = Building_Type::Harvest;
        }

        Regenerate_Terrain_Tiles(
            state, state.game_map, non_persistent_arena, trash_arena, 0, editor_data);
        Regenerate_Element_Tiles(
            state, state.game_map, non_persistent_arena, trash_arena, 0, editor_data);

        if (first_time_initializing) {
            auto max_hypothetical_count_of_building_pages =
                Ceil_Division(tiles_count * sizeof(Building), os_data.page_size);

            assert(max_hypothetical_count_of_building_pages < 100);
            assert(max_hypothetical_count_of_building_pages > 0);

            state.game_map.building_pages_total = max_hypothetical_count_of_building_pages;
            state.game_map.building_pages_used = 0;
            state.game_map.building_pages =
                Allocate_Zeros_Array(arena, Page, state.game_map.building_pages_total);
            state.game_map.max_buildings_per_page = Assert_Truncate_To_u16(
                (os_data.page_size - sizeof(Building_Page_Meta)) / sizeof(Building));

            Place_Building(state, {2, 2}, 1);
            Place_Building(state, {4, 2}, 2);
            Place_Building(state, {5, 2}, 2);
            Place_Building(state, {6, 2}, 2);
            Place_Building(state, {4, 3}, 2);
            Place_Building(state, {4, 5}, 2);
            Place_Building(state, {4, 7}, 2);
        }

        On_Item_Built__Function((*callbacks[])) = {
            Renderer__On_Item_Built,
        };
        INITIALIZE_OBSERVER_WITH_CALLBACKS(state.On_Item_Built, callbacks, non_persistent_arena);

        Initialize_Renderer(state, arena, non_persistent_arena, trash_arena);

        memory.is_initialized = true;
    }

    // TODO(hulvdan): Оно ругается на null-pointer dereference. Если так написать, норм?
    if (state.renderer_state != nullptr)
        state.renderer_state->bitmap = &bitmap;
    else
        UNREACHABLE;

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

    Process_Events(memory, (u8*)input_events_bytes_ptr, input_events_count, dt);
    Render(state, dt);
}
