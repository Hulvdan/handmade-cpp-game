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

struct Load_BMP_RGBA_Result {
    bool success;
    u16 width;
    u16 height;
};

Load_BMP_RGBA_Result Load_BMP_RGBA(u8* output, const u8* filedata, size_t output_max_bytes)
{
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
    if (total_bytes > output_max_bytes) {
        // TODO(hulvdan): Diagnostic
        assert(false);
        return res;
    }

    res.width = header.width;
    res.height = header.height;

    assert(header.planes == 1);
    assert(header.bits_per_pixel == 32);
    assert(header.file_size - header.data_offset == total_bytes);

    memcpy(output, filedata + header.data_offset, total_bytes);

    res.success = true;
    return res;
}

Game_Renderer_State*
Initialize_Renderer(Game_State* game_state, Arena& arena, Arena& file_loading_arena)
{
    auto state_ = Allocate_For(arena, Game_Renderer_State);
    auto& state = *state_;

    auto load_result = Debug_Load_File_To_Arena("assets/art/human.bmp", file_loading_arena);
    assert(load_result.success);

    auto filedata = file_loading_arena.base + file_loading_arena.used;
    auto loading_address = arena.base + arena.used;
    auto bmp_result = Load_BMP_RGBA(loading_address, filedata, arena.size - arena.used);
    if (bmp_result.success) {
        state.human_texture.size = {bmp_result.width, bmp_result.height};
        state.human_texture.address = loading_address;
        Allocate_Array(arena, u8, (size_t)bmp_result.width * bmp_result.height * 4);
    }

    glEnable(GL_TEXTURE_2D);

    char buffer[512] = {};
    FOR_RANGE(int, i, 17)
    {
        sprintf(buffer, "assets/art/tiles/grass_%d.bmp", i + 1);

        auto load_result = Debug_Load_File_To_Arena(buffer, file_loading_arena);

        assert(load_result.success);

        auto filedata = file_loading_arena.base + file_loading_arena.used;
        auto loading_address = arena.base + arena.used;
        auto bmp_result = Load_BMP_RGBA(loading_address, filedata, arena.size - arena.used);

        if (bmp_result.success) {
            Loaded_Texture& texture = state.grass_textures[i];

            constexpr size_t NAME_SIZE = 256;
            char name[NAME_SIZE] = {};
            sprintf(name, "grass_%d", i + 1);
            auto name_size = Find_Newline_Or_EOF((u8*)name, NAME_SIZE);
            assert(name_size > 0);
            assert(name_size < NAME_SIZE);

            // texture.id = state.texture_ids[i] + 10;
            texture.id = scast<BF_Texture_ID>(Hash32((u8*)name, name_size));
            texture.size = {bmp_result.width, bmp_result.height};
            texture.address = loading_address;
            Allocate_Array(arena, u8, (size_t)bmp_result.width * bmp_result.height * 4);

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
    }

    auto rule_file_result =
        Debug_Load_File_To_Arena("assets/art/tiles/tilerule_grass.txt", file_loading_arena);
    assert(rule_file_result.success);

    state.grass_smart_tile.id = (Tile_ID)Terrain::GRASS;
    auto rule_loading_result = Load_Smart_Tile_Rules(
        state.grass_smart_tile,  //
        arena.base + arena.used,
        arena.size - arena.used,  //
        file_loading_arena.base + file_loading_arena.used,  //
        rule_file_result.size);
    assert(rule_loading_result.success);
    Allocate_Array(arena, u8, rule_loading_result.size);

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
    const glm::mat3& projection)
{
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
    for (ptrdiff_t i : {0, 1, 2, 2, 1, 3}) {
        // TODO(hulvdan): How bad is that there are 2 vertices duplicated?
        glTexCoord2f(texture_vertices[2 * i], texture_vertices[2 * i + 1]);
        glVertex2f(vertices[i].x, vertices[i].y);
    }
};

void Render(Game_State& state, Game_Renderer_State& renderer_state, Game_Bitmap& bitmap)
{
    glClear(GL_COLOR_BUFFER_BIT);
    assert(!glGetError());

    GLuint texture_name = 1;
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

    GLuint human_texture_name = 2;
    auto& rstate = *state.renderer_state;
    auto& debug_texture = rstate.grass_textures[14];
    if (debug_texture.address) {
        glBindTexture(GL_TEXTURE_2D, human_texture_name);
        assert(!glGetError());

        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
        assert(!glGetError());

        glTexImage2D(
            GL_TEXTURE_2D, 0, GL_RGBA8, debug_texture.size.x, debug_texture.size.y, 0, GL_BGRA_EXT,
            GL_UNSIGNED_BYTE, debug_texture.address);
    }

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

    auto gsize = state.gamemap.size;
    FOR_RANGE(int, y, gsize.y)
    {
        FOR_RANGE(int, x, gsize.x)
        {
            auto& tile = Get_Terrain_Tile(state.gamemap, {x, y});
            if (tile.terrain != Terrain::GRASS)
                continue;

            auto texture_id = 2;
            // TODO(hulvdan): Make a proper game renderer
            // auto texture_id = TestSmart_Tile(
            //     state.gamemap.terrain_tiles, sizeof(Tile_ID), state.gamemap.size, {x, y},
            //     state.grass_smart_tile);

            glBindTexture(GL_TEXTURE_2D, texture_id);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glBegin(GL_TRIANGLES);

            auto cell_size = 32;
            auto sprite_pos = v2i(x + 2, y + 2) * cell_size;
            auto sprite_size = v2i(1, 1) * cell_size;

            auto swidth = (f32)bitmap.width;
            auto sheight = (f32)bitmap.height;

            auto projection = glm::mat3(1);
            projection = glm::translate(projection, glm::vec2(0, 1));
            projection = glm::scale(projection, glm::vec2(1 / swidth, -1 / sheight));
            projection = glm::rotate(projection, -0.2f);

            Draw_Sprite(0, 0, 1, 1, sprite_pos, sprite_size, 0, projection);

            glEnd();
        }
    }

    glDeleteTextures(1, (GLuint*)&texture_name);
    if (rstate.human_texture.address)
        glDeleteTextures(1, (GLuint*)&human_texture_name);
    assert(!glGetError());
}
