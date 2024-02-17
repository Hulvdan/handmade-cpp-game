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
    // NOTE(hulvdan): Mapping things below this line is not technically correct.
    // Earlier versions of BMP Info Headers don't anything specified below
    u32 compression;
};
#pragma pack(pop)

const i32 global_road_starting_tile_id = 4;
const i32 global_flag_starting_tile_id = global_road_starting_tile_id + 16;

struct Load_BMP_RGBA_Result {
    bool success;
    u8* output;
    u16 width;
    u16 height;
};

Load_BMP_RGBA_Result Load_BMP_RGBA(Arena& arena, const u8* filedata) {
    Load_BMP_RGBA_Result res = {};

    auto& header = *(Debug_BMP_Header*)filedata;

    if (header.signature != *(u16*)"BM") {
        // TODO(hulvdan): Diagnostic. Not a BMP file
        assert(false);
        return res;
    }

    auto dib_size = header.dib_header_size;
    if (dib_size != 125 && dib_size != 56) {
        // TODO(hulvdan): Is not yet implemented algorithm
        assert(false);
        return res;
    }

    auto pixels_count = (u32)header.width * header.height;
    auto total_bytes = (size_t)pixels_count * 4;

    res.output = Allocate_Array(arena, u8, total_bytes);
    res.width = header.width;
    res.height = header.height;

    assert(header.planes == 1);
    assert(header.bits_per_pixel == 32);
    assert(header.file_size - header.data_offset == total_bytes);

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
    assert(!glGetError());

    glTexImage2D(
        GL_TEXTURE_2D, 0, GL_RGBA8, texture.size.x, texture.size.y, 0, GL_BGRA_EXT,
        GL_UNSIGNED_BYTE, texture.base);
    assert(!glGetError());
}

void DEBUG_Load_Texture(
    Arena& arena,
    Arena& temp_arena,
    const char* texture_name,
    Loaded_Texture& out_texture  //
) {
    char filepath[512];
    sprintf(filepath, "assets/art/%s.bmp", texture_name);

    Load_BMP_RGBA_Result bmp_result = {};
    {
        auto load_result = Debug_Load_File_To_Arena(filepath, temp_arena);
        assert(load_result.success);
        DEFER(Deallocate_Array(temp_arena, u8, load_result.size));

        bmp_result = Load_BMP_RGBA(arena, load_result.output);
        assert(bmp_result.success);
    }

    out_texture.id = scast<BF_Texture_ID>(Hash32_String(texture_name));
    out_texture.size = {bmp_result.width, bmp_result.height};
    out_texture.base = bmp_result.output;
    Send_Texture_To_GPU(out_texture);
}

int Get_Road_Texture_Number(Element_Tile* element_tiles, v2i pos, v2i gsize) {
    v2i adjacent_offsets[] = {{1, 0}, {0, 1}, {-1, 0}, {0, -1}};

    Element_Tile& tile = *(element_tiles + pos.y * gsize.x + pos.x);

    int road_texture_number = 0;
    FOR_RANGE(int, i, 4) {
        auto new_pos = pos + adjacent_offsets[i];
        if (!Pos_Is_In_Bounds(new_pos, gsize))
            continue;

        Element_Tile& adjacent_tile = *(element_tiles + new_pos.y * gsize.x + new_pos.x);

        auto both_are_flags =
            adjacent_tile.type == Element_Tile_Type::Flag && tile.type == Element_Tile_Type::Flag;
        auto adjacent_is_either_road_or_flag = adjacent_tile.type == Element_Tile_Type::Road ||
            adjacent_tile.type == Element_Tile_Type::Flag;

        if (both_are_flags)
            continue;
        if (adjacent_is_either_road_or_flag)
            road_texture_number += (1 << i);
    }

    return road_texture_number;
}

void Initialize_Renderer(Game_State& state, Arena& arena, Arena& temp_arena) {
    if (state.renderer_state != nullptr && state.renderer_state->is_initialized)
        return;

    state.renderer_state = Allocate_Zeros_For(arena, Game_Renderer_State);
    auto& rstate = *state.renderer_state;
    auto& game_map = state.game_map;

    DEBUG_Load_Texture(arena, temp_arena, "human", rstate.human_texture);

    glEnable(GL_TEXTURE_2D);

    auto gsize = game_map.size;

    char texture_name[512] = {};
    FOR_RANGE(int, i, 17) {
        sprintf(texture_name, "tiles/grass_%d", i);
        DEBUG_Load_Texture(arena, temp_arena, texture_name, rstate.grass_textures[i]);
    }

    FOR_RANGE(int, i, 3) {
        sprintf(texture_name, "tiles/forest_%d", i);
        DEBUG_Load_Texture(arena, temp_arena, texture_name, rstate.forest_textures[i]);
    }

    FOR_RANGE(int, i, 16) {
        sprintf(texture_name, "tiles/road_%d", i);
        DEBUG_Load_Texture(arena, temp_arena, texture_name, rstate.road_textures[i]);
    }

    FOR_RANGE(int, i, 4) {
        sprintf(texture_name, "tiles/flag_%d", i);
        DEBUG_Load_Texture(arena, temp_arena, texture_name, rstate.flag_textures[i]);
    }

    rstate.grass_smart_tile.id = 1;
    rstate.forest_smart_tile.id = 2;
    rstate.forest_top_tile_id = 3;

    {
        auto path = "assets/art/tiles/tilerule_grass.txt";
        auto load_result = Debug_Load_File_To_Arena(path, temp_arena);
        assert(load_result.success);
        DEFER(Deallocate_Array(temp_arena, u8, load_result.size));

        auto rule_loading_result = Load_Smart_Tile_Rules(
            rstate.grass_smart_tile, arena, load_result.output, load_result.size);
        assert(rule_loading_result.success);
    }

    {
        auto path = "assets/art/tiles/tilerule_forest.txt";
        auto load_result = Debug_Load_File_To_Arena(path, temp_arena);
        assert(load_result.success);
        DEFER(Deallocate_Array(temp_arena, u8, load_result.size));

        auto rule_loading_result = Load_Smart_Tile_Rules(
            rstate.forest_smart_tile, arena, load_result.output, load_result.size);
        assert(rule_loading_result.success);
    }

    i32 max_height = 0;
    FOR_RANGE(i32, y, gsize.y) {
        FOR_RANGE(i32, x, gsize.x) {
            auto& terrain_tile = *(game_map.terrain_tiles + y * gsize.x + x);
            max_height = MAX(max_height, terrain_tile.height);
        }
    }

    rstate.tilemaps_count = 0;
    // NOTE(hulvdan): Terrain tilemaps ([0; max_height])
    rstate.terrain_tilemaps_count = max_height + 1;
    rstate.tilemaps_count += rstate.terrain_tilemaps_count;

    // NOTE(hulvdan): Terrain Resources (forests, stones, etc.)
    // Currently use 2 tilemaps:
    // 1) Forests
    // 2) Tree's top tiles
    rstate.resources_tilemap_index = rstate.tilemaps_count;
    rstate.tilemaps_count += 2;

    // NOTE(hulvdan): Element Tiles (roads, buildings, etc.)
    // Currently use 2 tilemaps:
    // 1) Roads
    // 2) Flags above roads
    rstate.element_tilemap_index = rstate.tilemaps_count;
    rstate.tilemaps_count += 2;

    rstate.tilemaps = Allocate_Array(arena, Tilemap, rstate.tilemaps_count);

    FOR_RANGE(i32, h, rstate.tilemaps_count) {
        auto& tilemap = *(rstate.tilemaps + h);
        tilemap.tiles = Allocate_Array(arena, Tile_ID, gsize.x * gsize.y);

        FOR_RANGE(i32, y, gsize.y) {
            FOR_RANGE(i32, x, gsize.x) {
                auto& tile = *(game_map.terrain_tiles + y * gsize.x + x);
                auto& tilemap_tile = *(tilemap.tiles + y * gsize.x + x);

                bool grass = tile.terrain == Terrain::Grass && tile.height >= h;
                tilemap_tile = grass * rstate.grass_smart_tile.id;
            }
        }
    }

    auto& resources_tilemap = *(rstate.tilemaps + rstate.resources_tilemap_index);
    auto& resources_tilemap2 = *(rstate.tilemaps + rstate.resources_tilemap_index + 1);
    FOR_RANGE(i32, y, gsize.y) {
        auto is_last_row = y == gsize.y - 1;
        FOR_RANGE(i32, x, gsize.x) {
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
                forest_above = tile_above == rstate.forest_smart_tile.id;
            }

            if (!forest_above)
                *(resources_tilemap2.tiles + y * gsize.x + x) = rstate.forest_top_tile_id;
        }
    }

    // --- Element Tiles ---
    auto& element_tilemap = *(rstate.tilemaps + rstate.element_tilemap_index);
    v2i adjacent_offsets[4] = {
        {1, 0},
        {0, 1},
        {-1, 0},
        {0, -1},
    };
    FOR_RANGE(i32, y, gsize.y) {
        FOR_RANGE(i32, x, gsize.x) {
            Element_Tile& element_tile = *(game_map.element_tiles + y * gsize.x + x);
            if (element_tile.type != Element_Tile_Type::Road)
                continue;

            auto tex = Get_Road_Texture_Number(game_map.element_tiles, v2i(x, y), gsize);
            auto& tile_id = *(element_tilemap.tiles + y * gsize.x + x);
            tile_id = global_road_starting_tile_id + tex;
        }
    }
    // --- Element Tiles End ---

    rstate.zoom = 1;
    rstate.zoom_target = 1;
    rstate.cell_size = 32;

    {
        auto& b = *Get_Scriptable_Building(state, global_city_hall_building_id);
        b.texture = Allocate_For(arena, Loaded_Texture);
        DEBUG_Load_Texture(arena, temp_arena, "tiles/building_house", *b.texture);
    }
    {
        auto& b = *Get_Scriptable_Building(state, global_lumberjacks_hut_building_id);
        b.texture = Allocate_For(arena, Loaded_Texture);
        DEBUG_Load_Texture(arena, temp_arena, "tiles/building_lumberjack", *b.texture);
    }

    rstate.ui_state = Allocate_Zeros_For(arena, Game_UI_State);
    auto& ui_state = *rstate.ui_state;

    ui_state.buildables_panel_params.smart_stretchable = true;
    ui_state.buildables_panel_params.stretch_paddings_h = {6, 6};
    ui_state.buildables_panel_params.stretch_paddings_v = {5, 6};
    DEBUG_Load_Texture(
        arena, temp_arena, "ui/buildables_panel", ui_state.buildables_panel_background);
    DEBUG_Load_Texture(
        arena, temp_arena, "ui/buildables_placeholder", ui_state.buildables_placeholder_background);

    rstate.is_initialized = true;
}

void Draw_Sprite(
    f32 x0,
    f32 y0,
    f32 x1,
    f32 y1,
    v2f pos,
    v2f size,
    float rotation,
    const glm::mat3& projection  //
) {
    assert(x0 < x1);
    assert(y0 < y1);

    auto model = glm::mat3(1);
    model = glm::translate(model, pos);
    // model = glm::translate(model, pos / size);
    model = glm::rotate(model, rotation);
    model = glm::scale(model, size);

    auto matrix = projection * model;
    // TODO(hulvdan): How bad is it that there are vec3, but not vec2?
    v3f vertices[] = {{0, 0, 1}, {0, 1, 1}, {1, 0, 1}, {1, 1, 1}};
    for (auto& vertex : vertices) {
        vertex = matrix * vertex;
        // Converting from homogeneous to eucledian
        // vertex.x = vertex.x / vertex.z;
        // vertex.y = vertex.y / vertex.z;
    }

    f32 texture_vertices[] = {x0, y0, x0, y1, x1, y0, x1, y1};
    for (ptrd i : {0, 1, 2, 2, 1, 3}) {
        // TODO(hulvdan): How bad is that there are 2 vertices duplicated?
        glTexCoord2f(texture_vertices[2 * i], texture_vertices[2 * i + 1]);
        glVertex2f(vertices[i].x, vertices[i].y);
    }
};

void Draw_UI_Sprite(
    f32 x0,
    f32 y0,
    f32 x1,
    f32 y1,
    v2f pos,
    v2f size,
    v2f anchor  //
) {
    assert(x0 < x1);
    assert(y0 < y1);

    v2f vertices[] = {
        {pos.x + size.x * (0 - anchor.x), pos.y + size.y * (0 - anchor.y)},
        {pos.x + size.x * (0 - anchor.x), pos.y + size.y * (1 - anchor.y)},
        {pos.x + size.x * (1 - anchor.x), pos.y + size.y * (0 - anchor.y)},
        {pos.x + size.x * (1 - anchor.x), pos.y + size.y * (1 - anchor.y)},
    };

    f32 texture_vertices[] = {x0, y0, x0, y1, x1, y0, x1, y1};
    for (ptrd i : {0, 1, 2, 2, 1, 3}) {
        // TODO(hulvdan): How bad is that there are 2 vertices duplicated?
        glTexCoord2f(texture_vertices[2 * i], texture_vertices[2 * i + 1]);
        glVertex2f(vertices[i].x, vertices[i].y);
    }
};

f32 Move_Towards(f32 value, f32 target, f32 diff) {
    assert(diff >= 0);
    auto d = target - value;
    if (d > 0)
        value += MIN(d, diff);
    else if (d < 0)
        value -= MAX(d, diff);
    return value;
}

v2f World_To_Screen(Game_State& state, v2f pos) {
    assert(state.renderer_state != nullptr);
    auto& rstate = *state.renderer_state;
    assert(rstate.bitmap != nullptr);
    Game_Bitmap& bitmap = *rstate.bitmap;

    auto swidth = (f32)bitmap.width;
    auto sheight = (f32)bitmap.height;
    auto gsize = state.game_map.size;
    auto cell_size = rstate.cell_size;

    auto projection = glm::mat3(1);
    projection = glm::translate(projection, rstate.pan_pos + rstate.pan_offset);
    projection = glm::scale(projection, v2f(rstate.zoom, rstate.zoom));
    projection = glm::translate(projection, v2f(swidth, sheight) / 2.0f);
    projection = glm::translate(projection, -(v2f)gsize * cell_size / 2.0f);
    projection = glm::scale(projection, v2f(cell_size, cell_size));
    auto projection_inv = glm::inverse(projection);

    return projection * v3f(pos.x, pos.y, 1);
}

v2f Screen_To_World(Game_State& state, v2f pos) {
    assert(state.renderer_state != nullptr);
    auto& rstate = *state.renderer_state;
    assert(rstate.bitmap != nullptr);
    Game_Bitmap& bitmap = *rstate.bitmap;

    auto swidth = (f32)bitmap.width;
    auto sheight = (f32)bitmap.height;
    auto gsize = state.game_map.size;
    auto cell_size = rstate.cell_size;

    auto projection = glm::mat3(1);
    projection = glm::translate(projection, rstate.pan_pos + rstate.pan_offset);
    projection = glm::scale(projection, v2f(rstate.zoom, rstate.zoom));
    projection = glm::translate(projection, v2f(swidth, sheight) / 2.0f);
    projection = glm::translate(projection, -(v2f)gsize * (f32)cell_size / 2.0f);
    projection = glm::scale(projection, v2f(cell_size, cell_size));
    auto projection_inv = glm::inverse(projection);

    return projection_inv * v3f(pos.x, pos.y, 1);
}

v2i World_Pos_To_Tile(v2f pos) {
    auto x = (int)(pos.x);
    auto y = (int)(pos.y);
    // if (pos.x < 0.5f)
    //     x--;
    // if (pos.y < -0.5f)
    //     y--;
    return v2i(x, y);
}

void Draw_Stretchable_Sprite(
    f32 x0,
    f32 x3,
    f32 y0,
    f32 y3,
    Loaded_Texture& texture,
    UI_Sprite_Params& sprite_params,
    v2i panel_size,
    f32 in_scale  //
) {
    auto& pad_h = sprite_params.stretch_paddings_h;
    auto& pad_v = sprite_params.stretch_paddings_v;

    f32 x1 = (f32)pad_h.x / texture.size.x;
    f32 x2 = (f32)pad_h.y / texture.size.x;
    f32 y1 = (f32)pad_v.x / texture.size.y;
    f32 y2 = (f32)pad_v.y / texture.size.y;
    assert(y1 * in_scale + y2 * in_scale <= 1);
    assert(x1 * in_scale + x2 * in_scale <= 1);

    auto p0 = v3f(x0, y0, 1);
    auto p3 = v3f(x3, y3, 1);
    auto dx = x3 - x0;
    auto dy = y3 - y0;
    auto dp1 =
        v3f(pad_h.x * in_scale * dx / panel_size.x, pad_v.x * in_scale * dy / panel_size.y, 0);
    auto dp2 =
        v3f(pad_h.y * in_scale * dx / panel_size.x, pad_v.y * in_scale * dy / panel_size.y, 0);
    v3f p1 = p0 + dp1;
    v3f p2 = p3 - dp2;

    v3f points[] = {p0, p1, p2, p3};

    f32 texture_vertices_x[] = {0, x1, 1 - x2, 1};
    f32 texture_vertices_y[] = {0, y1, 1 - y2, 1};

    glBindTexture(GL_TEXTURE_2D, texture.id);
    glBegin(GL_TRIANGLES);

    FOR_RANGE(int, y, 3) {
        auto tex_y0 = texture_vertices_y[2 - y];
        auto tex_y1 = texture_vertices_y[3 - y];
        auto sprite_y0 = points[2 - y].y;
        auto sy = points[3 - y].y - points[2 - y].y;

        FOR_RANGE(int, x, 3) {
            auto tex_x0 = texture_vertices_x[x];
            auto tex_x1 = texture_vertices_x[x + 1];
            auto sprite_x0 = points[x].x;
            auto sx = points[x + 1].x - sprite_x0;
            assert(sx >= 0);

            Draw_UI_Sprite(
                tex_x0, tex_y0,  //
                tex_x1, tex_y1,  //
                v2f(sprite_x0, sprite_y0),  //
                v2f(sx, sy),  //
                v2f(0, 0)  //
            );
        }
    }
    glEnd();
}

void Render(Game_State& state, f32 dt) {
    assert(state.renderer_state != nullptr);
    auto& rstate = *state.renderer_state;
    auto& game_map = state.game_map;

    assert(rstate.bitmap != nullptr);
    Game_Bitmap& bitmap = *rstate.bitmap;

    auto gsize = game_map.size;
    auto swidth = (f32)bitmap.width;
    auto sheight = (f32)bitmap.height;
    auto cell_size = 32;

    {
        auto cursor_on_tilemap_pos = Screen_To_World(state, rstate.mouse_pos);
        ImGui::Text("Tilemap %.3f.%.3f", cursor_on_tilemap_pos.x, cursor_on_tilemap_pos.y);

        auto new_zoom = Move_Towards(rstate.zoom, rstate.zoom_target, 2 * dt * rstate.zoom);
        rstate.zoom = new_zoom;

        auto cursor_on_tilemap_pos2 = Screen_To_World(state, rstate.mouse_pos);

        auto cursor_d = cursor_on_tilemap_pos2 - cursor_on_tilemap_pos;

        auto projection = glm::mat3(1);
        projection = glm::translate(projection, v2f(0, 1));
        projection = glm::scale(projection, v2f(1 / swidth, -1 / sheight));
        projection = glm::translate(projection, rstate.pan_pos + rstate.pan_offset);
        projection = glm::scale(projection, v2f(rstate.zoom, rstate.zoom));
        projection = glm::translate(projection, v2f(swidth, sheight) / 2.0f);
        projection = glm::translate(projection, -(v2f)gsize * (f32)cell_size / 2.0f);
        auto d = projection * v3f(cursor_d.x, cursor_d.y, 0);
        rstate.pan_pos += cursor_d * (f32)(rstate.zoom * cell_size);

        auto d3 = World_To_Screen(state, v2f(0, 0));
        ImGui::Text("d3 %.3f.%.3f", d3.x, d3.y);

        auto d4 = World_Pos_To_Tile(cursor_on_tilemap_pos2);
        ImGui::Text("d4 %d.%d", d4.x, d4.y);
    }

    glClear(GL_COLOR_BUFFER_BIT);
    assert(!glGetError());

    GLuint texture_name = 3;
    glBindTexture(GL_TEXTURE_2D, texture_name);
    assert(!glGetError());

    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    assert(!glGetError());

    glTexImage2D(
        GL_TEXTURE_2D, 0, GL_RGBA8, bitmap.width, bitmap.height, 0, GL_BGRA_EXT, GL_UNSIGNED_BYTE,
        bitmap.memory);
    assert(!glGetError());

    glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);
    assert(!glGetError());

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
    projection = glm::translate(projection, v2f(0, 1));
    projection = glm::scale(projection, v2f(1 / swidth, -1 / sheight));
    projection = glm::translate(projection, rstate.pan_pos + rstate.pan_offset);
    projection = glm::scale(projection, v2f(rstate.zoom, rstate.zoom));
    projection = glm::translate(projection, v2f(swidth, sheight) / 2.0f);
    projection = glm::translate(projection, -(v2f)gsize * (f32)cell_size / 2.0f);
    // projection = glm::scale(projection, v2f(1, 1) * (f32)cell_size);
    // projection = glm::scale(projection, v2f(2, 2) / 2.0f);

    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    FOR_RANGE(i32, h, rstate.terrain_tilemaps_count) {
        auto& tilemap = *(rstate.tilemaps + h);

        FOR_RANGE(int, y, gsize.y) {
            FOR_RANGE(int, x, gsize.x) {
                auto& tile = Get_Terrain_Tile(game_map, {x, y});
                if (tile.terrain != Terrain::Grass)
                    continue;

                if (tile.height < h)
                    continue;

                // TODO(hulvdan): Spritesheets! Vertices array,
                // texture vertices array, indices array
                auto texture_id =
                    Test_Smart_Tile(tilemap, game_map.size, {x, y}, rstate.grass_smart_tile);

                glBindTexture(GL_TEXTURE_2D, texture_id);

                auto sprite_pos = v2i(x, y) * cell_size;
                auto sprite_size = v2i(1, 1) * cell_size;

                glBegin(GL_TRIANGLES);
                Draw_Sprite(0, 0, 1, 1, sprite_pos, sprite_size, 0, projection);
                glEnd();
            }
        }
    }

    // --- Drawing Resoures ---
    auto& resources_tilemap = *(rstate.tilemaps + rstate.resources_tilemap_index);
    FOR_RANGE(int, y, gsize.y) {
        FOR_RANGE(int, x, gsize.x) {
            auto& tile = *(resources_tilemap.tiles + y * gsize.x + x);
            if (tile == 0)
                continue;

            if (tile == rstate.forest_smart_tile.id) {
                auto texture_id = Test_Smart_Tile(
                    resources_tilemap, game_map.size, {x, y}, rstate.forest_smart_tile);

                glBindTexture(GL_TEXTURE_2D, texture_id);

                auto sprite_pos = v2i(x, y) * cell_size;
                auto sprite_size = v2i(1, 1) * cell_size;

                glBegin(GL_TRIANGLES);
                Draw_Sprite(0, 0, 1, 1, sprite_pos, sprite_size, 0, projection);
                glEnd();
            } else
                UNREACHABLE;
        }
    }

    auto& resources_tilemap2 = *(rstate.tilemaps + rstate.resources_tilemap_index + 1);
    FOR_RANGE(int, y, gsize.y) {
        FOR_RANGE(int, x, gsize.x) {
            auto& tile = *(resources_tilemap2.tiles + y * gsize.x + x);
            if (tile == 0)
                continue;

            if (tile == rstate.forest_top_tile_id) {
                glBindTexture(GL_TEXTURE_2D, rstate.forest_textures[0].id);

                auto sprite_pos = v2i(x, y + 1) * cell_size;
                auto sprite_size = v2i(1, 1) * cell_size;

                glBegin(GL_TRIANGLES);
                Draw_Sprite(0, 0, 1, 1, sprite_pos, sprite_size, 0, projection);
                glEnd();
            } else
                UNREACHABLE;
        }
    }
    // --- Drawing Resoures End ---

    // --- Drawing Element Tiles ---
    auto& element_tilemap = *(rstate.tilemaps + rstate.element_tilemap_index);
    auto& element_tilemap_2 = *(rstate.tilemaps + rstate.element_tilemap_index + 1);
    FOR_RANGE(int, y, gsize.y) {
        FOR_RANGE(int, x, gsize.x) {
            auto& tile = *(element_tilemap.tiles + y * gsize.x + x);
            if (tile < global_road_starting_tile_id)
                continue;

            auto road_texture_offset = tile - global_road_starting_tile_id;
            assert(road_texture_offset >= 0);

            auto tex_id = rstate.road_textures[road_texture_offset].id;
            glBindTexture(GL_TEXTURE_2D, tex_id);

            auto sprite_pos = v2i(x, y) * cell_size;
            auto sprite_size = v2i(1, 1) * cell_size;

            glBegin(GL_TRIANGLES);
            Draw_Sprite(0, 0, 1, 1, sprite_pos, sprite_size, 0, projection);
            glEnd();
        }
    }

    FOR_RANGE(int, y, gsize.y) {
        FOR_RANGE(int, x, gsize.x) {
            auto& tile = *(element_tilemap_2.tiles + y * gsize.x + x);
            if (tile < global_flag_starting_tile_id)
                continue;

            auto tex_id = rstate.flag_textures[tile - global_flag_starting_tile_id].id;
            glBindTexture(GL_TEXTURE_2D, tex_id);

            auto sprite_pos = v2i(x, y) * cell_size;
            auto sprite_size = v2i(1, 1) * cell_size;

            glBegin(GL_TRIANGLES);
            Draw_Sprite(0, 0, 1, 1, sprite_pos, sprite_size, 0, projection);
            glEnd();
        }
    }
    // --- Drawing Element Tiles End ---

    FOR_RANGE(size_t, page_index, game_map.building_pages_used) {
        auto& page = *(game_map.building_pages + page_index);
        auto count = Get_Building_Page_Meta(state.os_data->page_size, page).count;

        FOR_RANGE(size_t, index, count) {
            Building& building = *(rcast<Building*>(page.base) + index);
            if (!building.active)
                continue;

            auto scriptable_building_ = Get_Scriptable_Building(state, building.scriptable_id);
            assert(scriptable_building_ != nullptr);
            auto& scriptable_building = *scriptable_building_;

            auto tex_id = scriptable_building.texture->id;
            glBindTexture(GL_TEXTURE_2D, tex_id);

            auto sprite_pos = building.pos * cell_size;
            auto sprite_size = v2i(1, 1) * cell_size;

            glBegin(GL_TRIANGLES);
            Draw_Sprite(0, 0, 1, 1, sprite_pos, sprite_size, 0, projection);
            glEnd();
        }
    }

    glDeleteTextures(1, (GLuint*)&texture_name);
    assert(!glGetError());

    {
        // Drawing left buildables thingy
        auto& ui_state = *rstate.ui_state;
        ui_state.buildables_panel_params.stretch_paddings_h = {6, 6};

        auto sprite_params = ui_state.buildables_panel_params;
        auto& pad_h = sprite_params.stretch_paddings_h;
        auto& pad_v = sprite_params.stretch_paddings_v;

        auto texture = ui_state.buildables_panel_background;
        auto placeholder_texture = ui_state.buildables_placeholder_background;
        auto& psize = placeholder_texture.size;

        auto scale = 3.0f;
        auto in_scale = 1.0f;
        v2f sprite_anchor = {0.0f, 0.5f};

        v2f padding = {6, 6};
        f32 placeholders_gap = 4;
        auto placeholders = 2;
        auto panel_size =
            v2f(psize.x + 2 * padding.x,
                2 * padding.y + placeholders_gap * (placeholders - 1) + placeholders * psize.y);

        auto outer_anchor = v2f(0.0f, 0.5f);
        auto outer_container_size = v2i(swidth, sheight);
        auto outer_x = outer_container_size.x * outer_anchor.x;
        auto outer_y = outer_container_size.y * outer_anchor.y;

        auto projection = glm::mat3(1);
        projection = glm::translate(projection, v2f(0, 1));
        projection = glm::scale(projection, v2f(1 / swidth, -1 / sheight));
        projection = glm::translate(projection, v2f((int)outer_x, (int)outer_y));
        projection = glm::scale(projection, v2f(scale));

        {
            auto model = glm::mat3(1);
            model = glm::scale(model, v2f(panel_size));

            auto p0_local = model * v3f(v2f_zero - sprite_anchor, 1);
            auto p1_local = model * v3f(v2f_one - sprite_anchor, 1);
            auto p0 = projection * p0_local;
            auto p1 = projection * p1_local;
            Draw_Stretchable_Sprite(
                p0.x, p1.x, p0.y, p1.y, texture, sprite_params, panel_size, in_scale);

            auto origin = (p1_local + p0_local) / 2.0f;

            glBindTexture(GL_TEXTURE_2D, ui_state.buildables_placeholder_background.id);
            glBegin(GL_TRIANGLES);
            // Aligning items in a column
            // justify-content: center
            FOR_RANGE(int, i, placeholders) {
                auto drawing_point = origin;
                drawing_point.y -= (placeholders - 1) * (psize.y + placeholders_gap) / 2;
                drawing_point.y += i * (placeholders_gap + psize.y);

                auto p = projection * drawing_point;
                auto s = projection * v3f(psize, 0);
                Draw_UI_Sprite(0, 0, 1, 1, p, s, v2f_one / 2.0f);
            }
            glEnd();

            GLuint buildable_textures[] = {
                (state.scriptable_buildings + 1)->texture->id,
                (rstate.road_textures + 15)->id,
            };
            auto buildable_size = v2f(psize) * (2.0f / 3.0f);
            FOR_RANGE(int, i, placeholders) {
                auto drawing_point = origin;
                drawing_point.y -= (placeholders - 1) * (psize.y + placeholders_gap) / 2;
                drawing_point.y += i * (placeholders_gap + psize.y);

                auto p = projection * drawing_point;
                auto s = projection * v3f(buildable_size, 0);
                glBindTexture(GL_TEXTURE_2D, buildable_textures[i]);
                glBegin(GL_TRIANGLES);
                Draw_UI_Sprite(0, 0, 1, 1, p, s, v2f_one / 2.0f);
                glEnd();
            }
        }
    }
}

// NOTE(hulvdan): Game_State& state, v2i pos, Item_To_Build item
On_Item_Built__Function(Renderer__On_Item_Built) {
    assert(state.renderer_state != nullptr);
    auto& rstate = *state.renderer_state;
    auto& game_map = state.game_map;
    auto gsize = game_map.size;

    auto& element_tilemap = *(rstate.tilemaps + rstate.element_tilemap_index);
    auto& element_tilemap_2 = *(rstate.tilemaps + rstate.element_tilemap_index + 1);
    auto tile_index = pos.y * gsize.x + pos.x;

    auto& element_tile = *(game_map.element_tiles + tile_index);

    switch (element_tile.type) {
    case Element_Tile_Type::Road: {
        *(element_tilemap_2.tiles + pos.y * gsize.x + pos.x) = 0;
    } break;

    case Element_Tile_Type::Flag: {
        *(element_tilemap_2.tiles + pos.y * gsize.x + pos.x) = global_flag_starting_tile_id;
    } break;

    default:
        UNREACHABLE;
    }
    assert(element_tile.building == nullptr);

    v2i offsets[] = {{0, 0}, {1, 0}, {0, 1}, {-1, 0}, {0, -1}};
    for (auto offset : offsets) {
        auto new_pos = pos + offset;
        if (!Pos_Is_In_Bounds(new_pos, gsize))
            continue;

        auto element_tiles = game_map.element_tiles;
        Element_Tile& element_tile = *(element_tiles + new_pos.y * gsize.x + new_pos.x);

        switch (element_tile.type) {
        case Element_Tile_Type::Flag: {
            auto tex = Get_Road_Texture_Number(game_map.element_tiles, new_pos, gsize);
            auto& tile_id = *(element_tilemap.tiles + new_pos.y * gsize.x + new_pos.x);
            tile_id = global_road_starting_tile_id + tex;
        } break;

        case Element_Tile_Type::Road: {
            auto tex = Get_Road_Texture_Number(game_map.element_tiles, new_pos, gsize);
            auto& tile_id = *(element_tilemap.tiles + new_pos.y * gsize.x + new_pos.x);
            tile_id = global_road_starting_tile_id + tex;
        } break;

        case Element_Tile_Type::None:
            break;

        default:
            UNREACHABLE;
        }
    }
}
