#include "imgui.h"
#include "imgui_impl_opengl3.h"
#include "imgui_impl_win32.h"
#ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#endif

#include <cstdlib>
#include <source_location>
#include <memory>
#include <concepts>

#include "glew.h"
#include "wglew.h"

#include "bf_base.h"
#include "bf_game.h"

Library_Integration_Data* global_library_integration_data = nullptr;

#include <optional>

#include "bf_atlas_generated.h"
#include "bf_gamelib_generated.h"

// NOLINTBEGIN(bugprone-suspicious-include)
#include "bf_math.cpp"
#include "bf_memory_arena.cpp"
#include "bf_opengl.cpp"
#include "bf_rand.cpp"
#include "bf_file.cpp"
#include "bf_log.cpp"
#include "bf_memory.cpp"
#include "bf_containers.cpp"
#include "bf_game_types.cpp"
#include "bf_strings.cpp"
#include "bf_hash.cpp"
#include "bf_game_map.cpp"

#ifdef BF_CLIENT
#    include "bfc_tilemap.cpp"
#    include "bfc_renderer.cpp"
#endif  // BF_CLIENT

#if BF_SERVER
#endif  // BF_SERVER
// NOLINTEND(bugprone-suspicious-include)

bool UI_Clicked(Game_State& state) {
    auto& game_map = state.game_map;
    auto& rstate   = *state.renderer_state;
    auto& ui_state = *rstate.ui_state;

    Game_Bitmap& bitmap = Assert_Deref(rstate.bitmap);

    const auto gsize   = game_map.size;
    const auto swidth  = (f32)bitmap.width;
    const auto sheight = (f32)bitmap.height;

    const auto  sprite_params = ui_state.buildables_panel_params;
    const auto& pad_h         = sprite_params.stretch_paddings_h;
    const auto& pad_v         = sprite_params.stretch_paddings_v;

    const auto& placeholder_texture
        = *Get_Texture(rstate.atlas, ui_state.buildables_placeholder_texture);

    const v2f psize = v2f(placeholder_texture.size);

    const v2f sprite_anchor = ui_state.buildables_panel_sprite_anchor;

    const v2f padding          = v2f(ui_state.padding);
    const f32 placeholders_gap = (f32)ui_state.placeholders_gap;
    const f32 placeholders     = (f32)ui_state.placeholders;
    const v2f panel_size       = v2f(
        psize.x + 2 * padding.x,
        2 * padding.y + placeholders_gap * (placeholders - 1) + placeholders * psize.y
    );

    const v2f  outer_anchor         = ui_state.buildables_panel_container_anchor;
    const v2i  outer_container_size = v2i(swidth, sheight);
    const auto outer_pos            = v2f(outer_container_size) * outer_anchor;

    auto VIEW = glm::mat3(1);
    VIEW      = glm::translate(VIEW, v2f((int)outer_pos.x, (int)outer_pos.y));
    VIEW      = glm::scale(VIEW, v2f(ui_state.scale));

    i32 clicked_buildable_index = -1;

    {
        auto MODEL = glm::mat3(1);
        MODEL      = glm::scale(MODEL, v2f(panel_size));

        v3f p0_local = MODEL * v3f(v2f_zero - sprite_anchor, 1);
        v3f p1_local = MODEL * v3f(v2f_one - sprite_anchor, 1);

        v3f origin = (p1_local + p0_local) / 2.0f;

        // Aligning items in a column
        // justify-content: center
        FOR_RANGE (i32, i, placeholders) {
            v3f drawing_point = origin;
            drawing_point.y -= (placeholders - 1) * (psize.y + placeholders_gap) / 2;
            drawing_point.y += (f32)i * (placeholders_gap + psize.y);

            v3f p   = VIEW * drawing_point;
            v3f s   = VIEW * v3f(psize, 0);
            v2f p2  = v2f(p) - v2f(s) * 0.5f;  // anchor
            v2f off = v2f(rstate.mouse_pos) - p2;
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
    Game_State& state,
    const u8*   events,
    size_t      input_events_count,
    float /* dt */,
    MCTX
) {
    auto& rstate   = *state.renderer_state;
    auto& ui_state = *rstate.ui_state;

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
                    if (ui_state.selected_buildable_index >= 0) {
                        Assert(
                            ui_state.selected_buildable_index < ui_state.buildables_count
                        );
                        auto& selected_buildable
                            = *(ui_state.buildables + ui_state.selected_buildable_index);

                        auto tile_pos
                            = World_Pos_To_Tile(Screen_To_World(state, rstate.mouse_pos));
                        if (Pos_Is_In_Bounds(tile_pos, state.game_map.size)) {
                            switch (selected_buildable.type) {
                            case Item_To_Build_Type::Road: {
                                Try_Build(state, tile_pos, Item_To_Build_Flag, ctx);
                                Try_Build(state, tile_pos, Item_To_Build_Road, ctx);
                            } break;

                            case Item_To_Build_Type::Flag: {
                                Try_Build(state, tile_pos, Item_To_Build_Flag, ctx);
                            } break;

                            case Item_To_Build_Type::Building: {
                                Try_Build(state, tile_pos, selected_buildable, ctx);
                            } break;

                            default:
                                INVALID_PATH;
                            }
                        }
                    }
                }
                else if (event.type == Mouse_Button_Type::Right) {
                    rstate.panning       = true;
                    rstate.pan_start_pos = event.position;
                    rstate.pan_offset    = v2i(0, 0);
                }
            }
        } break;

        case Event_Type::Mouse_Released: {
            PROCESS_EVENTS_CONSUME(Mouse_Released, event);

            rstate.mouse_pos = event.position;

            if (event.type == Mouse_Button_Type::Left) {
            }
            else if (event.type == Mouse_Button_Type::Right) {
                rstate.panning    = false;
                rstate.pan_pos    = (v2i)rstate.pan_pos + (v2i)rstate.pan_offset;
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
            }
            else if (event.value < 0) {
                rstate.zoom_target /= 2.0f;
            }
            else {
                INVALID_PATH;
            }

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
            Assert(event.axis >= 0);
            Assert(event.axis <= 1);

            if (event.axis == 0)
                state.player_pos.x += event.value;
            else
                state.player_pos.y -= event.value;
        } break;

        default:
            // TODO: Diagnostic
            INVALID_PATH;
        }
    }
}

void Map_Arena(Arena& root_arena, Arena& arena_to_map, size_t size) {
    arena_to_map.base = Allocate_Array(root_arena, u8, size);
    arena_to_map.size = size;
}

void Reset_Arena(Arena& arena) {
    memset(arena.base, 0, arena.size);
    arena.used = 0;
}

template <typename T>
T* Map_Logger(Arena& arena, bool first_time_initializing) {
    if (std::is_same_v<T, void*>)
        return nullptr;

    T* logger = Allocate_For(arena, T);
    if (first_time_initializing) {
        MAKE_LOGGER(logger, arena);
    }

    return logger;
}

const BFGame::Game_Library* Load_Game_Library(Arena& arena) {
    auto result = Debug_Load_File_To_Arena("gamelib.bin", arena);
    Assert(result.success);
    return BFGame::GetGame_Library(result.output);
}

On_Item_Built_function(On_Item_Built) {
#ifdef BF_CLIENT
    Renderer_OnItemBuilt(state, pos, item, ctx);
#endif
}

On_Human_Created_function(On_Human_Created) {
#ifdef BF_CLIENT
    Renderer_OnHumanCreated(state, id, human, ctx);
#endif
}

On_Human_Removed_function(On_Human_Removed) {
#ifdef BF_CLIENT
    Renderer_OnHumanRemoved(state, id, human, reason, ctx);
#endif
}

extern "C" GAME_LIBRARY_EXPORT Game_Update_And_Render_function(Game_Update_And_Render) {
    ZoneScoped;
    global_library_integration_data = &library_integration_data;

    const float MAX_DT = 1.0f / (f32)10;
    if (dt > MAX_DT)
        dt = MAX_DT;

    Arena root_arena{};
    root_arena.debug_name = "root_arena";
    root_arena.base       = (u8*)memory_ptr;
    root_arena.size       = memory_size;
    root_arena.used       = 0;

    auto& memory = *Allocate_For(root_arena, Game_Memory);

    Game_State& state  = memory.state;
    state.hot_reloaded = hot_reloaded;

    auto first_time_initializing = !memory.is_initialized;

    root_logger = Map_Logger<Root_Logger_Type>(root_arena, first_time_initializing);

    if (first_time_initializing) {
        root_allocator = Allocate_For(root_arena, Root_Allocator_Type);
        std::construct_at(root_allocator);
    }

    Context _ctx(
        0,
        nullptr,
        nullptr,
        (root_logger != nullptr) ? Tracing_Logger_Routine : nullptr,
        (root_logger != nullptr) ? Tracing_Logger_Tracing_Routine : nullptr,
        root_logger
    );
    auto ctx = &_ctx;

    if (!library_integration_data.game_context_set) {
        ImGui::SetCurrentContext(library_integration_data.imgui_context);
        library_integration_data.game_context_set = true;
    }

    auto& editor_data = state.editor_data;

    // --- IMGUI ---
    if (first_time_initializing)
        editor_data = Default_Editor_Data();

    if (!first_time_initializing) {
        auto& rstate = Assert_Deref(state.renderer_state);
        ImGui::Text("Mouse %d.%d", rstate.mouse_pos.x, rstate.mouse_pos.y);

        if (ImGui::SliderInt(
                "Terrain Octaves", &editor_data.terrain_perlin.octaves, 1, 9
            ))
        {
            editor_data.changed = true;
        }
        if (ImGui::SliderFloat(
                "Terrain Scaling Bias",
                &editor_data.terrain_perlin.scaling_bias,
                0.001f,
                2.0f
            ))
        {
            editor_data.changed = true;
        }
        if (ImGui::Button("New Terrain Seed")) {
            editor_data.changed = true;
            editor_data.terrain_perlin.seed++;
        }
        if (ImGui::SliderInt(
                "Terrain Max Height", &editor_data.terrain_max_height, 1, 35
            ))
        {
            editor_data.changed = true;
        }

        if (ImGui::SliderInt("Forest Octaves", &editor_data.forest_perlin.octaves, 1, 9))
        {
            editor_data.changed = true;
        }
        if (ImGui::SliderFloat(
                "Forest Scaling Bias",
                &editor_data.forest_perlin.scaling_bias,
                0.001f,
                2.0f
            ))
        {
            editor_data.changed = true;
        }
        if (ImGui::Button("New Forest Seed")) {
            editor_data.changed = true;
            editor_data.forest_perlin.seed++;
        }
        if (ImGui::SliderFloat(
                "Forest Threshold", &editor_data.forest_threshold, 0.0f, 1.0f
            ))
        {
            editor_data.changed = true;
        }
        if (ImGui::SliderInt("Forest MaxAmount", &editor_data.forest_max_amount, 1, 35)) {
            editor_data.changed = true;
        }
    }
    // --- IMGUI END ---

    if (!first_time_initializing && state.hot_reloaded) {
        Deinit_Game_Map(state, ctx);
    }

    if (first_time_initializing || editor_data.changed || state.hot_reloaded) {
        editor_data.changed = false;

        auto trash_arena_size          = Megabytes((size_t)1);
        auto non_persistent_arena_size = Megabytes((size_t)1);
        auto arena_size = root_arena.size - root_arena.used - non_persistent_arena_size
                          - trash_arena_size;

        // NOTE: `arena` remains the same after hot reloading. Others get reset
        auto& arena                = state.arena;
        auto& non_persistent_arena = state.non_persistent_arena;
        auto& trash_arena          = state.trash_arena;

        Map_Arena(root_arena, arena, arena_size);
        Map_Arena(root_arena, non_persistent_arena, non_persistent_arena_size);
        Map_Arena(root_arena, trash_arena, trash_arena_size);

        if (first_time_initializing)
            state.gamelib = Load_Game_Library(arena);

        Reset_Arena(non_persistent_arena);
        Reset_Arena(trash_arena);

        state.arena.debug_name          = "arena";
        non_persistent_arena.debug_name = Allocate_Formatted_String(
            non_persistent_arena, "non_persistent_arena_%d", state.dll_reloads_count
        );
        trash_arena.debug_name = Allocate_Formatted_String(
            non_persistent_arena, "trash_arena_%d", state.dll_reloads_count
        );

        Initialize_As_Zeros<Game_Map>(state.game_map);
        state.game_map.size = {32, 24};
        auto& gsize         = state.game_map.size;

        auto tiles_count = (size_t)gsize.x * gsize.y;
        state.game_map.terrain_tiles
            = Allocate_Zeros_Array(non_persistent_arena, Terrain_Tile, tiles_count);
        state.game_map.terrain_resources
            = Allocate_Zeros_Array(non_persistent_arena, Terrain_Resource, tiles_count);
        state.game_map.element_tiles
            = Allocate_Zeros_Array(non_persistent_arena, Element_Tile, tiles_count);

        if (first_time_initializing) {
            auto resources = state.gamelib->resources();

            auto new_resources
                = Allocate_Array(arena, Scriptable_Resource, resources->size());

            FOR_RANGE (int, i, resources->size()) {
                auto  resource     = resources->Get(i);
                auto& new_resource = new_resources[i];
                new_resource.code  = resource->code()->c_str();
            }

            state.scriptable_resources_count = resources->size();
            state.scriptable_resources       = new_resources;
        }

        auto s = state.gamelib->buildings()->size();
        state.scriptable_buildings
            = Allocate_Zeros_Array(non_persistent_arena, Scriptable_Building, s);
        state.scriptable_buildings_count = s;
        FOR_RANGE (int, i, s) {
            auto& building    = state.scriptable_buildings[i];
            auto& libbuilding = *state.gamelib->buildings()->Get(i);

            building.code                 = libbuilding.code()->c_str();
            building.type                 = (Building_Type)libbuilding.type();
            building.harvestable_resource = nullptr;

            building.human_spawning_delay  //
                = libbuilding.human_spawning_delay();
            building.required_construction_points  //
                = libbuilding.required_construction_points();

            building.can_be_built = libbuilding.can_be_built();

            Init_Vector(building.construction_resources, ctx);

            if (libbuilding.construction_resources() != nullptr) {
                FOR_RANGE (int, i, libbuilding.construction_resources()->size()) {
                    auto resource = libbuilding.construction_resources()->Get(i);
                    auto code     = resource->resource_code()->c_str();
                    auto count    = resource->count();

                    Scriptable_Resource* scriptable_resource = nullptr;
                    FOR_RANGE (int, k, state.scriptable_resources_count) {
                        auto res = state.scriptable_resources + k;
                        if (strcmp(res->code, code) == 0) {
                            scriptable_resource = res;
                            break;
                        }
                    }

                    Assert(scriptable_resource != nullptr);
                    *building.construction_resources.Vector_Occupy_Slot(ctx)
                        = {scriptable_resource, count};
                }
            }
        }

        // {
        //     auto& b                        = Assert_Deref(state.scriptable_buildings +
        //     0); b.code                         = "City Hall"; b.type =
        //     Building_Type::City_Hall; b.human_spawning_delay         = 1;
        //     b.required_construction_points = 0;
        //     state.scriptable_building_city_hall = &b;
        //     Init_Vector(b.construction_resources, ctx);
        // }
        // {
        //     auto& b                = Assert_Deref(state.scriptable_buildings + 1);
        //     b.code                 = "Lumberjack's Hut";
        //     b.type                 = Building_Type::Harvest;
        //     b.human_spawning_delay = 0;
        //     state.scriptable_building_lumberjacks_hut = &b;
        //     b.required_construction_points            = 10;
        //
        //     Scriptable_Resource* plank_scriptable_resource =
        //     state.scriptable_resources; Init_Vector(b.construction_resources, ctx);
        //     Vector_Add(
        //         b.construction_resources,
        //         std::tuple(plank_scriptable_resource, (i16)2),
        //         ctx
        //     );
        // }

        Init_Game_Map(
            first_time_initializing, state.hot_reloaded, state, non_persistent_arena, ctx
        );
        Init_Renderer(
            first_time_initializing,
            state.hot_reloaded,
            state,
            arena,
            non_persistent_arena,
            trash_arena,
            ctx
        );

        Regenerate_Terrain_Tiles(
            state, state.game_map, non_persistent_arena, trash_arena, 0, editor_data, ctx
        );
        Regenerate_Element_Tiles(
            state, state.game_map, non_persistent_arena, trash_arena, 0, editor_data, ctx
        );

        Post_Init_Game_Map(
            first_time_initializing, state.hot_reloaded, state, non_persistent_arena, ctx
        );
        Post_Init_Renderer(
            first_time_initializing,
            state.hot_reloaded,
            state,
            arena,
            non_persistent_arena,
            trash_arena,
            ctx
        );

        memory.is_initialized = true;
    }

    if (state.renderer_state != nullptr
        && state.renderer_state->shaders_compilation_failed)
        ImGui::Text("ERROR: Shaders compilation failed!");

    // TODO: Оно ругается на null-pointer dereference. Если так написать, норм?
    if (state.renderer_state != nullptr)
        state.renderer_state->bitmap = &bitmap;
    else
        INVALID_PATH;

    Assert(bitmap.bits_per_pixel == 32);
    auto pixel = (u32*)bitmap.memory;

    auto offset_x = (i32)state.offset_x;
    auto offset_y = (i32)state.offset_y;

    state.offset_y += dt * 24.0f;
    state.offset_x += dt * 32.0f;

    const auto player_radius = 24;

    FOR_RANGE (i32, y, bitmap.height) {
        FOR_RANGE (i32, x, bitmap.width) {
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
                red                     = player_color;
                green                   = player_color;
                blue                    = player_color;
            }

            (*pixel++) = (blue << 0) | (green << 8) | (red << 0);
        }
    }

    auto& trash_arena = state.trash_arena;
    TEMP_USAGE(trash_arena);

    Process_Events(state, (u8*)input_events_bytes_ptr, input_events_count, dt, ctx);
    Update_Game_Map(state, dt, ctx);
    Render(state, dt, ctx);
}
