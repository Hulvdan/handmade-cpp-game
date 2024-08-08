
#ifndef BF_CLIENT
#    error "This code should run on a client! BF_CLIENT must be defined!"
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
    // Earlier versions of BMP Info Headers don't do anything specified below.
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

Load_BMP_RGBA_Result Load_BMP_RGBA(Arena& arena, const u8* data) {
    Load_BMP_RGBA_Result res{};

    auto& header = *(Debug_BMP_Header*)data;

    if (header.signature != *(u16*)"BM") {
        // TODO: Diagnostic. Not a BMP file
        INVALID_PATH;
        return res;
    }

    auto dib_size = header.dib_header_size;
    if (dib_size != 125 && dib_size != 56 && dib_size != 40) {
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

    memcpy(res.output, data + header.data_offset, total_bytes);

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
        Assert_Truncate_To_i32(texture.size.x),
        Assert_Truncate_To_i32(texture.size.y),
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

    Load_BMP_RGBA_Result bmp_result{};
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

const BFGame::Atlas* Load_Atlas_From_Binary(Arena& arena) {
    auto result = Debug_Load_File_To_Arena("assets/art/atlas.bin", arena);
    Assert(result.success);
    return BFGame::GetAtlas(result.output);
}

Atlas Load_Atlas(
    const char* atlas_name,
    Arena&      destination_arena,
    Arena&      trash_arena,
    MCTX
) {
    TEMP_USAGE(trash_arena);

    char filepath[512];
    sprintf(filepath, "assets/art/%s.bmp", atlas_name);

    Load_BMP_RGBA_Result bmp_result{};
    {
        TEMP_USAGE(trash_arena);
        auto load_result = Debug_Load_File_To_Arena(filepath, trash_arena);
        Assert(load_result.success);

        bmp_result = Load_BMP_RGBA(destination_arena, load_result.output);
        Assert(bmp_result.success);
    }

    auto atlas_spec = Load_Atlas_From_Binary(trash_arena);

    Atlas atlas{};
    atlas.size = {atlas_spec->size_x(), atlas_spec->size_y()};
    Init_Vector(atlas.textures, ctx);
    const auto textures_count = atlas_spec->textures()->size();

    FOR_RANGE (int, i, textures_count) {
        auto& texture = *atlas_spec->textures()->Get(i);

        C_Texture t = {
            scast<Texture_ID>(texture.id()),
            {
                f32(texture.atlas_x()) / f32(atlas.size.x),
                f32(texture.atlas_y()) / f32(atlas.size.y),
            },
            {texture.size_x(), texture.size_y()},
        };

        *atlas.textures.Vector_Occupy_Slot(ctx) = t;
    }

    auto& atlas_texture = atlas.texture;
    atlas_texture.id    = scast<BF_Texture_ID>(Hash32_String(atlas_name));
    atlas_texture.size  = {bmp_result.width, bmp_result.height};
    atlas_texture.base  = bmp_result.output;
    Send_Texture_To_GPU(atlas_texture);

    return atlas;
}

int Get_Road_Texture_Number(Element_Tile* element_tiles, v2i16 pos, v2i16 gsize) {
    Element_Tile& tile = element_tiles[pos.y * gsize.x + pos.x];

    bool tile_is_flag     = tile.type == Element_Tile_Type::Flag;
    bool tile_is_building = tile.type == Element_Tile_Type::Building;
    // bool tile_is_road     = tile.type == Element_Tile_Type::Road;

    int road_texture_number = 0;
    FOR_RANGE (int, i, 4) {
        auto new_pos = pos + v2i16_adjacent_offsets[i];
        if (!Pos_Is_In_Bounds(new_pos, gsize))
            continue;

        Element_Tile& adjacent_tile    = element_tiles[new_pos.y * gsize.x + new_pos.x];
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

    GLint succeeded = 0;
    glGetShaderiv(shader_id, GL_COMPILE_STATUS, &succeeded);

    GLint log_length = 0;
    glGetShaderiv(shader_id, GL_INFO_LOG_LENGTH, &log_length);

    GLchar  zero     = 0;
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

// Создаёт программу и запихивает её в program, если успешно скомпилилось.
// Если неуспешно - значение program остаётся прежним.
void Create_Shader_Program(
    GLuint&     program,
    const char* vertex_code,
    const char* fragment_code,
    Arena&      trash_arena
) {
    TEMP_USAGE(trash_arena);

    auto vertex_shader = glCreateShader(GL_VERTEX_SHADER);
    defer {
        glDeleteShader(vertex_shader);
    };

    glShaderSource(vertex_shader, 1, scast<const GLchar* const*>(&vertex_code), nullptr);
    glCompileShader(vertex_shader);

    GLint vertex_success = 0;
    glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &vertex_success);
    Debug_Print_Shader_Info_Log(vertex_shader, trash_arena, "Vertex shader compilation");

    // Similiar for Fragment Shader
    auto fragment_shader = glCreateShader(GL_FRAGMENT_SHADER);
    defer {
        glDeleteShader(fragment_shader);
    };

    glShaderSource(
        fragment_shader, 1, scast<const GLchar* const*>(&fragment_code), nullptr
    );
    glCompileShader(fragment_shader);

    GLint fragment_success = 0;
    glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &fragment_success);
    Debug_Print_Shader_Info_Log(
        fragment_shader, trash_arena, "Fragment shader compilation"
    );

    if (!fragment_success || !vertex_success)
        return;

    // shader Program
    auto new_program_id = glCreateProgram();
    glAttachShader(new_program_id, vertex_shader);
    glAttachShader(new_program_id, fragment_shader);
    glLinkProgram(new_program_id);
    // print linking errors if any
    GLint log_length = 0;
    glGetProgramiv(new_program_id, GL_INFO_LOG_LENGTH, &log_length);

    GLint program_success = 0;
    glGetProgramiv(new_program_id, GL_LINK_STATUS, &program_success);

    auto    zero     = GLchar(0);
    GLchar* info_log = &zero;
    if (log_length) {
        info_log = Allocate_Array(trash_arena, GLchar, log_length);
        glGetProgramInfoLog(new_program_id, log_length, nullptr, info_log);
    }

    if (program_success)
        DEBUG_Print("INFO:\tProgram compilation succeeded: %s\n", info_log);
    else
        DEBUG_Error("ERROR:\tProgram compilation failed: %s\n", info_log);

    if (program)
        glDeleteProgram(program);

    program = new_program_id;
}

void Init_Renderer(
    bool        first_time_initializing,
    bool        hot_reloaded,
    Game_State& state,
    Arena&      arena,
    Arena& /* non_persistent_arena */,
    Arena& /* trash_arena */,
    MCTX
) {
    CTX_ALLOCATOR;

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
}

void Add_Building_Sprite(
    Game_Renderer_State& rstate,
    Game_Map&            game_map,
    v2i                  pos,
    Building_ID          building_id,
    MCTX
) {
    Assert(building_id != Building_ID_Missing);

    auto building = Query_Building(game_map, building_id);

    auto& scriptable = *building->scriptable;

    C_Sprite building_sprite{};
    building_sprite.pos   = pos;
    building_sprite.scale = {1, 1};
    // building_sprite.anchor   = {0.5f, 0.5f};
    building_sprite.anchor   = {1, 1};
    building_sprite.rotation = 0;
    building_sprite.texture  = scriptable.texture;
    building_sprite.z        = 0;

    {
        auto [pid, pvalue] = rstate.sprites.Add(ctx);
        *pid               = building_id;
        *pvalue            = building_sprite;
    }
}

void Set_Flag_Tile(
    Game_Renderer_State& rstate,
    Game_Map& /* game_map */,
    v2i pos,
    MCTX_
) {
    auto& placeables_tilemap = rstate.tilemaps[rstate.element_tilemap_index + 1];

    auto t = pos.y * placeables_tilemap.size.x + pos.x;

    auto& tile_id    = placeables_tilemap.tiles[t];
    auto& texture_id = placeables_tilemap.textures[t];

    tile_id    = rstate.flag_tile_id;
    texture_id = rstate.flag_textures[0];
}

void Post_Init_Renderer(
    bool        first_time_initializing,
    bool        hot_reloaded,
    Game_State& state,
    Arena&      arena,
    Arena&      non_persistent_arena,
    Arena&      trash_arena,
    MCTX
) {
    CTX_ALLOCATOR;

    if (!first_time_initializing && !hot_reloaded)
        return;

    auto& rstate = Assert_Deref(state.renderer_state);

    rstate.ui_state = Allocate_Zeros_For(non_persistent_arena, Game_UI_State);
    auto& ui_state  = *rstate.ui_state;

    auto& game_map = state.game_map;
    auto  gsize    = game_map.size;

    std::construct_at(
        &rstate.atlas, Load_Atlas("atlas", non_persistent_arena, trash_arena, ctx)
    );

    std::construct_at(&rstate.sprites);

    // Перезагрузка шейдеров.
    Create_Shader_Program(
        rstate.sprites_shader_program,
        R"VertexShader(
#version 330 core

layout (location = 0) in vec2 a_pos;
layout (location = 1) in vec3 a_color;
layout (location = 2) in vec2 a_tex_coord;

out vec3 color;
out vec2 tex_coord;

void main() {
    gl_Position = vec4(a_pos, 0, 1);
    color = a_color;
    tex_coord = a_tex_coord;
}
)VertexShader",
        R"FragmentShader(
#version 330 core

uniform vec2 a_tile_size_relative_to_atlas;

out vec4 frag_color;

in vec3 color;
varying in vec2 tex_coord;

uniform sampler2D ourTexture;

void main() {
    frag_color =
        texture(ourTexture, vec2(tex_coord.x, 1 - tex_coord.y))
        * vec4(color, 1);
}
)FragmentShader",
        trash_arena
    );

    Create_Shader_Program(
        rstate.tilemap_shader_program,
        R"VertexShader(
#version 330 core

uniform vec3 a_color;

layout (location = 0) in vec2 a_pos;
layout (location = 1) in vec2 a_tex_coord;

out vec3 color;
out vec2 tex_coord;

void main() {
    gl_Position = vec4(a_pos, 0, 1);
    color = a_color;
    tex_coord = a_tex_coord;
}
)VertexShader",
        R"FragmentShader(
#version 430 core

uniform vec2 a_tile_size_relative_to_atlas;
uniform ivec2 a_gsize;
uniform ivec2 a_resolution;
uniform ivec2 a_atlas_texture_size;

uniform mat3 a_gl2w;  // unused
uniform mat3 a_relscreen2w;

layout(std430, binding = 1) buffer tiles2AtlasPositionsBuffer {
    float data[];
};

in vec3 color;
in vec2 tex_coord;

out vec4 frag_color;

uniform sampler2D ourTexture;

float map(float value, float min1, float max1, float min2, float max2) {
    return min2 + (value - min1) * (max2 - min2) / (max1 - min1);
}

void main() {
    vec2 relative_frag_screen_pos = vec2(gl_FragCoord) / gl_FragCoord.w / a_resolution;
    relative_frag_screen_pos.y = 1 - relative_frag_screen_pos.y;

#if 0
    frag_color = vec4(
        relative_frag_screen_pos,
        0,
        1
    );
    return;
#endif

    vec2 pos = tex_coord * a_gsize;

    ivec2 tile  = ivec2(pos);

    vec2 offset = pos - vec2(tile);
    offset.y    = 1 - offset.y;

#if 0
    frag_color = vec4(
        vec2(tile) / vec2(a_gsize),
        0,
        1
    );
    return;
#endif

    if (
        tile.x < 0
        || tile.y < 0
        || tile.x > a_gsize.x
        || tile.y > a_gsize.y
    )
        discard;

    int tiles_count = a_gsize.x * a_gsize.y;
    int tile_number = tile.y * a_gsize.x + tile.x;

#if 0
    frag_color = vec4(
        float(tile_number) / float(tiles_count),
        0,
        0,
        1
    );
    return;
#endif

    vec2 atlas_texture = vec2(
        data[2 * tile_number],
        data[2 * tile_number + 1]
    );
    if (atlas_texture.x < 0 || atlas_texture.y < 0)
        discard;

#if 0
    frag_color = vec4(
        atlas_texture_x,
        atlas_texture_y,
        0,
        1
    );
    return;
#endif

#if 0
    frag_color = vec4(
        atlas_offset,
        0,
        1
    );
    return;
#endif

    frag_color = texture(
        ourTexture,
        vec2(
            clamp(
                atlas_texture.x + a_tile_size_relative_to_atlas.x * offset.x,
                0, 1),
            1 - clamp(
                atlas_texture.y + a_tile_size_relative_to_atlas.y * offset.y,
                0, 1)
        )
    );

    frag_color.rgb *= color.rgb;
}
)FragmentShader",
        trash_arena
    );

    if (!rstate.shaders_compilation_failed) {
        glUseProgram(rstate.sprites_shader_program);
        glEnable(GL_TEXTURE_2D);
        {
            auto location = glGetUniformLocation(
                rstate.sprites_shader_program, "a_tile_size_relative_to_atlas"
            );
            auto tile_x = 16.0f / (f32)rstate.atlas.texture.size.x;
            auto tile_y = 16.0f / (f32)rstate.atlas.texture.size.y;
            glUniform2f(location, tile_x, tile_y);
        }

        glUseProgram(rstate.tilemap_shader_program);
        glEnable(GL_TEXTURE_2D);
        {
            auto location = glGetUniformLocation(
                rstate.tilemap_shader_program, "a_tile_size_relative_to_atlas"
            );
            auto tile_x = 16.0f / (f32)rstate.atlas.texture.size.x;
            auto tile_y = 16.0f / (f32)rstate.atlas.texture.size.y;
            glUniform2f(location, tile_x, tile_y);
        }
        {
            auto location
                = glGetUniformLocation(rstate.tilemap_shader_program, "a_gsize");
            glUniform2i(location, gsize.x, gsize.y);
        }
        {
            auto location = glGetUniformLocation(
                rstate.tilemap_shader_program, "a_atlas_texture_size"
            );
            glUniform2i(
                location,
                Assert_Truncate_To_i32(rstate.atlas.texture.size.x),
                Assert_Truncate_To_i32(rstate.atlas.texture.size.y)
            );
        }

        rstate.sprites_shader_gl2w_location
            = glGetUniformLocation(rstate.sprites_shader_program, "a_gl2w");

        rstate.tilemap_shader_gl2w_location
            = glGetUniformLocation(rstate.tilemap_shader_program, "a_gl2w");
        rstate.tilemap_shader_relscreen2w_location
            = glGetUniformLocation(rstate.tilemap_shader_program, "a_relscreen2w");
        rstate.tilemap_shader_resolution_location
            = glGetUniformLocation(rstate.tilemap_shader_program, "a_resolution");

        rstate.tilemap_shader_color_location
            = glGetUniformLocation(rstate.tilemap_shader_program, "a_color");
        glUniform3f(rstate.tilemap_shader_color_location, 1, 1, 1);
    }

    rstate.grass_smart_tile.id  = 1;
    rstate.forest_smart_tile.id = 2;
    rstate.forest_top_tile_id   = 3;
    rstate.flag_tile_id         = 4;

    {
        auto art = state.gamelib->art();

        rstate.human_texture                = art->human();
        rstate.building_in_progress_texture = art->building_in_progress();

        FOR_RANGE (int, i, 3) {
            rstate.forest_textures[i] = art->forest()->Get(i);
        }
        FOR_RANGE (int, i, 17) {
            rstate.grass_textures[i] = art->grass()->Get(i);
        }
        FOR_RANGE (int, i, 16) {
            rstate.road_textures[i] = art->road()->Get(i);
        }
        FOR_RANGE (int, i, 4) {
            rstate.flag_textures[i] = art->flag()->Get(i);
        }

        auto ui                                 = art->ui();
        ui_state.buildables_panel_texture       = ui->buildables_panel();
        ui_state.buildables_placeholder_texture = ui->buildables_placeholder();

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
            const auto& lib_instance = *state.gamelib->resources()->Get(i);

            auto& resource         = state.scriptable_resources[i];
            resource.texture       = lib_instance.texture();
            resource.small_texture = lib_instance.small_texture();
        }

        FOR_RANGE (int, i, state.scriptable_buildings_count) {
            const auto& lib_instance = *state.gamelib->buildings()->Get(i);

            auto& building   = state.scriptable_buildings[i];
            building.texture = lib_instance.texture();
        }
    }

    i32 max_height = 0;
    FOR_RANGE (i32, y, gsize.y) {
        FOR_RANGE (i32, x, gsize.x) {
            auto& terrain_tile = game_map.terrain_tiles[y * gsize.x + x];
            max_height         = MAX(max_height, terrain_tile.height);
        }
    }

    rstate.tilemaps_count = 0;
    // NOTE: Terrain tilemaps ([0; max_height])
    rstate.terrain_tilemaps_count = max_height + 1;
    rstate.tilemaps_count += rstate.terrain_tilemaps_count;

    // NOTE: Terrain Resources (forests, stones, etc.)
    // 1) Forests
    // 2) Tree's top tiles
    rstate.resources_tilemap_index = rstate.tilemaps_count;
    rstate.tilemaps_count += 2;

    // NOTE: Element Tiles (roads, buildings, etc.)
    // 1) Roads
    // 2) Flags above roads
    rstate.element_tilemap_index = rstate.tilemaps_count;
    rstate.tilemaps_count += 2;

    rstate.tilemaps = Allocate_Array_And_Initialize(
        non_persistent_arena, Tilemap, rstate.tilemaps_count
    );

    FOR_RANGE (i32, h, rstate.tilemaps_count) {
        auto& tilemap = rstate.tilemaps[h];

        tilemap.size     = gsize + v2i16(0, 1);
        auto tiles_count = tilemap.size.x * tilemap.size.y;

        tilemap.tiles = Allocate_Zeros_Array(non_persistent_arena, Tile_ID, tiles_count);
        tilemap.textures
            = Allocate_Zeros_Array(non_persistent_arena, Texture_ID, tiles_count);
        tilemap.debug_rendering_enabled = true;
    }

    // Проставление текстур тайлов в tilemap-е terrain-а.
    FOR_RANGE (i32, h, rstate.terrain_tilemaps_count) {
        auto& tilemap = rstate.tilemaps[h];

        FOR_RANGE (i32, y, gsize.y) {
            FOR_RANGE (i32, x, gsize.x) {
                auto& tile         = game_map.terrain_tiles[y * gsize.x + x];
                auto& tilemap_tile = tilemap.tiles[y * gsize.x + x];

                bool grass   = tile.terrain == Terrain::Grass && tile.height >= h;
                tilemap_tile = grass * rstate.grass_smart_tile.id;
            }
        }

        FOR_RANGE (i32, y, gsize.y) {
            FOR_RANGE (i32, x, gsize.x) {
                Texture_ID id = 0;

                if (tilemap.tiles[y * gsize.x + x])
                    id = Test_Smart_Tile(
                        tilemap, game_map.size, {x, y}, rstate.grass_smart_tile
                    );
                else
                    id = Texture_ID_Missing;

                tilemap.textures[y * gsize.x + x] = id;
            }
        }
    }

    auto& resources_tilemap  = rstate.tilemaps[rstate.resources_tilemap_index];
    auto& resources_tilemap2 = rstate.tilemaps[rstate.resources_tilemap_index + 1];

    FOR_RANGE (i32, y, gsize.y) {
        FOR_RANGE (i32, x, gsize.x) {
            const auto& resource = game_map.terrain_resources[y * gsize.x + x];

            const bool forest = resource.amount > 0;

            if (forest) {
                auto& tile = resources_tilemap.tiles[y * gsize.x + x];
                tile       = rstate.forest_smart_tile.id;
            }
        }
    }

    FOR_RANGE (i32, y, gsize.y) {
        bool is_last_row = (y == (gsize.y - 1));

        FOR_RANGE (i32, x, gsize.x) {
            const auto t       = y * gsize.x + x;
            const auto t_above = (y + 1) * gsize.x + x;

            const auto& tile = resources_tilemap.tiles[t];
            if (!tile)
                continue;

            resources_tilemap.textures[t] = Test_Smart_Tile(
                resources_tilemap, gsize, {x, y}, rstate.forest_smart_tile
            );

            bool forest_is_above = false;
            if (!is_last_row) {
                auto& tile_above = resources_tilemap.tiles[t_above];
                forest_is_above  = tile_above == rstate.forest_smart_tile.id;
            }

            if (!forest_is_above) {
                resources_tilemap2.tiles[t_above]    = rstate.forest_top_tile_id;
                resources_tilemap2.textures[t_above] = rstate.forest_textures[0];
            }
        }
    }

    // --- Element Tiles ---
    auto& element_tilemap = rstate.tilemaps[rstate.element_tilemap_index];
    FOR_RANGE (i32, y, gsize.y) {
        FOR_RANGE (i32, x, gsize.x) {
            const auto t = y * gsize.x + x;

            const Element_Tile& element_tile = game_map.element_tiles[t];

            auto type = element_tile.type;

            if (type == Element_Tile_Type::Building  //
                || type == Element_Tile_Type::Flag   //
                || type == Element_Tile_Type::Road)
            {
                auto tex
                    = Get_Road_Texture_Number(game_map.element_tiles, v2i16(x, y), gsize);
                auto& tile_id               = element_tilemap.tiles[t];
                tile_id                     = global_road_starting_tile_id + tex;
                element_tilemap.textures[t] = rstate.road_textures[tex];

                if (element_tile.type == Element_Tile_Type::Building)
                    Add_Building_Sprite(
                        rstate, game_map, {x, y}, element_tile.building_id, ctx
                    );

                if (element_tile.type == Element_Tile_Type::Flag)
                    Set_Flag_Tile(rstate, game_map, {x, y}, ctx);
            }
        }
    }
    // --- Element Tiles End ---

    rstate.zoom        = 1;
    rstate.zoom_target = 1;
    rstate.cell_size   = 32;

    ui_state.buildables_panel_params.smart_stretchable  = true;
    ui_state.buildables_panel_params.stretch_paddings_h = {6, 6};
    ui_state.buildables_panel_params.stretch_paddings_v = {5, 6};
    // DEBUG_Load_Texture(
    //     arena,
    //     non_persistent_arena,
    //     "ui/buildables_panel",
    //     ui_state.buildables_panel_background
    // );
    // DEBUG_Load_Texture(
    //     arena,
    //     non_persistent_arena,
    //     "ui/buildables_placeholder",
    //     ui_state.buildables_placeholder_background
    // );

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

void Deinit_Renderer(Game_State& state, MCTX) {
    CTX_ALLOCATOR;

    auto& rstate = *state.renderer_state;

    // TODO: deinit `rstate.sprites`

    FOR_RANGE (int, i, rstate.tilemaps_count) {
        auto& tilemap = rstate.tilemaps[i];

        auto tiles_count = tilemap.size.x * tilemap.size.y;

        FREE(tilemap.tiles, sizeof(Tile_ID) * tiles_count);
        FREE(tilemap.textures, sizeof(Texture_ID) * tiles_count);
        tilemap.tiles    = nullptr;
        tilemap.textures = nullptr;
    }

    if (rstate.rendering_indices_buffer_size > 0) {
        FREE(rstate.rendering_indices_buffer, rstate.rendering_indices_buffer_size);
        rstate.rendering_indices_buffer      = nullptr;
        rstate.rendering_indices_buffer_size = 0;
    }
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

    auto matrix     = projection * model;
    v3f  vertices[] = {{0, 0, 1}, {0, 1, 1}, {1, 0, 1}, {1, 1, 1}};
    for (auto& vertex : vertices) {
        vertex = matrix * vertex;
        // Converting from homogeneous to eucledian
        // vertex.x = vertex.x / vertex.z;
        // vertex.y = vertex.y / vertex.z;
    }

    f32 texture_vertices[] = {x0, y0, x0, y1, x1, y0, x1, y1};
    for (int i : {0, 1, 2, 2, 1, 3}) {
        // TODO: How bad is that there are 2 vertices duplicated?
        glTexCoord2f(texture_vertices[2 * i], texture_vertices[2 * i + 1]);
        glVertex2f(vertices[i].x, vertices[i].y);
    }
};

C_Texture* Get_Texture(Atlas& atlas, Texture_ID id) {
    Assert(id != Texture_ID_Missing);

    for (auto texture : Iter(&atlas.textures)) {
        if (texture->id == id)
            return texture;
    }

    INVALID_PATH;
    return nullptr;
}

void Draw_UI_Sprite(
    f32                  tex_coord_x0,
    f32                  tex_coord_x1,
    f32                  tex_coord_y0,
    f32                  tex_coord_y1,
    v2f                  gl_pos,
    v2f                  gl_size,
    v2f                  anchor,
    BF_Color             color,
    Game_Renderer_State& rstate
) {
    std::swap(tex_coord_y0, tex_coord_y1);

    f32 xx0 = gl_pos.x + gl_size.x * (0 - anchor.x);
    f32 xx1 = gl_pos.x + gl_size.x * (1 - anchor.x);
    f32 yy0 = gl_pos.y + gl_size.y * (0 - anchor.y);
    f32 yy1 = gl_pos.y + gl_size.y * (1 - anchor.y);

    f32 verticesss[] = {
        xx0, yy0, 0, color.r, color.g, color.b, tex_coord_x0, tex_coord_y0,  //
        xx1, yy0, 0, color.r, color.g, color.b, tex_coord_x1, tex_coord_y0,  //
        xx0, yy1, 0, color.r, color.g, color.b, tex_coord_x0, tex_coord_y1,  //
        xx0, yy1, 0, color.r, color.g, color.b, tex_coord_x0, tex_coord_y1,  //
        xx1, yy0, 0, color.r, color.g, color.b, tex_coord_x1, tex_coord_y0,  //
        xx1, yy1, 0, color.r, color.g, color.b, tex_coord_x1, tex_coord_y1,  //
    };

    GLuint vao;
    glGenVertexArrays(1, &vao);
    GLuint vbo;
    glGenBuffers(1, &vbo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verticesss), verticesss, GL_STATIC_DRAW);

    // 3. then set our vertex attributes pointers
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(f32), (void*)(0));
    glVertexAttribPointer(
        1, 3, GL_FLOAT, GL_FALSE, 8 * sizeof(f32), (void*)(3 * sizeof(f32))
    );
    glVertexAttribPointer(
        2, 2, GL_FLOAT, GL_FALSE, 8 * sizeof(f32), (void*)(6 * sizeof(f32))
    );
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);

    // ..:: Drawing code (in render loop) :: ..
    glUseProgram(rstate.sprites_shader_program);
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    glBindVertexArray(0);
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
    Atlas&                  atlas,
    const Texture_ID        id,
    const f32               gl_pos_x0,
    const f32               gl_pos_x3,
    const f32               gl_pos_y0,
    const f32               gl_pos_y3,
    const UI_Sprite_Params& sprite_params,
    const v2i16             sprite_size,
    const f32               in_scale,
    Game_Renderer_State&    rstate
) {
    const auto& tex = *Get_Texture(atlas, id);

    // Нахождение координат разреза растягиваемой текстуры:
    auto x0 = tex.pos_inside_atlas.x;
    auto x1 = tex.pos_inside_atlas.x + (f32)tex.size.x / (f32)atlas.size.x;
    auto y0 = tex.pos_inside_atlas.y;
    auto y1 = tex.pos_inside_atlas.y + (f32)tex.size.y / (f32)atlas.size.y;

    auto& pad_h = sprite_params.stretch_paddings_h;
    auto& pad_v = sprite_params.stretch_paddings_v;

    f32 tex_coord_x1 = (f32)pad_h.x / (f32)tex.size.x;
    f32 tex_coord_x2 = (f32)pad_h.y / (f32)tex.size.x;
    f32 tex_coord_y1 = (f32)pad_v.x / (f32)tex.size.y;
    f32 tex_coord_y2 = (f32)pad_v.y / (f32)tex.size.y;

    Assert(tex_coord_y1 * in_scale + tex_coord_y2 * in_scale <= 1);
    Assert(tex_coord_x1 * in_scale + tex_coord_x2 * in_scale <= 1);

    f32 tex_coords_x[] = {0, tex_coord_x1, 1 - tex_coord_x2, 1};
    f32 tex_coords_y[] = {0, tex_coord_y1, 1 - tex_coord_y2, 1};

    // Нахождение следующих позиций для рендеринга на экране:
    //
    // .-.-.-* gl_p3
    // | | | |
    // .-.-*-. gl_p2
    // | | | |
    // .-*-.-. gl_p1
    // | | | |
    // *-.-.-. gl_p0
    //
    auto gl_p0 = v3f(gl_pos_x0, gl_pos_y0, 1);
    auto gl_p3 = v3f(gl_pos_x3, gl_pos_y3, 1);
    auto dx    = gl_pos_x3 - gl_pos_x0;
    auto dy    = gl_pos_y3 - gl_pos_y0;
    auto dp1   = v3f(
        (f32)pad_h.x * in_scale * dx / (f32)sprite_size.x,
        (f32)pad_v.x * in_scale * dy / (f32)sprite_size.y,
        0
    );
    auto dp2 = v3f(
        (f32)pad_h.y * in_scale * dx / (f32)sprite_size.x,
        (f32)pad_v.y * in_scale * dy / (f32)sprite_size.y,
        0
    );
    v3f gl_p1 = gl_p0 + dp1;
    v3f gl_p2 = gl_p3 - dp2;

    v3f points[] = {gl_p0, gl_p1, gl_p2, gl_p3};

    FOR_RANGE (int, y, 3) {
        auto tex_y0 = 1 - tex_coords_y[3 - y];
        auto tex_y1 = 1 - tex_coords_y[2 - y];
        auto gl_y0  = points[2 - y].y;
        auto sy     = points[3 - y].y - points[2 - y].y;

        FOR_RANGE (int, x, 3) {
            auto tex_x0 = tex_coords_x[x];
            auto tex_x1 = tex_coords_x[x + 1];
            auto gl_x0  = points[x].x;
            auto sx     = points[x + 1].x - gl_x0;
            Assert(sx >= 0);

            Draw_UI_Sprite(
                Lerp(x0, x1, tex_x0),
                Lerp(x0, x1, tex_x1),
                Lerp(y0, y1, tex_y0),
                Lerp(y0, y1, tex_y1),
                v2f(gl_x0, gl_y0),
                v2f(sx, sy),
                v2f(0, 0),
                BF_Color_White,
                rstate
            );
        }
    }
}

struct Get_Buildable_Textures_Result {
    size_t      deallocation_size;
    Texture_ID* textures;
};

Get_Buildable_Textures_Result
Get_Buildable_Textures(Arena& trash_arena, Game_State& state) {
    auto& rstate   = Assert_Deref(state.renderer_state);
    auto& ui_state = Assert_Deref(rstate.ui_state);

    Get_Buildable_Textures_Result res{};
    auto allocation_size = sizeof(GLuint) * ui_state.buildables_count;

    res.deallocation_size = allocation_size;
    res.textures = Allocate_Array(trash_arena, Texture_ID, ui_state.buildables_count);

    FOR_RANGE (int, i, ui_state.buildables_count) {
        auto& buildable = ui_state.buildables[i];

        switch (buildable.type) {
        case Item_To_Build_Type::Road: {
            res.textures[i] = rstate.road_textures[15];
        } break;

        case Item_To_Build_Type::Building: {
            res.textures[i] = buildable.scriptable_building->texture;
        } break;

        default:
            INVALID_PATH;
        }
    }

    return res;
}

v2f Query_Texture_Pos_Inside_Atlas(Atlas& atlas, Tilemap& tilemap, i16 x, i16 y) {
    auto id = tilemap.textures[y * tilemap.size.x + x];
    Assert(id != Texture_ID_Missing);
    return Get_Texture(atlas, id)->pos_inside_atlas;
}

void Map_Sprites_With_Textures(
    Game_Renderer_State&                                    rstate,
    std::invocable<C_Texture&, Entity_ID, C_Sprite&> auto&& func
) {
    for (auto [entity_id, sprite] : Iter(&rstate.sprites)) {
        for (auto texture : Iter(&rstate.atlas.textures)) {
            if (sprite->texture != texture->id)
                continue;

            func(*texture, entity_id, *sprite);
        }
    }
}

glm::mat3 Get_W2RelScreen_Matrix(
    v2i gsize,
    v2i screen_size,
    v2f pan_pos,
    v2f pan_offset,
    f32 zoom,
    i16 cell_size
) {
    auto mat = glm::mat3(1);

    mat = glm::translate(mat, v2f(0, 1));
    mat = glm::scale(mat, v2f(1.0f / screen_size.x, -1.0f / screen_size.y));
    // Смещение карты игроком.
    mat = glm::translate(mat, pan_pos + pan_offset);
    // Масштабирование карты игроком.
    mat = glm::scale(mat, v2f(zoom, zoom));
    // Перемещаем центр карты в середину экрана.
    mat = glm::translate(mat, v2f(screen_size) / 2.0f);
    // Масштабируем до размера клетки.
    mat = glm::scale(mat, v2f_one * (f32)cell_size);
    // Ставим центр - середину карты.
    mat = glm::translate(mat, -(v2f)gsize / 2.0f);

    return mat;
}

OpenGL_Framebuffer Create_Framebuffer(v2i size) {
    // NOTE: Build the texture that will serve
    // as the color attachment for the framebuffer.
    GLuint color_texture;
    glGenTextures(1, &color_texture);
    glBindTexture(GL_TEXTURE_2D, color_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTexImage2D(
        GL_TEXTURE_2D, 0, GL_RGBA, size.x, size.y, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL
    );

    // Build the framebuffer.
    GLuint framebuffer_id;
    glGenFramebuffers(1, &framebuffer_id);
    glBindFramebuffer(GL_FRAMEBUFFER, framebuffer_id);
    glFramebufferTexture2D(
        GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, color_texture, 0
    );

    // Check.
    auto check = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    Assert(check == GL_FRAMEBUFFER_COMPLETE);
    Check_OpenGL_Errors();

    OpenGL_Framebuffer res{};
    res.id    = framebuffer_id;
    res.size  = size;
    res.color = color_texture;
    return res;
}

void Delete_Framebuffer(const OpenGL_Framebuffer& fb) {
    glDeleteFramebuffers(1, &fb.id);
    glDeleteTextures(1, &fb.color);
}

glm::mat3 Get_UI_Projection_Matrix(v2f screen_size) {
    auto mat = glm::mat3(1);
    mat      = glm::scale(mat, v2f(1, -1));
    mat      = glm::translate(mat, v2f(-1, 1));
    mat      = glm::scale(mat, v2f(2, -2) / screen_size);
    return mat;
}

void Render_UI(Game_State& state, f32 dt, MCTX) {
    auto& rstate   = *state.renderer_state;
    auto& ui_state = *rstate.ui_state;

    auto& game_map = state.game_map;

    Arena& trash_arena = state.trash_arena;
    TEMP_USAGE(trash_arena);

    Game_Bitmap& bitmap = Assert_Deref(rstate.bitmap);

    const auto swidth  = (f32)bitmap.width;
    const auto sheight = (f32)bitmap.height;

    const auto PROJECTION = Get_UI_Projection_Matrix({swidth, sheight});

    // Рисование UI плашки слева.
    {
        glBindTexture(GL_TEXTURE_2D, rstate.atlas.texture.id);

        const auto  sprite_params = ui_state.buildables_panel_params;
        const auto& pad_h         = sprite_params.stretch_paddings_h;
        const auto& pad_v         = sprite_params.stretch_paddings_v;

        const auto& panel_texture
            = *Get_Texture(rstate.atlas, ui_state.buildables_panel_texture);

        const auto& placeholder_texture
            = *Get_Texture(rstate.atlas, ui_state.buildables_placeholder_texture);

        const auto& psize = placeholder_texture.size;

        const auto in_scale      = ui_state.buildables_panel_in_scale;
        const v2f  sprite_anchor = ui_state.buildables_panel_sprite_anchor;

        const v2f  padding          = ui_state.padding;
        const f32  placeholders_gap = ui_state.placeholders_gap;
        const auto placeholders     = ui_state.placeholders;
        const auto panel_size       = v2f(
            psize.x + 2 * padding.x,
            2 * padding.y + placeholders_gap * (placeholders - 1) + placeholders * psize.y
        );

        const auto outer_anchor         = ui_state.buildables_panel_container_anchor;
        const auto outer_container_size = v2i(swidth, sheight);
        const auto outer_pos            = v2f(
            outer_container_size.x * outer_anchor.x,
            outer_container_size.y * outer_anchor.y
        );

        auto VIEW = glm::mat3(1);
        VIEW      = glm::translate(VIEW, v2f((int)outer_pos.x, (int)outer_pos.y));
        VIEW      = glm::scale(VIEW, v2f(ui_state.scale));

        {
            auto MODEL = glm::mat3(1);
            MODEL      = glm::scale(MODEL, v2f(panel_size));

            auto p0_local = MODEL * v3f(v2f_zero - sprite_anchor, 1);
            auto p1_local = MODEL * v3f(v2f_one - sprite_anchor, 1);

            auto p0 = PROJECTION * VIEW * p0_local;
            auto p1 = PROJECTION * VIEW * p1_local;

            Draw_Stretchable_Sprite(
                rstate.atlas,
                ui_state.buildables_panel_texture,
                p0.x,
                p1.x,
                p0.y,
                p1.y,
                sprite_params,
                panel_size,
                in_scale,
                rstate
            );

            auto origin = (p1_local + p0_local) / 2.0f;

            // Aligning items in a column
            // justify-content: center

            FOR_RANGE (int, i, placeholders) {
                const auto& tex = placeholder_texture;

                auto drawing_point = origin;
                drawing_point.y -= (placeholders - 1) * (psize.y + placeholders_gap) / 2;
                drawing_point.y += i * (placeholders_gap + psize.y);

                auto p     = PROJECTION * VIEW * drawing_point;
                auto s     = PROJECTION * VIEW * v3f(psize, 0);
                auto color = (i == ui_state.selected_buildable_index)
                                 ? ui_state.selected_buildable_color
                                 : ui_state.not_selected_buildable_color;
                Draw_UI_Sprite(
                    tex.pos_inside_atlas.x,
                    tex.pos_inside_atlas.x
                        + (f32)tex.size.x / (f32)rstate.atlas.texture.size.x,
                    tex.pos_inside_atlas.y,
                    tex.pos_inside_atlas.y
                        + (f32)tex.size.y / (f32)rstate.atlas.texture.size.y,
                    p,
                    s,
                    v2f_one / 2.0f,
                    color,
                    rstate
                );
            }

            auto buildable_textures = Get_Buildable_Textures(trash_arena, state);

            auto buildable_size = v2f(psize) * (2.0f / 3.0f);
            FOR_RANGE (int, i, placeholders) {
                const auto& tex
                    = *Get_Texture(rstate.atlas, buildable_textures.textures[i]);

                auto drawing_point = origin;
                drawing_point.y -= (placeholders - 1) * (psize.y + placeholders_gap) / 2;
                drawing_point.y += i * (placeholders_gap + psize.y);

                auto p = PROJECTION * VIEW * drawing_point;
                auto s = PROJECTION * VIEW * v3f(buildable_size, 0);

                auto color = (i == ui_state.selected_buildable_index)
                                 ? ui_state.selected_buildable_color
                                 : ui_state.not_selected_buildable_color;

                Draw_UI_Sprite(
                    tex.pos_inside_atlas.x,
                    tex.pos_inside_atlas.x
                        + (f32)tex.size.x / (f32)rstate.atlas.texture.size.x,
                    tex.pos_inside_atlas.y,
                    tex.pos_inside_atlas.y
                        + (f32)tex.size.y / (f32)rstate.atlas.texture.size.y,
                    p,
                    s,
                    v2f_one / 2.0f,
                    color,
                    rstate
                );
            }
        }
    }

    // Дебаг-рисование штук для чувачков.
    {
        for (auto [human_id, human_ptr] : Iter(&game_map.humans)) {
            auto& human = Assert_Deref(human_ptr);

#if 0
            if (human.moving.path.count > 0) {
                auto last_p = human.moving.path.base[human.moving.path.count - 1];
                auto p = W2GL * v3f((v2f(last_p) + v2f_one / 2.0f) * (f32)cell_size, 1);

                glPointSize(12);

                glBlendFunc(GL_ONE, GL_ZERO);

                glBegin(GL_POINTS);
                glVertex2f(p.x, p.y);
                glEnd();
            }
#endif

#if 0
            if (human.moving.to.has_value()) {
                auto p = W2GL * v3f(v2f(human.moving.to.value()) * (f32)cell_size, 1);

                glPointSize(12);

                glBlendFunc(GL_ONE, GL_ZERO);

                glBegin(GL_POINTS);
                glVertex2f(p.x, p.y);
                glEnd();
            }
#endif
        }
    }

    // Дебаг-рисование сегментов.
#if 0
    {
        const BF_Color colors[] = {
            {1, 0, 0},  //
            {0, 1, 0},  //
            {0, 0, 1},  //
            {1, 1, 0},  //
            {0, 1, 1},  //
            {1, 0, 1},  //
            {1, 1, 1}   //
        };
        size_t               colors_count  = sizeof(colors) / sizeof(colors[0]);
        local_persist size_t segment_index = 0;
        segment_index++;
        if (segment_index >= colors_count)
            segment_index = 0;

        int rendered_segments = 0;
        for (auto segment_ptr : Iter(&game_map.segments)) {
            auto& segment = *segment_ptr;
            auto& graph   = segment.graph;

            rendered_segments++;

            FOR_RANGE (int, y, graph.size.y) {
                FOR_RANGE (int, x, graph.size.x) {
                    auto node = *(graph.nodes + y * graph.size.x + x);
                    if (!node)
                        continue;

                    v2f center = v2f(x, y) + v2f(graph.offset.x, graph.offset.y)
                                 + v2f_one / 2.0f;
                    FOR_RANGE (int, ii, 4) {
                        auto dir = (Direction)ii;
                        if (!Graph_Node_Has(node, dir))
                            continue;

                        auto offsetted = center + (v2f)(As_Offset(dir)) / 2.0f;

                        auto p1 = W2GL * v3f(center * (f32)cell_size, 1);
                        auto p2 = W2GL * v3f(offsetted * (f32)cell_size, 1);
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

void Render(Game_State& state, f32 dt, MCTX) {
    CTX_ALLOCATOR;
    ZoneScoped;

    Arena& trash_arena = state.trash_arena;
    TEMP_USAGE(trash_arena);

    auto&        rstate   = Assert_Deref(state.renderer_state);
    auto&        game_map = state.game_map;
    Game_Bitmap& bitmap   = Assert_Deref(rstate.bitmap);

    const auto gsize     = game_map.size;
    const auto swidth    = (f32)bitmap.width;
    const auto sheight   = (f32)bitmap.height;
    const auto cell_size = 32;

    if (swidth <= 0.0f || sheight <= 0.0f)
        return;

    // Обработка зума карты к курсору.
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

        auto PROJECTION = glm::mat3(1);
        PROJECTION      = glm::translate(PROJECTION, v2f(0, 1));
        PROJECTION      = glm::scale(PROJECTION, v2f(1 / swidth, -1 / sheight));
        PROJECTION      = glm::translate(PROJECTION, rstate.pan_pos + rstate.pan_offset);
        PROJECTION      = glm::scale(PROJECTION, v2f(rstate.zoom, rstate.zoom));
        PROJECTION      = glm::translate(PROJECTION, v2f(swidth, sheight) / 2.0f);
        PROJECTION      = glm::translate(PROJECTION, -(v2f)gsize * (f32)cell_size / 2.0f);

        auto d = PROJECTION * v3f(cursor_d.x, cursor_d.y, 0);
        rstate.pan_pos += cursor_d * (f32)(rstate.zoom * cell_size);

        auto d3 = World_To_Screen(state, v2f(0, 0));
        ImGui::Text("d3 %.3f.%.3f", d3.x, d3.y);

        auto d4 = World_Pos_To_Tile(cursor_on_tilemap_pos2);
        ImGui::Text("d4 %d.%d", d4.x, d4.y);
    }

    glClear(GL_COLOR_BUFFER_BIT);
    Check_OpenGL_Errors();

    // Рисование заднего синего фона.
    {
        glUseProgram(0);

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

    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);
    Assert(bitmap.width == viewport[2]);
    Assert(bitmap.height == viewport[3]);
    v2f v2f_screen{viewport[2], viewport[3]};

    // Матрица перевода координат мира
    // в координаты экрана в диапазоне от 0 до 1. Y вверх.
    const auto W2RelScreen = Get_W2RelScreen_Matrix(
        gsize,
        {viewport[2], viewport[3]},
        rstate.pan_pos,
        rstate.pan_offset,
        rstate.zoom,
        cell_size
    );
    const auto RelScreen2W = glm::inverse(W2RelScreen);

    // Матрица перевода координат мира
    // в координаты OpenGL от -1 до 1. Y вниз.
    auto W2GL = glm::mat3(1);
    W2GL      = glm::translate(W2GL, v2f(-1, 1));
    W2GL      = glm::scale(W2GL, v2f(2, -2));
    W2GL *= W2RelScreen;

    // Рисование tilemap.
    if (!rstate.shaders_compilation_failed) {
        auto tilemap_framebuffer = Create_Framebuffer(v2i(viewport[2], viewport[3]));
        defer {
            Delete_Framebuffer(tilemap_framebuffer);
        };

        glUseProgram(rstate.tilemap_shader_program);
        glBindFramebuffer(GL_FRAMEBUFFER, tilemap_framebuffer.id);
        glBindTexture(GL_TEXTURE_2D, rstate.atlas.texture.id);

        const auto GL2W = glm::inverse(W2GL);
        glUniformMatrix3fv(
            rstate.tilemap_shader_gl2w_location, 1, GL_FALSE, (GLfloat*)(&GL2W)
        );

        glUniformMatrix3fv(
            rstate.sprites_shader_gl2w_location, 1, GL_FALSE, (GLfloat*)(&GL2W)
        );

        glUniform2i(rstate.tilemap_shader_resolution_location, viewport[2], viewport[3]);

        glUniformMatrix3fv(
            rstate.tilemap_shader_relscreen2w_location,
            1,
            GL_FALSE,
            (GLfloat*)(&RelScreen2W)
        );

        // Выделение памяти под rendering_indices_buffer.
        auto bytes_per_tile  = 2 * sizeof(GLfloat);
        auto tiles_count     = rstate.tilemaps[0].size.x * rstate.tilemaps[0].size.y;
        auto required_memory = bytes_per_tile * tiles_count;

        if (rstate.rendering_indices_buffer == nullptr) {
            rstate.rendering_indices_buffer      = ALLOC(required_memory);
            rstate.rendering_indices_buffer_size = required_memory;
        }

        glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE);
        glBlendEquation(GL_FUNC_ADD);
        glEnable(GL_BLEND);

        FOR_RANGE (int, i, rstate.tilemaps_count) {
            auto& tilemap = rstate.tilemaps[i];

            {
                TEMP_USAGE(trash_arena);
                auto checkbox_title
                    = Allocate_Formatted_String(trash_arena, "render tilemap %d", i);

                ImGui::Checkbox(checkbox_title, &tilemap.debug_rendering_enabled);
            }

            if (!tilemap.debug_rendering_enabled)
                continue;

            // Заполнение rendering_indices_buffer.
            size_t t = 0;
            FOR_RANGE (int, y, tilemap.size.y) {
                FOR_RANGE (int, x, tilemap.size.x) {
                    GLfloat* px = ((GLfloat*)rstate.rendering_indices_buffer) + t;
                    GLfloat* py = ((GLfloat*)rstate.rendering_indices_buffer) + t + 1;

                    auto tile = tilemap.tiles[y * tilemap.size.x + x];
                    if (tile) {
                        auto pos
                            = Query_Texture_Pos_Inside_Atlas(rstate.atlas, tilemap, x, y);
                        *px = pos.x;
                        *py = pos.y;
                    }
                    else {
                        *px = -1;
                        *py = -1;
                    }

                    t += 2;
                }
            }

            GLuint ssbo;
            glGenBuffers(1, &ssbo);
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
            glBufferData(
                GL_SHADER_STORAGE_BUFFER,
                required_memory,
                rstate.rendering_indices_buffer,
                GL_DYNAMIC_DRAW
            );
            glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, ssbo);
            glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
            Check_OpenGL_Errors();

            // Рисование квада tilemap.
            auto p0 = W2GL * v3f(0, 0, 1);
            auto p1 = W2GL * v3f(tilemap.size.x, 0, 1);
            auto p2 = W2GL * v3f(0, tilemap.size.y, 1);
            auto p3 = W2GL * v3f(tilemap.size.x, tilemap.size.y, 1);
            Assert(p0.z == 1);
            Assert(p3.z == 1);

            v2f vertices_with_tex_coords[] = {
                v2f(p0),
                v2f(0, 0),
                v2f(p1),
                v2f(1, 0),
                v2f(p2),
                v2f(0, 1),
                v2f(p2),
                v2f(0, 1),
                v2f(p1),
                v2f(1, 0),
                v2f(p3),
                v2f(1, 1),
            };

            GLuint vao = 0;
            glGenVertexArrays(1, &vao);
            GLuint vbo = 0;
            glGenBuffers(1, &vbo);

            glBindVertexArray(vao);
            glBindBuffer(GL_ARRAY_BUFFER, vbo);
            glBufferData(
                GL_ARRAY_BUFFER,
                sizeof(vertices_with_tex_coords),
                vertices_with_tex_coords,
                GL_STATIC_DRAW
            );

            // 3. then set our vertex attributes pointers
            glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(f32), (void*)0);
            glVertexAttribPointer(
                1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(f32), (void*)(2 * sizeof(f32))
            );
            glEnableVertexAttribArray(0);
            glEnableVertexAttribArray(1);

            // ..:: Drawing code (in render loop) :: ..
            // glUseProgram(rstate.sprites_shader_program);
            glBindVertexArray(vao);
            glDrawArrays(GL_TRIANGLES, 0, 6);

            glBindVertexArray(0);
            glDeleteVertexArrays(1, &vao);
            glDeleteBuffers(1, &vbo);
            Check_OpenGL_Errors();

            SANITIZE;
        }

        glBindFramebuffer(GL_READ_FRAMEBUFFER, tilemap_framebuffer.id);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glBlitFramebuffer(
            0,
            0,
            tilemap_framebuffer.size.x,
            tilemap_framebuffer.size.y,
            0,
            0,
            viewport[2],
            viewport[3],
            GL_COLOR_BUFFER_BIT,
            GL_NEAREST
        );
    }

    Check_OpenGL_Errors();

    glBindTexture(GL_TEXTURE_2D, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
    Check_OpenGL_Errors();

    // Рисование спрайтов.
    Memory_Buffer vertex_data{ctx};
    defer {
        vertex_data.Deinit(ctx);
    };

    auto    sprites_count = 0;
    GLfloat zero          = 0;
    GLfloat one           = 1;
    Map_Sprites_With_Textures(
        rstate,
        [&](C_Texture& tex,
            Entity_ID /* sprite_id */,
            C_Sprite& s  //
        ) {
            auto texture_id   = tex.id;
            auto required_mem = sizeof(GLfloat) * 7 * 6;
            vertex_data.Reserve(vertex_data.max_count + required_mem, ctx);

            // TODO: rotation
            // TODO: filter
            f32 x0 = s.pos.x + s.scale.x * (s.anchor.x - 1);
            f32 x1 = s.pos.x + s.scale.x * (s.anchor.x - 0);
            f32 y0 = s.pos.y + s.scale.y * (s.anchor.y - 1);
            f32 y1 = s.pos.y + s.scale.y * (s.anchor.y - 0);

            v3f p0 = W2GL * v3f(x0, y0, 1);
            v3f p1 = W2GL * v3f(x1, y1, 1);
            Assert(p0.z == 1);
            Assert(p1.z == 1);
            x0 = p0.x;
            y0 = p0.y;
            x1 = p1.x;
            y1 = p1.y;

            f32 ax0 = tex.pos_inside_atlas.x;
            f32 ax1 = tex.pos_inside_atlas.x + (f32(tex.size.x) / rstate.atlas.size.x);
            f32 ay1 = tex.pos_inside_atlas.y;
            f32 ay0 = tex.pos_inside_atlas.y + (f32(tex.size.y) / rstate.atlas.size.y);

            v2f verticess[] = {
                v2f(x0, y0),
                v2f(x1, y0),
                v2f(x0, y1),
                v2f(x0, y1),
                v2f(x1, y0),
                v2f(x1, y1),
            };

            v2f texture_positions[] = {
                v2f(ax0, ay0),
                v2f(ax1, ay0),
                v2f(ax0, ay1),
                v2f(ax0, ay1),
                v2f(ax1, ay0),
                v2f(ax1, ay1),
            };

            FOR_RANGE (int, i, 6) {
                v2f vertex        = verticess[i];
                v2f texture_coord = texture_positions[i];
                vertex_data.Add_Unsafe((void*)&vertex, sizeof(vertex));
                vertex_data.Add_Unsafe((void*)&v3f_one, sizeof(v3f_one));  // TODO: color
                vertex_data.Add_Unsafe((void*)&texture_coord, sizeof(texture_coord));
            }

            sprites_count++;
        }
    );

    {
        glUseProgram(rstate.sprites_shader_program);
        glBindTexture(GL_TEXTURE_2D, rstate.atlas.texture.id);
        GLuint vao = 0;
        glGenVertexArrays(1, &vao);
        GLuint vbo = 0;
        glGenBuffers(1, &vbo);

        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(
            GL_ARRAY_BUFFER, vertex_data.count, vertex_data.base, GL_STATIC_DRAW
        );

        // 3. then set our vertex attributes pointers
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 7 * sizeof(f32), (void*)(0));
        glVertexAttribPointer(
            1, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(f32), (void*)(2 * sizeof(f32))
        );
        glVertexAttribPointer(
            2, 2, GL_FLOAT, GL_FALSE, 7 * sizeof(f32), (void*)(5 * sizeof(f32))
        );
        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);
        glEnableVertexAttribArray(2);

        // ..:: Drawing code (in render loop) :: ..
        // glUseProgram(rstate.sprites_shader_program);
        glBindVertexArray(vao);
        glDrawArrays(GL_TRIANGLES, 0, 6 * sprites_count);

        glBindVertexArray(0);
        glDeleteVertexArrays(1, &vao);
        glDeleteBuffers(1, &vbo);
        Check_OpenGL_Errors();
    }

    SANITIZE;

    Render_UI(state, dt, ctx);

    SANITIZE;
}

// NOTE: Game_State& state, v2i16 pos, Item_To_Build item
On_Item_Built_function(Renderer_OnItemBuilt) {
    Assert(state.renderer_state != nullptr);

    CTX_ALLOCATOR;

    auto& rstate   = *state.renderer_state;
    auto& game_map = state.game_map;
    auto  gsize    = game_map.size;

    // Тут рисуются дороги.
    auto& roads_tilemap = rstate.tilemaps[rstate.element_tilemap_index];
    // Тут рисуются флаги и здания.
    auto& placeables_tilemap = rstate.tilemaps[rstate.element_tilemap_index + 1];

    auto t = pos.y * gsize.x + pos.x;

    auto& element_tile = game_map.element_tiles[t];

    switch (element_tile.type) {
    case Element_Tile_Type::Building:
    case Element_Tile_Type::Road: {
        placeables_tilemap.tiles[pos.y * gsize.x + pos.x] = 0;
    } break;

    case Element_Tile_Type::Flag: {
        placeables_tilemap.tiles[pos.y * gsize.x + pos.x] = global_flag_starting_tile_id;
    } break;

    default:
        INVALID_PATH;
    }

    if (element_tile.type != Element_Tile_Type::Building)
        Assert(element_tile.building_id == Building_ID_Missing);

    if (element_tile.type == Element_Tile_Type::Building)
        Add_Building_Sprite(rstate, game_map, pos, element_tile.building_id, ctx);

    if (element_tile.type == Element_Tile_Type::Flag)
        Set_Flag_Tile(rstate, game_map, pos, ctx);

    for (auto offset : v2i16_adjacent_offsets_including_0) {
        auto new_pos = pos + offset;
        if (!Pos_Is_In_Bounds(new_pos, gsize))
            continue;

        auto t = new_pos.y * gsize.x + new_pos.x;

        auto& element_tile = game_map.element_tiles[t];

        switch (element_tile.type) {
        case Element_Tile_Type::Building:
        case Element_Tile_Type::Flag:
        case Element_Tile_Type::Road: {
            auto  tex = Get_Road_Texture_Number(game_map.element_tiles, new_pos, gsize);
            auto& tile_id = roads_tilemap.tiles[t];
            tile_id       = global_road_starting_tile_id + tex;

            if (offset == v2i16_zero && element_tile.type == Element_Tile_Type::Building)
            {
                Assert(false);
                INVALID_PATH;
            }
        } break;

        case Element_Tile_Type::None:
            break;

        default:
            INVALID_PATH;
        }
    }
}
