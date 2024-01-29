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

struct TerrainTile {
    Terrain terrain;
    // NOTE(hulvdan): Height starts at 0
    i8 height;
    bool is_cliff;
};

struct GameMap {
    v2i size;
    TerrainTile* terrain_tiles;
};

struct TerrainGenerationData {
    i32 max_height;  // INCLUSIVE
};


#ifdef BF_CLIENT
struct GameRendererState;
#endif

struct GameState {
    f32 offset_x;
    f32 offset_y;

    v2f player_pos;
    GameMap gamemap;

    Arena memory_arena;
    Arena file_loading_arena;

#ifdef BF_CLIENT
    GameRendererState* renderer_state;
#endif  // BF_CLIENT
};

struct GameMemory {
    bool is_initialized;
    GameState state;
};
// --- Game Logic End ---

#ifdef BF_CLIENT
// --- CLIENT. Rendering ---
using BFTextureID = u32;

struct LoadedTexture {
    BFTextureID id;
    v2i size;
    u8* address;
};
// --- CLIENT. Rendering End ---

// --- CLIENT. Game Rendering ---
using TileID = u32;

enum class TileStateCheck {
    SKIP,
    EXCLUDED,
    INCLUDED,
};

struct TileRule {
    BFTextureID texture_id;
    TileStateCheck states[8];
};

struct SmartTile {
    TileID id;
    BFTextureID fallback_texture_id;

    int rules_count;
    TileRule* rules;
};

struct LoadSmartTile_Result {
    bool success;
    size_t size;
};

struct Tilemap {
    v2i size;
    TileID* tiles;
};

struct GameRendererState {
    SmartTile grass_smart_tile;
    LoadedTexture human_texture;
    LoadedTexture grass_textures[17];

    int tilemaps_count;
    Tilemap* tilemaps;
};
// --- CLIENT. Game Rendering End ---
#endif  // BF_CLIENT
