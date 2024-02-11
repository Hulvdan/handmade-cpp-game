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

using Scriptable_Building_ID = u32;
using Scriptable_Resource_ID = u32;
global Scriptable_Resource_ID global_forest_resource_id = 1;

using Building_ID = u32;
using Human_ID = u32;

struct Scriptable_Building {};

struct Human {};

struct Resource_To_Book {
    Scriptable_Building_ID scriptable_id;
    i8 amount;
};

struct Building {
    Human_ID constructor;
    Human_ID employee;

    Scriptable_Building_ID scriptable_id;

    size_t resources_to_book_count;
    Resource_To_Book* resources_to_book;
    v2i pos;

    Building_ID id;
    f32 timeSinceHumanWasCreated;
    f32 timeSinceItemWasPlaced;
};

enum class Terrain {
    None = 0,
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
    // u32 id;
    const char* name;
};

// NOTE(hulvdan): `scriptable` is `null` when `amount` = 0
struct Terrain_Resource {
    Scriptable_Resource_ID scriptable_id;

    u8 amount;
};

enum class Item_To_Build {
    None,
    Road,
    Flag,
};

struct Game_Map {
    v2i size;
    Terrain_Tile* terrain_tiles;
    Terrain_Resource* terrain_resources;
    Element_Tile* element_tiles;

    size_t buildings_count;
    Building* buildings;
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

// Usage:
//     On_Item_Built__Function((*callbacks[])) = {
//         Renderer__On_Item_Built,
//     };
//     INITIALIZE_OBSERVER_WITH_CALLBACKS(state.On_Item_Built, callbacks, arena);
#define INITIALIZE_OBSERVER_WITH_CALLBACKS(observer, callbacks, arena)               \
    {                                                                                \
        (observer).count = sizeof(callbacks) / sizeof(callbacks[0]);                 \
        (observer).functions =                                                       \
            (decltype((observer).functions))(Allocate_((arena), sizeof(callbacks))); \
        memcpy((observer).functions, callbacks, sizeof(callbacks));                  \
    }

#define On_Item_Built__Function(name_) void name_(Game_State& state, v2i pos, Item_To_Build item)

struct Game_State {
    f32 offset_x;
    f32 offset_y;

    v2f player_pos;
    Game_Map game_map;

    size_t scriptable_resources_count;
    Scriptable_Resource* scriptable_resources;
    size_t scriptable_buildings_count;
    Scriptable_Building* scriptable_buildings;

    Arena memory_arena;
    Arena file_loading_arena;

#ifdef BF_CLIENT
    Game_Renderer_State* renderer_state;
#endif  // BF_CLIENT

    Observer<On_Item_Built__Function((*))> On_Item_Built;
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
    Game_Bitmap* bitmap;

    Smart_Tile grass_smart_tile;
    Smart_Tile forest_smart_tile;
    Tile_ID forest_top_tile_id;

    Loaded_Texture human_texture;
    Loaded_Texture grass_textures[17];
    Loaded_Texture forest_textures[3];
    Loaded_Texture road_textures[16];
    Loaded_Texture flag_textures[4];

    size_t building_textures_count;
    Loaded_Texture* building_textures;

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

    f32 cell_size;
};
// --- CLIENT. Game Rendering End ---
#endif  // BF_CLIENT
