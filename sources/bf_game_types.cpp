#pragma once

// ============================================================= //
//                     Forward Declarations                      //
// ============================================================= //
// SHIT(hulvdan): Oh, god, I hate this shit
struct Game_State;

#ifdef BF_CLIENT
struct Game_Renderer_State;
struct Loaded_Texture;
#endif

// ============================================================= //
//                            Memory                             //
// ============================================================= //
struct Arena : public Non_Copyable {
    size_t used;
    size_t size;
    u8* base;
};

struct Page : public Non_Copyable {
    u8* base;
};

struct Pages : public Non_Copyable {
    size_t total_count_cap;
    size_t allocated_count;
    Page* base;
    bool* in_use;
};

// ============================================================= //
//                        Data Structures                        //
// ============================================================= //
template <typename T>
struct Fixed_Size_Queue {
    size_t memory_size;
    size_t count;
    T* base;
};

template <typename T>
void Enqueue(Fixed_Size_Queue<T>& queue, const T& value) {
    // TODO(hulvdan): Test!

    Assert(queue.memory_size >= (queue.count + 1) * sizeof(T));

    *(queue.base + queue.count) = value;
    queue.count++;
}

template <typename T>
T Dequeue(Fixed_Size_Queue<T>& queue) {
    // TODO(hulvdan): Test!

    Assert(queue.base != nullptr);
    Assert(queue.count > 0);

    T res = *queue.base;
    queue.count -= 1;
    memmove(queue.base, queue.base + 1, sizeof(T) * queue.count);

    return res;
}

// ============================================================= //
//                          Game Logic                           //
// ============================================================= //
enum class Direction {
    Right = 0,
    Up = 1,
    Left = 2,
    Down = 3,
};

v2i As_Offset(Direction dir) {
    Assert((u8)dir >= 0);
    Assert((u8)dir < 4);
    return v2i_adjacent_offsets[(int)dir];
}

Direction Opposite(Direction dir) {
    Assert((u8)dir >= 0);
    Assert((u8)dir < 4);
    return (Direction)(((u8)(dir) + 2) % 4);
}

// using Graph_Segment_ID = u32;
using Graph_u = i8;
using Graph_Nodes_Count = u16;

struct Graph_v2u {
    Graph_u x;
    Graph_u y;
};

Graph_v2u To_Graph_v2u(v2i pos) {
    // Graph_v2u res;
    // res.x = (Graph_u)pos.x;
    // res.y = (Graph_u)pos.y;
    return Graph_v2u(pos.x, pos.y);
}

// NOTE(hulvdan): `Graph_Segment_Page_Meta` gets placed at the end of the `Page`
struct Graph_Segment_Page_Meta : public Non_Copyable {
    u16 count;
};

struct Graph : public Non_Copyable {
    Graph_Nodes_Count nodes_count;
    u8* nodes;  // 0b0000DLUR

    Graph_v2u size;
    Graph_v2u offset;

    // Graph_v2u* centers;
    // Graph_double_u centers_count;
};

struct Graph_Segment : public Non_Copyable {
    Graph_Nodes_Count vertices_count;
    Graph_v2u* vertices;

    Graph graph;
    bool active;
};

struct Graph_Segment_Precalculated_Data {
    // TODO(hulvdan): Reimplement `CalculatedGraphPathData` calculation from the old repo
};

bool Graph_Node_Has(u8 node, Direction d) {
    Assert((u8)d >= 0);
    Assert((u8)d < 4);
    return node & (1 << (u8)d);
}

u8 Graph_Node_Mark(u8 node, Direction d, b32 value) {
    Assert((u8)d >= 0);
    Assert((u8)d < 4);
    auto dir = (u8)d;

    if (value)
        node |= (u8)(1 << dir);
    else
        node &= (u8)(15 ^ (1 << dir));

    Assert(node < 16);
    return node;
}

void Graph_Update(Graph& graph, int x, int y, Direction dir, b32 value) {
    Assert((u8)dir >= 0);
    Assert((u8)dir < 4);
    Assert(graph.offset.x == 0);
    Assert(graph.offset.y == 0);
    auto& node = *(graph.nodes + y * graph.size.x + x);

    b32 node_is_zero_but_wont_be_after = (node == 0) && value;
    b32 node_is_not_zero_but_will_be =
        (!value) && (node != 0) && (Graph_Node_Mark(node, dir, false) == 0);

    if (node_is_zero_but_wont_be_after)
        graph.nodes_count += 1;
    else if (node_is_not_zero_but_will_be)
        graph.nodes_count -= 1;

    node = Graph_Node_Mark(node, dir, value);
}

using Scriptable_Building_ID = u16;
global Scriptable_Building_ID global_city_hall_building_id = 1;
global Scriptable_Building_ID global_lumberjacks_hut_building_id = 2;

using Scriptable_Resource_ID = u16;
global Scriptable_Resource_ID global_forest_resource_id = 1;

using Building_ID = u32;
using Human_ID = u32;

enum class Building_Type {
    City_Hall,
    Harvest,
    Plant,
    Fish,
    Produce,
};

struct Scriptable_Building : public Non_Copyable {
    const char* name;
    Building_Type type;

#ifdef BF_CLIENT
    Loaded_Texture* texture;
#endif  // BF_CLIENT

    Scriptable_Resource_ID harvestable_resource_id;
};

struct Human : public Non_Copyable {
    //
};

struct Resource_To_Book : public Non_Copyable {
    Scriptable_Resource_ID scriptable_id;
    u8 amount;
};

// NOTE(hulvdan): `Building_Page_Meta` gets placed at the end of the `Page`
struct Building_Page_Meta : public Non_Copyable {
    u16 count;
};

struct Building : public Non_Copyable {
    Human_ID constructor;
    Human_ID employee;

    Scriptable_Building_ID scriptable_id;

    size_t resources_to_book_count;
    Resource_To_Book* resources_to_book;
    v2i pos;

    Building_ID id;
    f32 time_since_human_was_created;
    // f32 time_since_item_was_placed;
    bool active;
};

enum class Terrain {
    None = 0,
    Grass,
};

struct Terrain_Tile : public Non_Copyable {
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

struct Element_Tile : public Non_Copyable {
    Element_Tile_Type type;
    Building* building;
};

void Validate_Element_Tile(Element_Tile& tile) {
    Assert((int)tile.type >= 0);
    Assert((int)tile.type <= 3);

    if (tile.type == Element_Tile_Type::Building)
        Assert(tile.building != nullptr);
    else
        Assert(tile.building == nullptr);
}

struct Scriptable_Resource : public Non_Copyable {
    const char* name;
};

// NOTE(hulvdan): `scriptable` is `null` when `amount` = 0
struct Terrain_Resource {
    Scriptable_Resource_ID scriptable_id;

    u8 amount;
};

enum class Item_To_Build_Type {
    None,
    Road,
    Flag,
    Building,
};

struct Item_To_Build : public Non_Copyable {
    Item_To_Build_Type type;
    Scriptable_Building_ID scriptable_building_id;

    Item_To_Build(Item_To_Build_Type a_type, Scriptable_Building_ID a_scriptable_building_id)
        : type(a_type), scriptable_building_id(a_scriptable_building_id) {}
};

static const Item_To_Build Item_To_Build_Road(Item_To_Build_Type::Road, 0);
static const Item_To_Build Item_To_Build_Flag(Item_To_Build_Type::Flag, 0);

struct Game_Map : public Non_Copyable {
    v2i size;
    Terrain_Tile* terrain_tiles;
    Terrain_Resource* terrain_resources;
    Element_Tile* element_tiles;

    Page* building_pages;
    u16 building_pages_used;
    u16 building_pages_total;
    u16 max_buildings_per_page;

    Page* segment_pages;
    u16 segment_pages_used;
    u16 segment_pages_total;
    u16 max_segments_per_page;
};

template <typename T>
struct Observer : public Non_Copyable {
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

#define On_Item_Built__Function(name_) \
    void name_(Game_State& state, v2i pos, const Item_To_Build& item)

struct Game_State : public Non_Copyable {
    bool hot_reloaded;

    f32 offset_x;
    f32 offset_y;

    v2f player_pos;
    Game_Map game_map;

    size_t scriptable_resources_count;
    Scriptable_Resource* scriptable_resources;
    size_t scriptable_buildings_count;
    Scriptable_Building* scriptable_buildings;

    Arena arena;
    Arena non_persistent_arena;  // Gets flushed on DLL reloads
    Arena trash_arena;  // Use for transient calculations

    OS_Data* os_data;
    Pages pages;

#ifdef BF_CLIENT
    Game_Renderer_State* renderer_state;
#endif  // BF_CLIENT

    Observer<On_Item_Built__Function((*))> On_Item_Built;
};

struct Game_Memory : public Non_Copyable {
    bool is_initialized;
    Game_State state;
};

enum class Tile_Updated_Type {
    Road_Placed,
    Flag_Placed,
    Flag_Removed,
    Road_Removed,
    Building_Placed,
    Building_Removed,
};

// struct On_Tiles_Updated_Result : public Non_Copyable {
// struct On_Tiles_Updated_Result {
//     u16 added_segments_count;
//     u16 deleted_segments_count;
//
//     Graph_Segment* added_segments;
//     Graph_Segment* deleted_segments;
// };

#ifdef BF_CLIENT
// ============================================================= //
//                    CLIENT. Game Rendering                     //
// ============================================================= //
using BF_Texture_ID = u32;

struct Loaded_Texture {
    BF_Texture_ID id;
    v2i size;
    u8* base;
};

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

struct UI_Sprite_Params {
    bool smart_stretchable;
    v2i stretch_paddings_h;
    v2i stretch_paddings_v;
};

struct BF_Color {
    f32 r;
    f32 g;
    f32 b;
};

static constexpr BF_Color BF_Color_White = {1, 1, 1};
static constexpr BF_Color BF_Color_Black = {0, 0, 0};
static constexpr BF_Color BF_Color_Red = {1, 0, 0};
static constexpr BF_Color BF_Color_Green = {0, 1, 0};
static constexpr BF_Color BF_Color_Blue = {0, 0, 1};
static constexpr BF_Color BF_Color_Yellow = {1, 1, 0};
static constexpr BF_Color BF_Color_Cyan = {0, 1, 1};
static constexpr BF_Color BF_Color_Magenta = {1, 0, 1};

struct Game_UI_State : public Non_Copyable {
    UI_Sprite_Params buildables_panel_params;
    Loaded_Texture buildables_panel_background;
    Loaded_Texture buildables_placeholder_background;

    u16 buildables_count;
    Item_To_Build* buildables;

    i8 selected_buildable_index;
    BF_Color selected_buildable_color;
    BF_Color not_selected_buildable_color;
    v2f buildables_panel_sprite_anchor;
    v2f buildables_panel_container_anchor;
    f32 buildables_panel_in_scale;
    f32 scale;

    v2i padding;
    i8 placeholders;
    i16 placeholders_gap;
};

struct Game_Renderer_State : public Non_Copyable {
    Game_UI_State* ui_state;
    Game_Bitmap* bitmap;

    Smart_Tile grass_smart_tile;
    Smart_Tile forest_smart_tile;
    Tile_ID forest_top_tile_id;

    Loaded_Texture human_texture;
    Loaded_Texture grass_textures[17];
    Loaded_Texture forest_textures[3];
    Loaded_Texture road_textures[16];
    Loaded_Texture flag_textures[4];

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

    bool shaders_compilation_failed;
    GLint ui_shader_program;
};
#endif  // BF_CLIENT
