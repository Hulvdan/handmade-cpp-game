#include <cassert>
#include <cstdlib>
#include <vector>

#include "glew.h"
#include "wglew.h"

#include "glm/gtx/matrix_transform_2d.hpp"
#include "glm/mat3x3.hpp"
#include "glm/vec2.hpp"

#include "bf_base.h"
#include "bf_game.h"

// NOLINTBEGIN(bugprone-suspicious-include)
#include "bf_strings.cpp"
#include "bf_renderer.cpp"
#include "bf_hash.cpp"
#include "bf_tilemap.cpp"
// NOLINTEND(bugprone-suspicious-include)

struct LoadedTexture {
    BFTextureID id;
    glm::ivec2 size;
    u8* address;
};

enum class Terrain {
    NONE,
    GRASS,
};

struct GameMap {
    v2i size;
    TileID* terrain_tiles;
};

struct Arena {
    size_t used;
    size_t size;
    u8* base;
};

#define AllocateFor(arena, type) (type*)AllocateFor_(arena, sizeof(type))
#define AllocateArray(arena, type, count) (type*)AllocateFor_(arena, sizeof(type) * (count))

u8* AllocateFor_(Arena& arena, size_t size)
{
    assert(size > 0);
    assert(size <= arena.size);
    assert(arena.used <= arena.size - size);

    u8* result = arena.base + arena.used;
    arena.used += size;
    return result;
}

struct GameState {
    f32 offset_x;
    f32 offset_y;

    v2f player_pos;
    GameMap game_map;

    Arena memory_arena;
    Arena file_loading_arena;

    SmartTile grass_smart_tile;
    LoadedTexture human_texture;
    LoadedTexture grass_textures[17];
};

struct GameMemory {
    bool is_initialized;
    GameState state;
};

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

struct Debug_LoadFileResult {
    bool success;
    u32 size;
};

Debug_LoadFileResult Debug_LoadFile(const char* filename, u8* output, size_t output_max_bytes)
{
    FILE* file = 0;
    auto failed = fopen_s(&file, filename, "r");
    assert(!failed);

    auto read_bytes = fread((void*)output, 1, output_max_bytes, file);

    Debug_LoadFileResult result = {};
    if (feof(file)) {
        result.success = true;
        result.size = read_bytes;
    }

    assert(fclose(file) == 0);
    return result;
}

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

struct LoadBMP_RGBA_Result {
    bool success;
    u16 width;
    u16 height;
};

LoadBMP_RGBA_Result LoadBMP_RGBA(u8* output, const u8* filedata, size_t output_max_bytes)
{
    LoadBMP_RGBA_Result res = {};

    auto& header = *(Debug_BMP_Header*)filedata;

    if (header.signature != *(u16*)("BM")) {
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

        state.game_map = {};
        state.game_map.size = {32, 24};
        auto& dim = state.game_map.size;

        state.game_map.terrain_tiles = AllocateArray(arena, TileID, (size_t)dim.x * dim.y);

        auto* terrain_tile = state.game_map.terrain_tiles;
        for (int y = 0; y < state.game_map.size.y; y++) {
            for (int x = 0; x < state.game_map.size.x; x++) {
                auto val = rand() % 4 ? Terrain::GRASS : Terrain::NONE;
                *terrain_tile++ = (TileID)val;
            }
        }

        terrain_tile = state.game_map.terrain_tiles;
        auto stride = state.game_map.size.x;
        for (int y = 0; y < state.game_map.size.y; y++) {
            for (int x = 0; x < state.game_map.size.x; x++) {
                auto is_grass = (Terrain)(*terrain_tile) == Terrain::GRASS;
                auto above_is_grass = false;
                auto below_is_grass = false;
                if (y < state.game_map.size.y - 1)
                    above_is_grass = (Terrain)(*(terrain_tile + stride)) == Terrain::GRASS;
                if (y > 0)
                    below_is_grass = (Terrain)(*(terrain_tile - stride)) == Terrain::GRASS;

                if (is_grass && !above_is_grass && !below_is_grass)
                    *terrain_tile = (TileID)Terrain::NONE;

                terrain_tile++;
            }
        }

        auto load_result = Debug_LoadFile(
            R"PATH(c:\Users\user\dev\home\handmade-cpp-game\assets\art\human.bmp)PATH",
            file_loading_arena.base + file_loading_arena.used,
            file_loading_arena.size - file_loading_arena.used);

        if (load_result.success) {
            auto filedata = file_loading_arena.base + file_loading_arena.used;
            auto loading_address = arena.base + arena.used;
            auto bmp_result = LoadBMP_RGBA(loading_address, filedata, arena.size - arena.used);
            if (bmp_result.success) {
                state.human_texture.size = v2i(bmp_result.width, bmp_result.height);
                state.human_texture.address = loading_address;
                AllocateArray(arena, u8, (size_t)bmp_result.width * bmp_result.height * 4);
            }
        }

        glEnable(GL_TEXTURE_2D);

        char buffer[512] = {};
        for (int i = 0; i < 17; i++) {
            const auto pattern =
                R"PATH(c:\Users\user\dev\home\handmade-cpp-game\assets\art\tiles\grass_%d.bmp)PATH";
            sprintf(buffer, pattern, i + 1);

            auto load_result = Debug_LoadFile(
                buffer, file_loading_arena.base + file_loading_arena.used,
                file_loading_arena.size - file_loading_arena.used);

            if (load_result.success) {
                auto filedata = file_loading_arena.base + file_loading_arena.used;
                auto loading_address = arena.base + arena.used;
                auto bmp_result = LoadBMP_RGBA(loading_address, filedata, arena.size - arena.used);

                if (bmp_result.success) {
                    LoadedTexture& texture = state.grass_textures[i];

                    constexpr size_t NAME_SIZE = 256;
                    char name[NAME_SIZE] = {};
                    sprintf(name, "grass_%d", i + 1);
                    auto name_size = FindNewlineOrEOF((u8*)name, 12);
                    assert(name_size > 0);
                    assert(name_size < NAME_SIZE);

                    // texture.id = state.texture_ids[i] + 10;
                    texture.id = static_cast<BFTextureID>(Hash32((u8*)name, name_size));
                    texture.size = v2i(bmp_result.width, bmp_result.height);
                    texture.address = loading_address;
                    AllocateArray(arena, u8, (size_t)bmp_result.width * bmp_result.height * 4);

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
        }

        auto rule_file_result = Debug_LoadFile(
            R"PATH(c:\Users\user\dev\home\handmade-cpp-game\assets\art\tiles\tilerule_grass.txt)PATH",
            file_loading_arena.base + file_loading_arena.used,
            file_loading_arena.size - file_loading_arena.used);
        assert(rule_file_result.success);

        state.grass_smart_tile.id = (TileID)Terrain::GRASS;
        auto rule_loading_result = LoadSmartTileRules(
            state.grass_smart_tile,  //
            state.memory_arena.base + state.memory_arena.used,
            state.memory_arena.size - state.memory_arena.used,  //
            state.file_loading_arena.base + state.file_loading_arena.used,  //
            rule_file_result.size);

        assert(rule_loading_result.success);

        AllocateArray(state.memory_arena, u8, rule_loading_result.size);
        memory.is_initialized = true;
    }

    ProcessEvents(memory, (u8*)input_events_bytes_ptr, input_events_count);

    assert(bitmap.bits_per_pixel == 32);
    auto pixel = (u32*)bitmap.memory;

    auto offset_x = (i32)state.offset_x;
    auto offset_y = (i32)state.offset_y;

    state.offset_y += dt * 256.0f;
    state.offset_x += dt * 256.0f;

    const auto player_radius = 24;

    for (i32 y = 0; y < bitmap.height; y++) {
        for (i32 x = 0; x < bitmap.width; x++) {
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

            (*pixel++) = (blue << 0) | (green << 8) | (red << 16);
        }
    }

    {
        // --- RENDERING START
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
            GL_TEXTURE_2D, 0, GL_RGBA8, bitmap.width, bitmap.height, 0, GL_BGRA_EXT,
            GL_UNSIGNED_BYTE, bitmap.memory);
        assert(!glGetError());

        GLuint human_texture_name = 2;
        auto& debug_texture = state.grass_textures[14];
        if (debug_texture.address) {
            glBindTexture(GL_TEXTURE_2D, human_texture_name);
            assert(!glGetError());

            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
            assert(!glGetError());

            glTexImage2D(
                GL_TEXTURE_2D, 0, GL_RGBA8, debug_texture.size.x, debug_texture.size.y, 0,
                GL_BGRA_EXT, GL_UNSIGNED_BYTE, debug_texture.address);
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

        if (debug_texture.address) {
            glBindTexture(GL_TEXTURE_2D, human_texture_name);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
            glBegin(GL_TRIANGLES);

            auto angle = BF_PI / 8;

            auto sprite_pos = glm::vec2(200, 200);
            auto sprite_size = glm::vec2(64, 64);

            auto swidth = (f32)bitmap.width;
            auto sheight = (f32)bitmap.height;

            auto projection = glm::mat3(1);
            projection = glm::translate(projection, glm::vec2(0, 1));
            projection = glm::scale(projection, glm::vec2(1 / swidth, -1 / sheight));

            DrawSprite(0, 0, 1, 1, sprite_pos, sprite_size, angle, projection);

            glEnd();
        }

        auto gsize = state.game_map.size;
        for (int y = 0; y < gsize.y; y++) {
            for (int x = 0; x < gsize.x; x++) {
                // TestSmartTile(TileID* tile_ids, size_t tile_ids_distance, v2i size, v2i pos,
                // SmartTile& tile)
                auto terrain_tiles = state.game_map.terrain_tiles;
                Terrain tile = *(Terrain*)(terrain_tiles + gsize.x * y + x);
                if (tile != Terrain::GRASS)
                    continue;

                auto texture_id = TestSmartTile(
                    state.game_map.terrain_tiles, sizeof(TileID), state.game_map.size, v2i(x, y),
                    state.grass_smart_tile);

                glBindTexture(GL_TEXTURE_2D, texture_id);
                glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
                // glBlendFunc(GL_ONE, GL_ZERO);
                glBegin(GL_TRIANGLES);

                auto cell_size = 32;
                auto sprite_pos =
                    v2i((x + 1) * cell_size,  //
                        (y + 4) * cell_size);
                auto sprite_size = v2i(cell_size, cell_size);

                auto swidth = (f32)bitmap.width;
                auto sheight = (f32)bitmap.height;

                auto projection = glm::mat3(1);
                projection = glm::translate(projection, glm::vec2(0, 1));
                projection = glm::scale(projection, glm::vec2(1 / swidth, -1 / sheight));
                projection = glm::rotate(projection, -0.2f);

                DrawSprite(0, 0, 1, 1, sprite_pos, sprite_size, 0, projection);

                glEnd();
            }
        }

        glDeleteTextures(1, (GLuint*)&texture_name);
        if (state.human_texture.address)
            glDeleteTextures(1, (GLuint*)&human_texture_name);
        assert(!glGetError());
        // --- RENDERING END
    }
}
