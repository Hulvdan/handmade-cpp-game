#pragma once

// --- Memory ---
struct Arena {
    size_t used;
    size_t size;
    u8* base;
};
// --- Memory End ---

// --- Game Logic ---
enum class Terrain {
    NONE,
    GRASS,
};

struct Terrain_Tile {
    Terrain terrain;
    // NOTE(hulvdan): Height starts at 0
    int height;
    bool is_cliff;
};

struct Scriptable_Resource {
    u32 id;
    const char* name;
};

// NOTE(hulvdan): `scriptable` is `null` when `amount` = 0
struct Terrain_Resource {
    Scriptable_Resource* scriptable;
    u8 amount;
};

struct Game_Map {
    v2i size;
    Terrain_Tile* terrain_tiles;
    Terrain_Resource* terrain_resources;
};

#ifdef BF_CLIENT
struct Game_Renderer_State;
#endif

struct Game_State {
    f32 offset_x;
    f32 offset_y;

    v2f player_pos;
    Game_Map game_map;

    Scriptable_Resource DEBUG_forest;

    Arena memory_arena;
    Arena file_loading_arena;

#ifdef BF_CLIENT
    Game_Renderer_State* renderer_state;
#endif  // BF_CLIENT
};

struct Game_Memory {
    bool is_initialized;
    Game_State state;
};
// --- Game Logic End ---

#ifdef BF_CLIENT
// --- CLIENT. Rendering ---
using BF_Texture_ID = u32;

struct Loaded_Texture {
    BF_Texture_ID id;
    v2i size;
    u8* address;
};
// --- CLIENT. Rendering End ---

// --- CLIENT. Game Rendering ---
using Tile_ID = u32;

enum class Tile_State_Check {
    SKIP,
    EXCLUDED,
    INCLUDED,
};

struct Tile_Rule {
    BF_Texture_ID texture_id;
    Tile_State_Check states[8];
};

struct Smart_Tile {
    Tile_ID id;
    BF_Texture_ID fallback_texture_id;

    int rules_count;
    Tile_Rule* rules;
};

struct Load_Smart_Tile_Result {
    bool success;
};

struct Tilemap {
    Tile_ID* tiles;
};

struct Game_Renderer_State {
    // WARNING(hulvdan): It is not filled with 0 upon initialization!
    Smart_Tile grass_smart_tile;
    Smart_Tile forest_smart_tile;
    Tile_ID forest_top_tile_id;

    Loaded_Texture human_texture;
    Loaded_Texture grass_textures[17];
    Loaded_Texture forest_textures[3];

    int tilemaps_count;
    Tilemap* tilemaps;

    u8 terrain_tilemaps_count;
    u8 resources_tilemap_index;

    v2i mouse_pos;
    bool panning;
    v2f pan_pos;
    v2f pan_offset;
    v2i pan_start_pos;
    f32 zoom_target;
    f32 zoom;
    f32 zoom_t;
};
// --- CLIENT. Game Rendering End ---
#endif  // BF_CLIENT
