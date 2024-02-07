#pragma once

// --- Forward Declarations ---
// SHIT(hulvdan): Oh, god, I hate this shit
struct Game_State;

#ifdef BF_CLIENT
struct Game_Renderer_State;
#endif
// --- Forward Declarations End ---

// --- Memory ---
struct Arena {
    size_t used;
    size_t size;
    u8* base;
};
// --- Memory End ---

// --- Game Logic ---
using BuildingID = u32;

struct Building {
    BuildingID id;
    v2i pos;
};

enum class Terrain {
    None,
    Grass,
};

struct Terrain_Tile {
    Terrain terrain;
    // NOTE(hulvdan): Height starts at 0
    int height;
    bool is_cliff;
};

// NOTE(hulvdan): Upon editing ensure `Validate_Element_Tile` remains correct
enum class Element_Tile_Type {
    None = 0,
    Road = 1,
    Building = 2,
    Flag = 3,
};

struct Element_Tile {
    Element_Tile_Type type;
    Building* building;
};

void Validate_Element_Tile(Element_Tile& tile) {
    assert((int)tile.type >= 0);
    assert((int)tile.type <= 3);

    if (tile.type == Element_Tile_Type::Building)
        assert(tile.building != nullptr);
    else
        assert(tile.building == nullptr);
}

struct Scriptable_Resource {
    u32 id;
    const char* name;
};

// NOTE(hulvdan): `scriptable` is `null` when `amount` = 0
struct Terrain_Resource {
    Scriptable_Resource* scriptable;
    u8 amount;
};

enum class Item_To_Build {
    None,
    Road,
};

struct Game_Map {
    v2i size;
    Terrain_Tile* terrain_tiles;
    Terrain_Resource* terrain_resources;
    Element_Tile* element_tiles;
};

template <typename T>
struct Observer {
    size_t count;
    T* functions;
};

// Usage:
//     INVOKE_OBSERVER(state.On_Item_Built, (state, game_map, pos, item))
#define INVOKE_OBSERVER(observer, code)                 \
    {                                                   \
        FOR_RANGE(size_t, i, observer.count) {          \
            auto& function = *(observer.functions + i); \
            function code;                              \
        }                                               \
    }

// using On_Item_Built_FType =
//     void (*)(Game_State& state, Game_Map& game_map, v2i pos, Item_To_Build item);

#define On_Item_Built__Function(name_) \
    void name_(Game_State& state, Game_Map& game_map, v2i pos, Item_To_Build item)
#define On_Item_Built__Type On_Item_Built__Function((*))

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

    Observer<On_Item_Built__Type> On_Item_Built;
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
    Skip,
    Excluded,
    Included,
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
    Loaded_Texture road_textures[16];

    int tilemaps_count;
    Tilemap* tilemaps;

    u8 terrain_tilemaps_count;
    u8 resources_tilemap_index;
    u8 element_tilemap_index;

    v2i mouse_pos;
    bool panning;
    v2f pan_pos;
    v2f pan_offset;
    v2i pan_start_pos;
    f32 zoom_target;
    f32 zoom;
};
// --- CLIENT. Game Rendering End ---
#endif  // BF_CLIENT
