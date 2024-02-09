//
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
        GL_UNSIGNED_BYTE, texture.address);
    assert(!glGetError());
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

Game_Renderer_State* Initialize_Renderer(Game_Map& game_map, Arena& arena, Arena& temp_arena) {
    auto state_ = Allocate_Zeros_For(arena, Game_Renderer_State);
    auto& state = *state_;

    Load_BMP_RGBA_Result bmp_result = {};
    {
        auto load_result = Debug_Load_File_To_Arena("assets/art/human.bmp", temp_arena);
        assert(load_result.success);
        DEFER(Deallocate_Array(temp_arena, u8, load_result.size));

        bmp_result = Load_BMP_RGBA(arena, load_result.output);
        assert(bmp_result.success);
    }

    state.human_texture.size = {bmp_result.width, bmp_result.height};
    state.human_texture.address = bmp_result.output;

    glEnable(GL_TEXTURE_2D);

    auto gsize = game_map.size;

    char buffer[512] = {};
    FOR_RANGE(int, i, 17) {
        sprintf(buffer, "assets/art/tiles/grass_%d.bmp", i);

        Load_BMP_RGBA_Result bmp_result = {};
        {
            auto load_result = Debug_Load_File_To_Arena(buffer, temp_arena);
            assert(load_result.success);
            DEFER(Deallocate_Array(temp_arena, u8, load_result.size));

            bmp_result = Load_BMP_RGBA(arena, load_result.output);
            assert(bmp_result.success);
        }

        Loaded_Texture& texture = state.grass_textures[i];

        constexpr size_t NAME_SIZE = 64;
        char name[NAME_SIZE] = {};
        sprintf(name, "grass_%d", i);

        texture.id = scast<BF_Texture_ID>(Hash32_String(name));
        texture.size = {bmp_result.width, bmp_result.height};
        texture.address = bmp_result.output;
        Send_Texture_To_GPU(texture);
    }

    FOR_RANGE(int, i, 3) {
        sprintf(buffer, "assets/art/tiles/forest_%d.bmp", i);

        Load_BMP_RGBA_Result bmp_result = {};
        {
            auto load_result = Debug_Load_File_To_Arena(buffer, temp_arena);
            DEFER(Deallocate_Array(temp_arena, u8, load_result.size));
            assert(load_result.success);

            bmp_result = Load_BMP_RGBA(arena, load_result.output);
            assert(bmp_result.success);
        }

        Loaded_Texture& texture = state.forest_textures[i];

        constexpr size_t NAME_SIZE = 64;
        char name[NAME_SIZE] = {};
        sprintf(name, "forest_%d", i);

        texture.id = scast<BF_Texture_ID>(Hash32_String(name));
        texture.size = {bmp_result.width, bmp_result.height};
        texture.address = bmp_result.output;
        Send_Texture_To_GPU(texture);
    }

    FOR_RANGE(int, i, 16) {
        sprintf(buffer, "assets/art/tiles/road_%d.bmp", i);

        Load_BMP_RGBA_Result bmp_result = {};
        {
            auto load_result = Debug_Load_File_To_Arena(buffer, temp_arena);
            DEFER(Deallocate_Array(temp_arena, u8, load_result.size));
            assert(load_result.success);

            bmp_result = Load_BMP_RGBA(arena, load_result.output);
            assert(bmp_result.success);
        }

        Loaded_Texture& texture = state.road_textures[i];

        constexpr size_t NAME_SIZE = 64;
        char name[NAME_SIZE] = {};
        sprintf(name, "road_%d", i);

        texture.id = scast<BF_Texture_ID>(Hash32_String(name));
        texture.size = {bmp_result.width, bmp_result.height};
        texture.address = bmp_result.output;
        Send_Texture_To_GPU(texture);
    }

    FOR_RANGE(int, i, 4) {
        sprintf(buffer, "assets/art/tiles/flag_%d.bmp", i);

        Load_BMP_RGBA_Result bmp_result = {};
        {
            auto load_result = Debug_Load_File_To_Arena(buffer, temp_arena);
            DEFER(Deallocate_Array(temp_arena, u8, load_result.size));
            assert(load_result.success);

            bmp_result = Load_BMP_RGBA(arena, load_result.output);
            assert(bmp_result.success);
        }

        Loaded_Texture& texture = state.flag_textures[i];

        constexpr size_t NAME_SIZE = 64;
        char name[NAME_SIZE] = {};
        sprintf(name, "flag_%d", i);

        texture.id = scast<BF_Texture_ID>(Hash32_String(name));
        texture.size = {bmp_result.width, bmp_result.height};
        texture.address = bmp_result.output;
        Send_Texture_To_GPU(texture);
    }

    state.grass_smart_tile.id = 1;
    state.forest_smart_tile.id = 2;
    state.forest_top_tile_id = 3;

    {
        auto path = "assets/art/tiles/tilerule_grass.txt";
        auto load_result = Debug_Load_File_To_Arena(path, temp_arena);
        assert(load_result.success);
        DEFER(Deallocate_Array(temp_arena, u8, load_result.size));

        auto rule_loading_result = Load_Smart_Tile_Rules(
            state.grass_smart_tile, arena, load_result.output, load_result.size);
        assert(rule_loading_result.success);
    }

    {
        auto path = "assets/art/tiles/tilerule_forest.txt";
        auto load_result = Debug_Load_File_To_Arena(path, temp_arena);
        assert(load_result.success);
        DEFER(Deallocate_Array(temp_arena, u8, load_result.size));

        auto rule_loading_result = Load_Smart_Tile_Rules(
            state.forest_smart_tile, arena, load_result.output, load_result.size);
        assert(rule_loading_result.success);
    }

    i32 max_height = 0;
    FOR_RANGE(i32, y, gsize.y) {
        FOR_RANGE(i32, x, gsize.x) {
            auto& terrain_tile = *(game_map.terrain_tiles + y * gsize.x + x);
            max_height = MAX(max_height, terrain_tile.height);
        }
    }

    state.tilemaps_count = 0;
    // NOTE(hulvdan): Terrain tilemaps ([0; max_height])
    state.terrain_tilemaps_count = max_height + 1;
    state.tilemaps_count += state.terrain_tilemaps_count;

    // NOTE(hulvdan): Terrain Resources (forests, stones, etc.)
    // Currently use 2 tilemaps:
    // 1) Forests
    // 2) Tree's top tiles
    state.resources_tilemap_index = state.tilemaps_count;
    state.tilemaps_count += 2;

    // NOTE(hulvdan): Element Tiles (roads, buildings, etc.)
    // Currently use 2 tilemaps:
    // 1) Roads
    // 2) Flags above roads
    state.element_tilemap_index = state.tilemaps_count;
    state.tilemaps_count += 2;

    state.tilemaps = Allocate_Array(arena, Tilemap, state.tilemaps_count);

    FOR_RANGE(i32, h, state.tilemaps_count) {
        auto& tilemap = *(state.tilemaps + h);
        tilemap.tiles = Allocate_Array(arena, Tile_ID, gsize.x * gsize.y);

        FOR_RANGE(i32, y, gsize.y) {
            FOR_RANGE(i32, x, gsize.x) {
                auto& tile = *(game_map.terrain_tiles + y * gsize.x + x);
                auto& tilemap_tile = *(tilemap.tiles + y * gsize.x + x);

                bool grass = tile.terrain == Terrain::Grass && tile.height >= h;
                tilemap_tile = grass * state.grass_smart_tile.id;
            }
        }
    }

    auto& resources_tilemap = *(state.tilemaps + state.resources_tilemap_index);
    auto& resources_tilemap2 = *(state.tilemaps + state.resources_tilemap_index + 1);
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

            tile = state.forest_smart_tile.id;

            bool forest_above = false;
            if (!is_last_row) {
                auto& tile_above = *(resources_tilemap.tiles + (y + 1) * gsize.x + x);
                forest_above = tile_above == state.forest_smart_tile.id;
            }

            if (!forest_above)
                *(resources_tilemap2.tiles + y * gsize.x + x) = state.forest_top_tile_id;
        }
    }

    // --- Element Tiles ---
    auto& element_tilemap = *(state.tilemaps + state.element_tilemap_index);
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

    state.zoom = 1;
    state.zoom_target = 1;
    state.cell_size = 32;

    return state_;
}

void Draw_Sprite(
    u16 x0,
    u16 y0,
    u16 x1,
    u16 y1,
    glm::vec2 pos,
    glm::vec2 size,
    float rotation,
    const glm::mat3& projection  //
) {
    assert(x0 < x1);
    assert(y0 < y1);

    auto model = glm::mat3(1);
    model = glm::translate(model, pos);
    model = glm::rotate(model, rotation);
    model = glm::scale(model, size / 2.0f);

    auto matrix = projection * model;
    // TODO(hulvdan): How bad is it that there are vec3, but not vec2?
    glm::vec3 vertices[] = {{-1, -1, 1}, {-1, 1, 1}, {1, -1, 1}, {1, 1, 1}};
    for (auto& vertex : vertices) {
        vertex = matrix * vertex;
        // Converting from homogeneous to eucledian
        // vertex.x = vertex.x / vertex.z;
        // vertex.y = vertex.y / vertex.z;
    }

    u16 texture_vertices[] = {x0, y0, x0, y1, x1, y0, x1, y1};
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
    projection = glm::scale(projection, glm::vec2(rstate.zoom, rstate.zoom));
    projection = glm::translate(projection, glm::vec2(swidth, sheight) / 2.0f);
    projection = glm::translate(projection, -(v2f)gsize * cell_size / 2.0f);
    projection = glm::scale(projection, glm::vec2(cell_size, cell_size));
    auto projection_inv = glm::inverse(projection);

    return projection * glm::vec3(pos.x, pos.y, 1);
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
    projection = glm::scale(projection, glm::vec2(rstate.zoom, rstate.zoom));
    projection = glm::translate(projection, glm::vec2(swidth, sheight) / 2.0f);
    projection = glm::translate(projection, -(v2f)gsize * (f32)cell_size / 2.0f);
    projection = glm::scale(projection, glm::vec2(cell_size, cell_size));
    auto projection_inv = glm::inverse(projection);

    return projection_inv * glm::vec3(pos.x, pos.y, 1);
}

v2i World_Pos_To_Tile(v2f pos) {
    auto x = (int)(pos.x + 0.5f);
    auto y = (int)(pos.y + 0.5f);
    if (pos.x < -0.5f)
        x--;
    if (pos.y < -0.5f)
        y--;
    return v2i(x, y);
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
        projection = glm::translate(projection, glm::vec2(0, 1));
        projection = glm::scale(projection, glm::vec2(1 / swidth, -1 / sheight));
        projection = glm::translate(projection, rstate.pan_pos + rstate.pan_offset);
        projection = glm::scale(projection, glm::vec2(rstate.zoom, rstate.zoom));
        projection = glm::translate(projection, glm::vec2(swidth, sheight) / 2.0f);
        projection = glm::translate(projection, -(v2f)gsize * (f32)cell_size / 2.0f);
        auto d = projection * glm::vec3(cursor_d.x, cursor_d.y, 0);
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

        glm::vec2 vertices[] = {{0, 0}, {0, 1}, {1, 0}, {1, 1}};
        for (auto i : {0, 1, 2, 2, 1, 3}) {
            auto& v = vertices[i];
            glTexCoord2f(v.x, v.y);
            glVertex2f(v.x, v.y);
        }

        glEnd();
    }

    auto projection = glm::mat3(1);
    projection = glm::translate(projection, glm::vec2(0, 1));
    projection = glm::scale(projection, glm::vec2(1 / swidth, -1 / sheight));
    projection = glm::translate(projection, rstate.pan_pos + rstate.pan_offset);
    projection = glm::scale(projection, glm::vec2(rstate.zoom, rstate.zoom));
    projection = glm::translate(projection, glm::vec2(swidth, sheight) / 2.0f);
    projection = glm::translate(projection, -(v2f)gsize * (f32)cell_size / 2.0f);

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
                assert(false);
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
                assert(false);
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

            glBindTexture(GL_TEXTURE_2D, rstate.road_textures[road_texture_offset].id);

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

            glBindTexture(
                GL_TEXTURE_2D, rstate.flag_textures[tile - global_flag_starting_tile_id].id);

            auto sprite_pos = v2i(x, y) * cell_size;
            auto sprite_size = v2i(1, 1) * cell_size;

            glBegin(GL_TRIANGLES);
            Draw_Sprite(0, 0, 1, 1, sprite_pos, sprite_size, 0, projection);
            glEnd();
        }
    }
    // --- Drawing Element Tiles End ---

    glDeleteTextures(1, (GLuint*)&texture_name);
    assert(!glGetError());
}

On_Item_Built__Function(Renderer__On_Item_Built) {
    // Game_State& state, v2i pos, Item_To_Build item
    assert(state.renderer_state != nullptr);
    auto& rstate = *state.renderer_state;
    auto& game_map = state.game_map;
    auto gsize = game_map.size;

    auto& element_tilemap = *(rstate.tilemaps + rstate.element_tilemap_index);
    auto& element_tilemap_2 = *(rstate.tilemaps + rstate.element_tilemap_index + 1);
    auto tile_index = pos.y * gsize.x + pos.x;

    auto& element_tile = *(game_map.element_tiles + tile_index);

    switch (item) {
    case (Item_To_Build::Road): {
        element_tile.type = Element_Tile_Type::Road;
    } break;

    case (Item_To_Build::Flag): {
        element_tile.type = Element_Tile_Type::Flag;
        // if (element_tile.type == Element_Tile_Type::Flag) {
        //     element_tile.type = Element_Tile_Type::Road;
        //     *(element_tilemap_2.tiles + pos.y * gsize.x + pos.x) = 0;
        //
        // } else if (element_tile.type == Element_Tile_Type::Road) {
        //     element_tile.type = Element_Tile_Type::Flag;
        //     *(element_tilemap_2.tiles + pos.y * gsize.x + pos.x) = global_flag_starting_tile_id;
        //
        // } else
        //     assert(false);
    } break;

    default:
        assert(false);
    }
    assert(element_tile.building == nullptr);

    if (element_tile.type == Element_Tile_Type::Flag) {
        *(element_tilemap_2.tiles + pos.y * gsize.x + pos.x) = global_flag_starting_tile_id;
    }

    v2i offsets[] = {{0, 0}, {1, 0}, {0, 1}, {-1, 0}, {0, -1}};
    for (auto offset : offsets) {
        auto new_pos = pos + offset;
        if (!Pos_Is_In_Bounds(new_pos, gsize))
            continue;

        auto element_tiles = game_map.element_tiles;
        Element_Tile& element_tile = *(element_tiles + new_pos.y * gsize.x + new_pos.x);

        switch (element_tile.type) {
        case (Element_Tile_Type::Flag): {
            auto tex = Get_Road_Texture_Number(game_map.element_tiles, new_pos, gsize);
            auto& tile_id = *(element_tilemap.tiles + new_pos.y * gsize.x + new_pos.x);
            tile_id = global_road_starting_tile_id + tex;
        } break;

        case (Element_Tile_Type::Road): {
            auto tex = Get_Road_Texture_Number(game_map.element_tiles, new_pos, gsize);
            auto& tile_id = *(element_tilemap.tiles + new_pos.y * gsize.x + new_pos.x);
            tile_id = global_road_starting_tile_id + tex;
        } break;

        case (Element_Tile_Type::None):
            break;

        default:
            assert(false);
        }
    }
}
