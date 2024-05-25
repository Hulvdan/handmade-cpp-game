#pragma once

#ifndef BF_CLIENT
#error "This code should run on a client! BF_CLIENT must be defined!"
#endif  // BF_CLIENT
#pragma pack(push, 1)
struct Debug_BMP_Header {
    u16 signature;
    u32 file_size;
    u32 reserved;
    u32 data_offset;

    u32 dib_header_size;
    u32 width;
    u32 height;
    u16 planes;
    u16 bits_per_pixel;
    // NOTE: Mapping things below this line is not technically correct.
    // Earlier versions of BMP Info Headers don't anything specified below
    u32 compression;
};
#pragma pack(pop)

const i32 global_road_starting_tile_id = 4;
const i32 global_flag_starting_tile_id = global_road_starting_tile_id + 16;

struct Load_BMP_RGBA_Result {
    bool success;
    u8*  output;
    u16  width;
    u16  height;
};

Load_BMP_RGBA_Result Load_BMP_RGBA(Arena& arena, const u8* filedata) {
    Load_BMP_RGBA_Result res = {};

    auto& header = *(Debug_BMP_Header*)filedata;

    if (header.signature != *(u16*)"BM") {
        // TODO: Diagnostic. Not a BMP file
        INVALID_PATH;
        return res;
    }

    auto dib_size = header.dib_header_size;
    if (dib_size != 125 && dib_size != 56) {
        // TODO: Is not yet implemented algorithm
        INVALID_PATH;
        return res;
    }

    auto pixels_count = (u32)header.width * header.height;
    auto total_bytes  = (size_t)pixels_count * 4;

    res.output = Allocate_Array(arena, u8, total_bytes);
    res.width  = header.width;
    res.height = header.height;

    Assert(header.planes == 1);
    Assert(header.bits_per_pixel == 32);
    Assert(header.file_size - header.data_offset == total_bytes);

    memcpy(res.output, filedata + header.data_offset, total_bytes);

    res.success = true;
    return res;
}

void Send_Texture_To_GPU(Loaded_Texture& texture) {
    glBindTexture(GL_TEXTURE_2D, texture.id);

    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    Check_OpenGL_Errors();

    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA8,
        texture.size.x,
        texture.size.y,
        0,
        GL_BGRA_EXT,
        GL_UNSIGNED_BYTE,
        texture.base
    );
    Check_OpenGL_Errors();
}

void DEBUG_Load_Texture(
    Arena&          destination_arena,
    Arena&          trash_arena,
    const char*     texture_name,
    Loaded_Texture& out_texture
) {
    char filepath[512];
    sprintf(filepath, "assets/art/%s.bmp", texture_name);

    Load_BMP_RGBA_Result bmp_result = {};
    {
        TEMP_USAGE(trash_arena);
        auto load_result = Debug_Load_File_To_Arena(filepath, trash_arena);
        Assert(load_result.success);

        bmp_result = Load_BMP_RGBA(destination_arena, load_result.output);
        Assert(bmp_result.success);
    }

    out_texture.id   = scast<BF_Texture_ID>(Hash32_String(texture_name));
    out_texture.size = {bmp_result.width, bmp_result.height};
    out_texture.base = bmp_result.output;
    Send_Texture_To_GPU(out_texture);
}

int Get_Road_Texture_Number(Element_Tile* element_tiles, v2i16 pos, v2i16 gsize) {
    Element_Tile& tile             = *(element_tiles + pos.y * gsize.x + pos.x);
    bool          tile_is_flag     = tile.type == Element_Tile_Type::Flag;
    bool          tile_is_road     = tile.type == Element_Tile_Type::Road;
    bool          tile_is_building = tile.type == Element_Tile_Type::Building;

    int road_texture_number = 0;
    FOR_RANGE (int, i, 4) {
        auto new_pos = pos + v2i16_adjacent_offsets[i];
        if (!Pos_Is_In_Bounds(new_pos, gsize))
            continue;

        Element_Tile& adjacent_tile = *(element_tiles + new_pos.y * gsize.x + new_pos.x);
        bool          adj_tile_is_flag = adjacent_tile.type == Element_Tile_Type::Flag;
        bool          adj_tile_is_road = adjacent_tile.type == Element_Tile_Type::Road;
        bool adj_tile_is_building = adjacent_tile.type == Element_Tile_Type::Building;

        bool should_connect = true;
        should_connect &= !(adj_tile_is_flag && tile_is_flag);
        should_connect &= !(adj_tile_is_building && tile_is_building);
        should_connect &= !(tile_is_building && adj_tile_is_flag);
        should_connect &= !(adj_tile_is_building && tile_is_flag);
        should_connect &= adj_tile_is_flag || adj_tile_is_building || adj_tile_is_road;

        road_texture_number += should_connect * (1 << i);
    }

    return road_texture_number;
}

#define Debug_Load_File(variable_name_, filepath_, arena_)                  \
    auto(variable_name_) = Debug_Load_File_To_Arena((filepath_), (arena_)); \
    Assert((variable_name_).success);

void Debug_Print_Shader_Info_Log(
    GLuint      shader_id,
    Arena&      trash_arena,
    const char* aboba
) {
    TEMP_USAGE(trash_arena);

    GLint succeeded;
    glGetShaderiv(shader_id, GL_COMPILE_STATUS, &succeeded);

    GLint log_length;
    glGetShaderiv(shader_id, GL_INFO_LOG_LENGTH, &log_length);

    GLchar  zero     = GLchar(0);
    GLchar* info_log = &zero;

    if (log_length) {
        info_log = Allocate_Array(trash_arena, GLchar, log_length);
        glGetShaderInfoLog(shader_id, log_length, nullptr, info_log);
    }

    if (succeeded)
        DEBUG_Print("INFO:\t%s succeeded: %s\n", aboba, info_log);
    else
        DEBUG_Error("ERROR:\t%s failed: %s\n", aboba, info_log);
}

void Initialize_Renderer(
    Game_State& state,
    Arena&      arena,
    Arena&      non_persistent_arena,
    Arena&      trash_arena
) {
    auto hot_reloaded            = state.hot_reloaded;
    auto first_time_initializing = state.renderer_state == nullptr;

    if (!first_time_initializing && !hot_reloaded)
        return;

    // NOTE: I could not find a way of initializing glew once
    // in win32 layer so that initializing here'd unnecessary.
    // However, I did not dig very deep into using GLEW as a dll.
    local_persist bool glew_was_initialized = false;
    if (!glew_was_initialized) {
        // glewExperimental = GL_TRUE;
        Assert(glewInit() == GLEW_OK);
        glew_was_initialized = true;
    }

    if (first_time_initializing)
        state.renderer_state = Allocate_Zeros_For(arena, Game_Renderer_State);

    auto& rstate   = Assert_Deref(state.renderer_state);
    auto& game_map = state.game_map;
    auto  gsize    = game_map.size;

    // NOTE: Reloading shaders
    {
        GLint fragment_success = 0;
        GLint vertex_success   = 0;
        GLint program_success  = 0;

        // Vertex Shader
        auto vertex_code = R"Shader(
#version 330 core

layout (location = 0) in vec3 a_pos;
layout (location = 1) in vec3 a_sprite_color;
layout (location = 2) in vec2 a_tex_coord;

out vec3 sprite_color;
out vec2 tex_coord;

void main() {
    gl_Position = vec4(a_pos.x * 2 - 1, 1 - a_pos.y * 2, 0, 1.0);
    sprite_color = a_sprite_color;
    tex_coord = a_tex_coord;
}
)Shader";

        auto vertex = glCreateShader(GL_VERTEX_SHADER);
        defer {
            glDeleteShader(vertex);
        };

        glShaderSource(vertex, 1, &vertex_code, nullptr);
        glCompileShader(vertex);
        glGetShaderiv(vertex, GL_COMPILE_STATUS, &vertex_success);
        Debug_Print_Shader_Info_Log(vertex, trash_arena, "Vertex shader compilation");

        // Similiar for Fragment Shader
        auto fragment_code = R"Shader(
#version 330 core
out vec4 frag_color;

in vec3 sprite_color;
in vec2 tex_coord;

uniform sampler2D ourTexture;

void main() {
    frag_color = texture(ourTexture, tex_coord) * vec4(sprite_color, 1);
}
)Shader";

        auto fragment = glCreateShader(GL_FRAGMENT_SHADER);
        defer {
            glDeleteShader(fragment);
        };

        glShaderSource(fragment, 1, &fragment_code, nullptr);
        glCompileShader(fragment);
        glGetShaderiv(fragment, GL_COMPILE_STATUS, &fragment_success);
        Debug_Print_Shader_Info_Log(fragment, trash_arena, "Fragment shader compilation");

        if (fragment_success && vertex_success) {
            TEMP_USAGE(trash_arena);

            // shader Program
            auto program_id = glCreateProgram();
            glAttachShader(program_id, vertex);
            glAttachShader(program_id, fragment);
            glLinkProgram(program_id);
            // print linking errors if any
            GLint log_length;
            glGetProgramiv(program_id, GL_INFO_LOG_LENGTH, &log_length);
            glGetProgramiv(program_id, GL_LINK_STATUS, &program_success);

            auto    zero     = GLchar(0);
            GLchar* info_log = &zero;
            if (log_length) {
                info_log = Allocate_Array(trash_arena, GLchar, log_length);
                glGetProgramInfoLog(program_id, log_length, nullptr, info_log);
            }

            if (program_success)
                DEBUG_Print("INFO:\tProgram compilation succeeded: %s\n", info_log);
            else
                DEBUG_Error("ERROR:\tProgram compilation failed: %s\n", info_log);

            if (rstate.ui_shader_program)
                glDeleteProgram(rstate.ui_shader_program);
            rstate.ui_shader_program          = program_id;
            rstate.shaders_compilation_failed = false;
        }
        else {
            rstate.shaders_compilation_failed = true;
        }
    }

    glEnable(GL_TEXTURE_2D);

    rstate.grass_smart_tile.id  = 1;
    rstate.forest_smart_tile.id = 2;
    rstate.forest_top_tile_id   = 3;

    {
        DEBUG_Load_Texture(
            non_persistent_arena, trash_arena, "human", rstate.human_texture
        );

        char texture_name[512] = {};
        FOR_RANGE (int, i, 17) {
            sprintf(texture_name, "tiles/grass_%d", i);
            DEBUG_Load_Texture(
                non_persistent_arena, trash_arena, texture_name, rstate.grass_textures[i]
            );
        }

        FOR_RANGE (int, i, 3) {
            sprintf(texture_name, "tiles/forest_%d", i);
            DEBUG_Load_Texture(
                non_persistent_arena, trash_arena, texture_name, rstate.forest_textures[i]
            );
        }

        FOR_RANGE (int, i, 16) {
            sprintf(texture_name, "tiles/road_%d", i);
            DEBUG_Load_Texture(
                non_persistent_arena, trash_arena, texture_name, rstate.road_textures[i]
            );
        }

        FOR_RANGE (int, i, 4) {
            sprintf(texture_name, "tiles/flag_%d", i);
            DEBUG_Load_Texture(
                non_persistent_arena, trash_arena, texture_name, rstate.flag_textures[i]
            );
        }

        {
            sprintf(texture_name, "tiles/building_in_progress");
            DEBUG_Load_Texture(
                non_persistent_arena,
                trash_arena,
                texture_name,
                rstate.in_progress_building_texture
            );
        }

        {
            TEMP_USAGE(trash_arena);

            auto path = "assets/art/tiles/tilerule_grass.txt";
            Debug_Load_File(load_result, path, trash_arena);

            auto rule_loading_result = Load_Smart_Tile_Rules(
                rstate.grass_smart_tile,
                non_persistent_arena,
                load_result.output,
                load_result.size
            );
            Assert(rule_loading_result.success);
        }

        {
            TEMP_USAGE(trash_arena);

            auto path = "assets/art/tiles/tilerule_forest.txt";
            Debug_Load_File(load_result, path, trash_arena);

            auto rule_loading_result = Load_Smart_Tile_Rules(
                rstate.forest_smart_tile,
                non_persistent_arena,
                load_result.output,
                load_result.size
            );
            Assert(rule_loading_result.success);
        }

        Assert(state.gamelib->resources()->size() == state.scriptable_resources_count);

        FOR_RANGE (int, i, state.scriptable_resources_count) {
            const auto& libresource = *(*state.gamelib->resources())[i];

            auto& resource         = state.scriptable_resources[i];
            resource.texture       = Allocate_For(arena, Loaded_Texture);
            resource.small_texture = Allocate_For(arena, Loaded_Texture);
            DEBUG_Load_Texture(
                non_persistent_arena,
                trash_arena,
                libresource.texture()->c_str(),
                *resource.texture
            );
            DEBUG_Load_Texture(
                non_persistent_arena,
                trash_arena,
                libresource.small_texture()->c_str(),
                *resource.small_texture
            );
        }

        FOR_RANGE (int, i, state.scriptable_buildings_count) {
            const auto& libbuilding = *(*state.gamelib->buildings())[i];

            auto& building   = state.scriptable_buildings[i];
            building.texture = Allocate_For(arena, Loaded_Texture);
            DEBUG_Load_Texture(
                non_persistent_arena,
                trash_arena,
                libbuilding.texture()->c_str(),
                *building.texture
            );
        }
    }

    i32 max_height = 0;
    FOR_RANGE (i32, y, gsize.y) {
        FOR_RANGE (i32, x, gsize.x) {
            auto& terrain_tile = *(game_map.terrain_tiles + y * gsize.x + x);
            max_height         = MAX(max_height, terrain_tile.height);
        }
    }

    rstate.tilemaps_count = 0;
    // NOTE: Terrain tilemaps ([0; max_height])
    rstate.terrain_tilemaps_count = max_height + 1;
    rstate.tilemaps_count += rstate.terrain_tilemaps_count;

    // NOTE: Terrain Resources (forests, stones, etc.)
    // Currently use 2 tilemaps:
    // 1) Forests
    // 2) Tree's top tiles
    rstate.resources_tilemap_index = rstate.tilemaps_count;
    rstate.tilemaps_count += 2;

    // NOTE: Element Tiles (roads, buildings, etc.)
    // Currently use 2 tilemaps:
    // 1) Roads
    // 2) Flags above roads
    rstate.element_tilemap_index = rstate.tilemaps_count;
    rstate.tilemaps_count += 2;

    rstate.tilemaps
        = Allocate_Array(non_persistent_arena, Tilemap, rstate.tilemaps_count);

    FOR_RANGE (i32, h, rstate.tilemaps_count) {
        auto& tilemap = *(rstate.tilemaps + h);
        tilemap.tiles = Allocate_Array(non_persistent_arena, Tile_ID, gsize.x * gsize.y);

        FOR_RANGE (i32, y, gsize.y) {
            FOR_RANGE (i32, x, gsize.x) {
                auto& tile         = *(game_map.terrain_tiles + y * gsize.x + x);
                auto& tilemap_tile = *(tilemap.tiles + y * gsize.x + x);

                bool grass   = tile.terrain == Terrain::Grass && tile.height >= h;
                tilemap_tile = grass * rstate.grass_smart_tile.id;
            }
        }
    }

    auto& resources_tilemap  = rstate.tilemaps[rstate.resources_tilemap_index];
    auto& resources_tilemap2 = rstate.tilemaps[rstate.resources_tilemap_index + 1];
    FOR_RANGE (i32, y, gsize.y) {
        auto is_last_row = y == gsize.y - 1;
        FOR_RANGE (i32, x, gsize.x) {
            auto& resource = *(game_map.terrain_resources + y * gsize.x + x);

            auto& tile = *(resources_tilemap.tiles + y * gsize.x + x);
            if (tile)
                continue;

            bool forest = resource.amount > 0;
            if (!forest)
                continue;

            tile = rstate.forest_smart_tile.id;

            bool forest_above = false;
            if (!is_last_row) {
                auto& tile_above = *(resources_tilemap.tiles + (y + 1) * gsize.x + x);
                forest_above     = tile_above == rstate.forest_smart_tile.id;
            }

            if (!forest_above)
                *(resources_tilemap2.tiles + y * gsize.x + x) = rstate.forest_top_tile_id;
        }
    }

    // --- Element Tiles ---
    auto& element_tilemap = rstate.tilemaps[rstate.element_tilemap_index];
    FOR_RANGE (i32, y, gsize.y) {
        FOR_RANGE (i32, x, gsize.x) {
            Element_Tile& element_tile = *(game_map.element_tiles + y * gsize.x + x);
            if (element_tile.type != Element_Tile_Type::Road)
                continue;

            auto tex
                = Get_Road_Texture_Number(game_map.element_tiles, v2i16(x, y), gsize);
            auto& tile_id = *(element_tilemap.tiles + y * gsize.x + x);
            tile_id       = global_road_starting_tile_id + tex;
        }
    }
    // --- Element Tiles End ---

    rstate.zoom        = 1;
    rstate.zoom_target = 1;
    rstate.cell_size   = 32;

    rstate.ui_state = Allocate_Zeros_For(non_persistent_arena, Game_UI_State);
    auto& ui_state  = *rstate.ui_state;

    ui_state.buildables_panel_params.smart_stretchable  = true;
    ui_state.buildables_panel_params.stretch_paddings_h = {6, 6};
    ui_state.buildables_panel_params.stretch_paddings_v = {5, 6};
    DEBUG_Load_Texture(
        arena,
        non_persistent_arena,
        "ui/buildables_panel",
        ui_state.buildables_panel_background
    );
    DEBUG_Load_Texture(
        arena,
        non_persistent_arena,
        "ui/buildables_placeholder",
        ui_state.buildables_placeholder_background
    );

    int buildable_buildings_count = 0;
    FOR_RANGE (int, i, state.scriptable_buildings_count) {
        if (state.scriptable_buildings[i].can_be_built) {
            buildable_buildings_count++;
        }
    }

    ui_state.buildables_count = 1 + buildable_buildings_count;
    ui_state.buildables = Allocate_Array(arena, Item_To_Build, ui_state.buildables_count);
    ui_state.buildables[0].type                = Item_To_Build_Type::Road;
    ui_state.buildables[0].scriptable_building = nullptr;

    auto ii = 1;
    FOR_RANGE (int, i, state.scriptable_buildings_count) {
        auto scriptable_building = state.scriptable_buildings + i;
        if (scriptable_building->can_be_built) {
            auto& buildable = ui_state.buildables[ii];

            buildable.type                = Item_To_Build_Type::Building;
            buildable.scriptable_building = scriptable_building;
            ii++;
        }
    }

    ui_state.padding                           = {6, 6};
    ui_state.placeholders                      = ui_state.buildables_count;
    ui_state.placeholders_gap                  = 4;
    ui_state.selected_buildable_index          = -1;
    ui_state.buildables_panel_sprite_anchor    = {0.0f, 0.5f};
    ui_state.scale                             = 3;
    ui_state.buildables_panel_in_scale         = 1;
    ui_state.buildables_panel_container_anchor = {0.0f, 0.5f};

    ui_state.not_selected_buildable_color.r = 255.0f / 255.0f;
    ui_state.not_selected_buildable_color.g = 255.0f / 255.0f;
    ui_state.not_selected_buildable_color.b = 255.0f / 255.0f;
    ui_state.selected_buildable_color.r     = 255.0f / 255.0f;
    ui_state.selected_buildable_color.g     = 233.0f / 255.0f;
    ui_state.selected_buildable_color.b     = 176.0f / 255.0f;
}

void Draw_Sprite(
    f32              x0,
    f32              y0,
    f32              x1,
    f32              y1,
    v2f              pos,
    v2f              size,
    float            rotation,
    const glm::mat3& projection
) {
    Assert(x0 < x1);
    Assert(y0 < y1);

    auto model = glm::mat3(1);
    model      = glm::translate(model, pos);
    // model = glm::translate(model, pos / size);
    model = glm::rotate(model, rotation);
    model = glm::scale(model, size);

    auto matrix = projection * model;
    // TODO: How bad is it that there are vec3, but not vec2?
    v3f vertices[] = {{0, 0, 1}, {0, 1, 1}, {1, 0, 1}, {1, 1, 1}};
    for (auto& vertex : vertices) {
        vertex = matrix * vertex;
        // Converting from homogeneous to eucledian
        // vertex.x = vertex.x / vertex.z;
        // vertex.y = vertex.y / vertex.z;
    }

    f32 texture_vertices[] = {x0, y0, x0, y1, x1, y0, x1, y1};
    for (ptrd i : {0, 1, 2, 2, 1, 3}) {
        // TODO: How bad is that there are 2 vertices duplicated?
        glTexCoord2f(texture_vertices[2 * i], texture_vertices[2 * i + 1]);
        glVertex2f(vertices[i].x, vertices[i].y);
    }
};

void Draw_UI_Sprite(
    f32                  x0,
    f32                  y0,
    f32                  x1,
    f32                  y1,
    v2f                  pos,
    v2f                  size,
    v2f                  anchor,
    BF_Color             color,
    Game_Renderer_State& rstate
) {
    Assert(x0 < x1);
    Assert(y0 < y1);

    f32 xx0 = pos.x + size.x * (0 - anchor.x);
    f32 xx1 = pos.x + size.x * (1 - anchor.x);
    f32 yy0 = pos.y + size.y * (0 - anchor.y);
    f32 yy1 = pos.y + size.y * (1 - anchor.y);

    f32 verticesss[] = {
        xx0, yy0, 0, color.r, color.g, color.b, x0, y0,  //
        xx1, yy0, 0, color.r, color.g, color.b, x1, y0,  //
        xx0, yy1, 0, color.r, color.g, color.b, x0, y1,  //
        xx0, yy1, 0, color.r, color.g, color.b, x0, y1,  //
        xx1, yy0, 0, color.r, color.g, color.b, x1, y0,  //
        xx1, yy1, 0, color.r, color.g, color.b, x1, y1,  //
    };

    GLuint vao;
    glGenVertexArrays(1, &vao);
    GLuint vbo;
    glGenBuffers(1, &vbo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verticesss), verticesss, GL_STATIC_DRAW);

    // 3. then set our vertex attributes pointers
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(f32), rcast<void*>(0));
    glVertexAttribPointer(
        1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(f32), rcast<void*>(3 * sizeof(f32))
    );
    glVertexAttribPointer(
        2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(f32), rcast<void*>(6 * sizeof(f32))
    );
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);

    // ..:: Drawing code (in render loop) :: ..
    glUseProgram(rstate.ui_shader_program);
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    glBindVertexArray(0);
    glUseProgram(0);
    glDeleteVertexArrays(1, &vao);
};

v2f World_To_Screen(Game_State& state, v2f pos) {
    auto&        rstate = Assert_Deref(state.renderer_state);
    Game_Bitmap& bitmap = Assert_Deref(rstate.bitmap);

    auto swidth    = (f32)bitmap.width;
    auto sheight   = (f32)bitmap.height;
    auto gsize     = state.game_map.size;
    auto cell_size = rstate.cell_size;

    auto projection     = glm::mat3(1);
    projection          = glm::translate(projection, rstate.pan_pos + rstate.pan_offset);
    projection          = glm::scale(projection, v2f(rstate.zoom, rstate.zoom));
    projection          = glm::translate(projection, v2f(swidth, sheight) / 2.0f);
    projection          = glm::translate(projection, -(v2f)gsize * cell_size / 2.0f);
    projection          = glm::scale(projection, v2f(cell_size, cell_size));
    auto projection_inv = glm::inverse(projection);

    return projection * v3f(pos.x, pos.y, 1);
}

v2f Screen_To_World(Game_State& state, v2f pos) {
    auto&        rstate = Assert_Deref(state.renderer_state);
    Game_Bitmap& bitmap = Assert_Deref(rstate.bitmap);

    auto swidth    = (f32)bitmap.width;
    auto sheight   = (f32)bitmap.height;
    auto gsize     = state.game_map.size;
    auto cell_size = rstate.cell_size;

    auto projection     = glm::mat3(1);
    projection          = glm::translate(projection, rstate.pan_pos + rstate.pan_offset);
    projection          = glm::scale(projection, v2f(rstate.zoom, rstate.zoom));
    projection          = glm::translate(projection, v2f(swidth, sheight) / 2.0f);
    projection          = glm::translate(projection, -(v2f)gsize * (f32)cell_size / 2.0f);
    projection          = glm::scale(projection, v2f(cell_size, cell_size));
    auto projection_inv = glm::inverse(projection);

    return projection_inv * v3f(pos.x, pos.y, 1);
}

v2i16 World_Pos_To_Tile(v2f pos) {
    auto x = (int)(pos.x);
    auto y = (int)(pos.y);
    // if (pos.x < 0.5f)
    //     x--;
    // if (pos.y < -0.5f)
    //     y--;
    return {x, y};
}

void Draw_Stretchable_Sprite(
    f32                  x0,
    f32                  x3,
    f32                  y0,
    f32                  y3,
    Loaded_Texture&      texture,
    UI_Sprite_Params&    sprite_params,
    v2i16                panel_size,
    f32                  in_scale,
    Game_Renderer_State& rstate
) {
    auto& pad_h = sprite_params.stretch_paddings_h;
    auto& pad_v = sprite_params.stretch_paddings_v;

    f32 x1 = (f32)pad_h.x / texture.size.x;
    f32 x2 = (f32)pad_h.y / texture.size.x;
    f32 y1 = (f32)pad_v.x / texture.size.y;
    f32 y2 = (f32)pad_v.y / texture.size.y;
    Assert(y1 * in_scale + y2 * in_scale <= 1);
    Assert(x1 * in_scale + x2 * in_scale <= 1);

    auto p0  = v3f(x0, y0, 1);
    auto p3  = v3f(x3, y3, 1);
    auto dx  = x3 - x0;
    auto dy  = y3 - y0;
    auto dp1 = v3f(
        pad_h.x * in_scale * dx / panel_size.x, pad_v.x * in_scale * dy / panel_size.y, 0
    );
    auto dp2 = v3f(
        pad_h.y * in_scale * dx / panel_size.x, pad_v.y * in_scale * dy / panel_size.y, 0
    );
    v3f p1 = p0 + dp1;
    v3f p2 = p3 - dp2;

    v3f points[] = {p0, p1, p2, p3};

    f32 texture_vertices_x[] = {0, x1, 1 - x2, 1};
    f32 texture_vertices_y[] = {0, y1, 1 - y2, 1};

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture.id);

    FOR_RANGE (int, y, 3) {
        auto tex_y0    = texture_vertices_y[2 - y];
        auto tex_y1    = texture_vertices_y[3 - y];
        auto sprite_y0 = points[2 - y].y;
        auto sy        = points[3 - y].y - points[2 - y].y;

        FOR_RANGE (int, x, 3) {
            auto tex_x0    = texture_vertices_x[x];
            auto tex_x1    = texture_vertices_x[x + 1];
            auto sprite_x0 = points[x].x;
            auto sx        = points[x + 1].x - sprite_x0;
            Assert(sx >= 0);

            Draw_UI_Sprite(
                tex_x0,
                tex_y0,
                tex_x1,
                tex_y1,
                v2f(sprite_x0, sprite_y0),
                v2f(sx, sy),
                v2f(0, 0),
                BF_Color_White,
                rstate
            );
        }
    }
}

struct Get_Buildable_Textures_Result {
    size_t  deallocation_size;
    GLuint* textures;
};

Get_Buildable_Textures_Result
Get_Buildable_Textures(Arena& trash_arena, Game_State& state) {
    auto& rstate   = Assert_Deref(state.renderer_state);
    auto& ui_state = Assert_Deref(rstate.ui_state);

    Get_Buildable_Textures_Result res = {};
    auto allocation_size              = sizeof(GLuint) * ui_state.buildables_count;

    res.deallocation_size = allocation_size;
    res.textures = Allocate_Array(trash_arena, GLuint, ui_state.buildables_count);

    FOR_RANGE (int, i, ui_state.buildables_count) {
        auto& buildable = *(ui_state.buildables + i);
        switch (buildable.type) {
        case Item_To_Build_Type::Road: {
            *(res.textures + i) = (rstate.road_textures + 15)->id;
        } break;

        case Item_To_Build_Type::Building: {
            *(res.textures + i) = buildable.scriptable_building->texture->id;
        } break;

        default:
            INVALID_PATH;
        }
    }

    return res;
}

void Render(Game_State& state, f32 dt, MCTX) {
    ZoneScoped;

    Arena& trash_arena = state.trash_arena;
    TEMP_USAGE(trash_arena);

    auto&        rstate   = Assert_Deref(state.renderer_state);
    auto&        game_map = state.game_map;
    Game_Bitmap& bitmap   = Assert_Deref(rstate.bitmap);

    auto gsize     = game_map.size;
    auto swidth    = (f32)bitmap.width;
    auto sheight   = (f32)bitmap.height;
    auto cell_size = 32;

    if (swidth == 0.0f || sheight == 0.0f)
        return;

    // NOTE: Обработка зума карты к курсору.
    // TODO: Зафиксить небольшую неточность, небольшой телепорт в самом конце анимации.
    {
        auto cursor_on_tilemap_pos = Screen_To_World(state, rstate.mouse_pos);
        ImGui::Text(
            "Tilemap %.3f.%.3f", cursor_on_tilemap_pos.x, cursor_on_tilemap_pos.y
        );

        auto new_zoom
            = Move_Towards(rstate.zoom, rstate.zoom_target, 2 * dt * rstate.zoom);
        rstate.zoom = new_zoom;

        auto cursor_on_tilemap_pos2 = Screen_To_World(state, rstate.mouse_pos);

        auto cursor_d = cursor_on_tilemap_pos2 - cursor_on_tilemap_pos;

        auto projection = glm::mat3(1);
        projection      = glm::translate(projection, v2f(0, 1));
        projection      = glm::scale(projection, v2f(1 / swidth, -1 / sheight));
        projection      = glm::translate(projection, rstate.pan_pos + rstate.pan_offset);
        projection      = glm::scale(projection, v2f(rstate.zoom, rstate.zoom));
        projection      = glm::translate(projection, v2f(swidth, sheight) / 2.0f);
        projection      = glm::translate(projection, -(v2f)gsize * (f32)cell_size / 2.0f);
        auto d          = projection * v3f(cursor_d.x, cursor_d.y, 0);
        rstate.pan_pos += cursor_d * (f32)(rstate.zoom * cell_size);

        auto d3 = World_To_Screen(state, v2f(0, 0));
        ImGui::Text("d3 %.3f.%.3f", d3.x, d3.y);

        auto d4 = World_Pos_To_Tile(cursor_on_tilemap_pos2);
        ImGui::Text("d4 %d.%d", d4.x, d4.y);
    }

    glClear(GL_COLOR_BUFFER_BIT);
    Check_OpenGL_Errors();

    GLuint texture_name = 3;
    glBindTexture(GL_TEXTURE_2D, texture_name);
    Check_OpenGL_Errors();

    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    Check_OpenGL_Errors();

    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA8,
        bitmap.width,
        bitmap.height,
        0,
        GL_BGRA_EXT,
        GL_UNSIGNED_BYTE,
        bitmap.memory
    );
    Check_OpenGL_Errors();

    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    Check_OpenGL_Errors();

    {
        glBlendFunc(GL_ONE, GL_ZERO);
        glBindTexture(GL_TEXTURE_2D, texture_name);
        glBegin(GL_TRIANGLES);

        v2f vertices[] = {{0, 0}, {0, 1}, {1, 0}, {1, 1}};
        for (auto i : {0, 1, 2, 2, 1, 3}) {
            auto& v = vertices[i];
            glTexCoord2f(v.x, v.y);
            glVertex2f(v.x, v.y);
        }

        glEnd();
    }

    auto projection = glm::mat3(1);
    projection      = glm::translate(projection, v2f(0, 1));
    projection      = glm::scale(projection, v2f(1 / swidth, -1 / sheight));
    projection      = glm::translate(projection, rstate.pan_pos + rstate.pan_offset);
    projection      = glm::scale(projection, v2f(rstate.zoom, rstate.zoom));
    projection      = glm::translate(projection, v2f(swidth, sheight) / 2.0f);
    projection      = glm::translate(projection, -(v2f)gsize * (f32)cell_size / 2.0f);
    // projection = glm::scale(projection, v2f(1, 1) * (f32)cell_size);
    // projection = glm::scale(projection, v2f(2, 2) / 2.0f);

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    // NOTE: Рисование поверхности (травы).
    FOR_RANGE (i32, h, rstate.terrain_tilemaps_count) {
        auto& tilemap = *(rstate.tilemaps + h);

        FOR_RANGE (int, y, gsize.y) {
            FOR_RANGE (int, x, gsize.x) {
                auto& tile = Get_Terrain_Tile(game_map, {x, y});
                if (tile.terrain != Terrain::Grass)
                    continue;

                if (tile.height < h)
                    continue;

                // TODO: Spritesheets! Vertices array,
                // texture vertices array, indices array
                auto texture_id = Test_Smart_Tile(
                    tilemap, game_map.size, {x, y}, rstate.grass_smart_tile
                );

                glBindTexture(GL_TEXTURE_2D, texture_id);

                auto sprite_pos  = v2i(x, y) * cell_size;
                auto sprite_size = v2i(1, 1) * cell_size;

                glBegin(GL_TRIANGLES);
                Draw_Sprite(0, 0, 1, 1, sprite_pos, sprite_size, 0, projection);
                glEnd();
            }
        }
    }

    // --- Drawing Resoures ---
    // NOTE: Рисование деревьев.
    auto& resources_tilemap = *(rstate.tilemaps + rstate.resources_tilemap_index);
    FOR_RANGE (int, y, gsize.y) {
        FOR_RANGE (int, x, gsize.x) {
            auto& tile = *(resources_tilemap.tiles + y * gsize.x + x);
            if (tile == 0)
                continue;

            if (tile == rstate.forest_smart_tile.id) {
                auto texture_id = Test_Smart_Tile(
                    resources_tilemap, game_map.size, {x, y}, rstate.forest_smart_tile
                );

                glBindTexture(GL_TEXTURE_2D, texture_id);

                auto sprite_pos  = v2i(x, y) * cell_size;
                auto sprite_size = v2i(1, 1) * cell_size;

                glBegin(GL_TRIANGLES);
                Draw_Sprite(0, 0, 1, 1, sprite_pos, sprite_size, 0, projection);
                glEnd();
            }
            else
                INVALID_PATH;
        }
    }

    auto& resources_tilemap2 = *(rstate.tilemaps + rstate.resources_tilemap_index + 1);
    FOR_RANGE (int, y, gsize.y) {
        FOR_RANGE (int, x, gsize.x) {
            auto& tile = *(resources_tilemap2.tiles + y * gsize.x + x);
            if (tile == 0)
                continue;

            if (tile == rstate.forest_top_tile_id) {
                glBindTexture(GL_TEXTURE_2D, rstate.forest_textures[0].id);

                auto sprite_pos  = v2i(x, y + 1) * cell_size;
                auto sprite_size = v2i(1, 1) * cell_size;

                glBegin(GL_TRIANGLES);
                Draw_Sprite(0, 0, 1, 1, sprite_pos, sprite_size, 0, projection);
                glEnd();
            }
            else
                INVALID_PATH;
        }
    }
    // --- Drawing Resoures End ---

    // --- Drawing Element Tiles ---
    // NOTE: Рисование дорог.
    auto& element_tilemap   = *(rstate.tilemaps + rstate.element_tilemap_index);
    auto& element_tilemap_2 = *(rstate.tilemaps + rstate.element_tilemap_index + 1);
    FOR_RANGE (int, y, gsize.y) {
        FOR_RANGE (int, x, gsize.x) {
            auto& tile = *(element_tilemap.tiles + y * gsize.x + x);
            if (tile < global_road_starting_tile_id)
                continue;

            auto road_texture_offset = tile - global_road_starting_tile_id;
            Assert(road_texture_offset >= 0);

            auto tex_id = rstate.road_textures[road_texture_offset].id;
            glBindTexture(GL_TEXTURE_2D, tex_id);

            auto sprite_pos  = v2i(x, y) * cell_size;
            auto sprite_size = v2i(1, 1) * cell_size;

            glBegin(GL_TRIANGLES);
            Draw_Sprite(0, 0, 1, 1, sprite_pos, sprite_size, 0, projection);
            glEnd();
        }
    }

    // NOTE: Рисование флагов.
    FOR_RANGE (int, y, gsize.y) {
        FOR_RANGE (int, x, gsize.x) {
            auto& tile = *(element_tilemap_2.tiles + y * gsize.x + x);
            if (tile < global_flag_starting_tile_id)
                continue;

            auto tex_id = rstate.flag_textures[tile - global_flag_starting_tile_id].id;
            glBindTexture(GL_TEXTURE_2D, tex_id);

            auto sprite_pos  = v2i(x, y) * cell_size;
            auto sprite_size = v2i(1, 1) * cell_size;

            glBegin(GL_TRIANGLES);
            Draw_Sprite(0, 0, 1, 1, sprite_pos, sprite_size, 0, projection);
            glEnd();
        }
    }
    // --- Drawing Element Tiles End ---

    // NOTE: Рисование зданий.
    for (auto building_ptr : Iter(&game_map.buildings)) {
        auto& building            = *building_ptr;
        auto& scriptable_building = Assert_Deref(building.scriptable);

        auto tex_id = scriptable_building.texture->id;

        auto constructed = building.construction_points
                           >= building.scriptable->required_construction_points;
        if (!constructed)
            tex_id = rstate.in_progress_building_texture.id;

        glBindTexture(GL_TEXTURE_2D, tex_id);

        auto sprite_pos  = v2i(building.pos) * cell_size;
        auto sprite_size = v2i(1, 1) * cell_size;

        glBegin(GL_TRIANGLES);
        Draw_Sprite(0, 0, 1, 1, sprite_pos, sprite_size, 0, projection);
        glEnd();
    }

    // NOTE: Рисование чувачков.
    {
        i32 i = 0;
        for (auto human_ptr : Iter(&game_map.humans)) {
            auto& human  = Assert_Deref(human_ptr);
            auto  tex_id = rstate.human_texture.id;
            glBindTexture(GL_TEXTURE_2D, tex_id);

            v2f pos = human.moving.pos;
            if (human.moving.to.has_value()) {
                f32 t = human.moving.progress;
                pos = v2f(human.moving.to.value()) * t + v2f(human.moving.pos) * (1 - t);
            }

            // ImGui::Text("human %d pos %f.%f", i, pos.x, pos.y);

            auto sprite_pos  = pos * v2f(cell_size);
            auto sprite_size = v2i(1, 1) * cell_size;

            glBegin(GL_TRIANGLES);
            Draw_Sprite(0, 0, 1, 1, sprite_pos, sprite_size, 0, projection);
            glEnd();
        }

        i++;
    }

    // NOTE: Рисование ресурсов-предметов на карте (которые, например,
    // сразу идут на старте игры или же были положены чувачками на карту).
    {
        i32 i = 0;

        FOR_RANGE (int, y, gsize.y) {
            FOR_RANGE (int, x, gsize.x) {
                for (auto resource_pptr :
                     Iter(&game_map.resources, {x, y}, game_map.size.x)) {
                    auto& resource   = Assert_Deref(Assert_Deref(resource_pptr));
                    auto& scriptable = Assert_Deref(resource.scriptable);
                    auto  tex_id     = Assert_Deref(scriptable.small_texture).id;

                    glBindTexture(GL_TEXTURE_2D, tex_id);

                    v2f  pos         = {x, y};
                    auto sprite_pos  = pos * v2f(cell_size);
                    auto sprite_size = v2i_one * cell_size;

                    glBegin(GL_TRIANGLES);
                    Draw_Sprite(0, 0, 1, 1, sprite_pos, sprite_size, 0, projection);
                    glEnd();
                }
            }
        }
    }

    glDeleteTextures(1, (GLuint*)&texture_name);
    Check_OpenGL_Errors();

    // NOTE: Рисование UI плашки слева.
    auto& ui_state = *rstate.ui_state;
    {
        auto  sprite_params = ui_state.buildables_panel_params;
        auto& pad_h         = sprite_params.stretch_paddings_h;
        auto& pad_v         = sprite_params.stretch_paddings_v;

        auto  texture             = ui_state.buildables_panel_background;
        auto  placeholder_texture = ui_state.buildables_placeholder_background;
        auto& psize               = placeholder_texture.size;

        auto scale         = ui_state.scale;
        auto in_scale      = ui_state.buildables_panel_in_scale;
        v2f  sprite_anchor = ui_state.buildables_panel_sprite_anchor;

        v2f  padding          = ui_state.padding;
        f32  placeholders_gap = ui_state.placeholders_gap;
        auto placeholders     = ui_state.placeholders;
        auto panel_size       = v2f(
            psize.x + 2 * padding.x,
            2 * padding.y + placeholders_gap * (placeholders - 1) + placeholders * psize.y
        );

        auto outer_anchor         = ui_state.buildables_panel_container_anchor;
        auto outer_container_size = v2i(swidth, sheight);
        auto outer_x              = outer_container_size.x * outer_anchor.x;
        auto outer_y              = outer_container_size.y * outer_anchor.y;

        auto projection = glm::mat3(1);
        projection      = glm::translate(projection, v2f(0, 1));
        projection      = glm::scale(projection, v2f(1 / swidth, -1 / sheight));
        projection      = glm::translate(projection, v2f((int)outer_x, (int)outer_y));
        projection      = glm::scale(projection, v2f(scale));

        {
            auto model = glm::mat3(1);
            model      = glm::scale(model, v2f(panel_size));

            auto p0_local = model * v3f(v2f_zero - sprite_anchor, 1);
            auto p1_local = model * v3f(v2f_one - sprite_anchor, 1);
            auto p0       = projection * p0_local;
            auto p1       = projection * p1_local;
            Draw_Stretchable_Sprite(
                p0.x,
                p1.x,
                p0.y,
                p1.y,
                texture,
                sprite_params,
                panel_size,
                in_scale,
                rstate
            );

            auto origin = (p1_local + p0_local) / 2.0f;

            glBindTexture(GL_TEXTURE_2D, ui_state.buildables_placeholder_background.id);
            // Aligning items in a column
            // justify-content: center
            FOR_RANGE (int, i, placeholders) {
                auto drawing_point = origin;
                drawing_point.y -= (placeholders - 1) * (psize.y + placeholders_gap) / 2;
                drawing_point.y += i * (placeholders_gap + psize.y);

                auto p     = projection * drawing_point;
                auto s     = projection * v3f(psize, 0);
                auto color = (i == ui_state.selected_buildable_index)
                                 ? ui_state.selected_buildable_color
                                 : ui_state.not_selected_buildable_color;
                Draw_UI_Sprite(0, 0, 1, 1, p, s, v2f_one / 2.0f, color, rstate);
            }

            auto buildable_textures = Get_Buildable_Textures(trash_arena, state);

            auto buildable_size = v2f(psize) * (2.0f / 3.0f);
            FOR_RANGE (int, i, placeholders) {
                auto drawing_point = origin;
                drawing_point.y -= (placeholders - 1) * (psize.y + placeholders_gap) / 2;
                drawing_point.y += i * (placeholders_gap + psize.y);

                auto p = projection * drawing_point;
                auto s = projection * v3f(buildable_size, 0);
                glBindTexture(GL_TEXTURE_2D, buildable_textures.textures[i]);

                auto color = (i == ui_state.selected_buildable_index)
                                 ? ui_state.selected_buildable_color
                                 : ui_state.not_selected_buildable_color;
                Draw_UI_Sprite(0, 0, 1, 1, p, s, v2f_one / 2.0f, color, rstate);
            }
        }
    }

    // NOTE: Дебаг-рисование штук для чувачков.
    {
        for (auto human_ptr : Iter(&game_map.humans)) {
            auto& human = Assert_Deref(human_ptr);

#if 0
            if (human.moving.path.count > 0) {
                auto last_p = *(human.moving.path.base + human.moving.path.count - 1);
                auto p = projection
                    * v3f((v2f(last_p) + v2f_one / 2.0f) * (f32)cell_size, 1);

                glPointSize(12);

                glBlendFunc(GL_ONE, GL_ZERO);

                glBegin(GL_POINTS);
                glVertex2f(p.x, p.y);
                glEnd();
            }
#endif

#if 0
            if (human.moving.to.has_value()) {
                auto p
                    = projection * v3f(v2f(human.moving.to.value()) * (f32)cell_size, 1);

                glPointSize(12);

                glBlendFunc(GL_ONE, GL_ZERO);

                glBegin(GL_POINTS);
                glVertex2f(p.x, p.y);
                glEnd();
            }
#endif
        }
    }

    // NOTE: Дебаг-рисование сегментов.
#if 0
    {
        const BF_Color colors[] = {
            {1, 0, 0},  //
            {0, 1, 0},  //
            {0, 0, 1},  //
            {1, 1, 0},  //
            {0, 1, 1},  //
            {1, 0, 1},  //
            {1, 1, 1}  //
        };
        size_t colors_count = sizeof(colors) / sizeof(colors[0]);
        local_persist size_t segment_index = 0;
        segment_index++;
        if (segment_index >= colors_count)
            segment_index = 0;

        int rendered_segments = 0;
        for (auto segment_ptr : Iter(&game_map.segments)) {
            auto& segment = *segment_ptr;
            auto& graph = segment.graph;

            rendered_segments++;

            FOR_RANGE(int, y, graph.size.y) {
                FOR_RANGE(int, x, graph.size.x) {
                    auto node = *(graph.nodes + y * graph.size.x + x);
                    if (!node)
                        continue;

                    v2f center = v2f(x, y) + v2f(graph.offset.x, graph.offset.y)
                        + v2f_one / 2.0f;
                    FOR_RANGE(int, ii, 4) {
                        auto dir = (Direction)ii;
                        if (!Graph_Node_Has(node, dir))
                            continue;

                        auto offsetted = center + (v2f)(As_Offset(dir)) / 2.0f;

                        auto p1 = projection * v3f(center * (f32)cell_size, 1);
                        auto p2 = projection * v3f(offsetted * (f32)cell_size, 1);
                        auto p3 = (p1 + p2) / 2.0f;

                        glPointSize(12);

                        glBlendFunc(GL_ONE, GL_ZERO);

                        glBegin(GL_POINTS);
                        glVertex2f(p1.x, p1.y);
                        glVertex2f(p2.x, p2.y);
                        glVertex2f(p3.x, p3.y);
                        glEnd();
                    }
                }
            }
        }

        ImGui::Text("rendered segments %d", rendered_segments);

        glColor3fv((GLfloat*)(colors + 6));
    }
#endif

    ImGui::Text(
        "ui_state.selected_buildable_index %d", ui_state.selected_buildable_index
    );
}

// NOTE: Game_State& state, v2i16 pos, Item_To_Build item
On_Item_Built__Function(Renderer__On_Item_Built) {
    Assert(state.renderer_state != nullptr);
    auto& rstate   = *state.renderer_state;
    auto& game_map = state.game_map;
    auto  gsize    = game_map.size;

    auto& element_tilemap   = *(rstate.tilemaps + rstate.element_tilemap_index);
    auto& element_tilemap_2 = *(rstate.tilemaps + rstate.element_tilemap_index + 1);
    auto  tile_index        = pos.y * gsize.x + pos.x;

    auto& element_tile = *(game_map.element_tiles + tile_index);

    switch (element_tile.type) {
    case Element_Tile_Type::Building:
    case Element_Tile_Type::Road: {
        *(element_tilemap_2.tiles + pos.y * gsize.x + pos.x) = 0;
    } break;

    case Element_Tile_Type::Flag: {
        *(element_tilemap_2.tiles + pos.y * gsize.x + pos.x)
            = global_flag_starting_tile_id;
    } break;

    default:
        INVALID_PATH;
    }

    if (element_tile.type != Element_Tile_Type::Building)
        Assert(element_tile.building == nullptr);

    for (auto offset : v2i16_adjacent_offsets_including_0) {
        auto new_pos = pos + offset;
        if (!Pos_Is_In_Bounds(new_pos, gsize))
            continue;

        auto          element_tiles = game_map.element_tiles;
        Element_Tile& element_tile  = *(element_tiles + new_pos.y * gsize.x + new_pos.x);

        switch (element_tile.type) {
        case Element_Tile_Type::Flag: {
            auto  tex = Get_Road_Texture_Number(game_map.element_tiles, new_pos, gsize);
            auto& tile_id = *(element_tilemap.tiles + new_pos.y * gsize.x + new_pos.x);
            tile_id       = global_road_starting_tile_id + tex;
        } break;

        case Element_Tile_Type::Building:
        case Element_Tile_Type::Road: {
            auto  tex = Get_Road_Texture_Number(game_map.element_tiles, new_pos, gsize);
            auto& tile_id = *(element_tilemap.tiles + new_pos.y * gsize.x + new_pos.x);
            tile_id       = global_road_starting_tile_id + tex;
        } break;

        case Element_Tile_Type::None:
            break;

        default:
            INVALID_PATH;
        }
    }
}
