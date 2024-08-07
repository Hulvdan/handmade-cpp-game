#pragma once

// =============================================================
// Forward Declarations
// =============================================================
// SHIT: Oh, god, I hate this shit
struct Game_State;

#ifdef BF_CLIENT
struct Game_Renderer_State;
struct Loaded_Texture;
#endif

using Entity_ID                   = u32;
const Entity_ID Entity_ID_Missing = 1 << 31;

constexpr Entity_ID Component_Mask(Entity_ID component_number) {
    return component_number << 22;
}

using Player_ID = u8;

using Human_ID                  = Entity_ID;
const Human_ID Human_ID_Missing = Entity_ID_Missing;

using Human_Constructor_ID                              = Human_ID;
const Human_Constructor_ID Human_Constructor_ID_Missing = Entity_ID_Missing;

using Building_ID                     = Entity_ID;
const Building_ID Building_ID_Missing = Entity_ID_Missing;

using Texture_ID                        = i16;
constexpr Texture_ID Texture_ID_Missing = std::numeric_limits<Texture_ID>::max();
// constexpr Texture_ID Texture_ID_Missing = 0;

// TODO: Куда-то перенести эту функцию
template <>
struct std::hash<v2i16> {
    size_t operator()(const v2i16& o) const noexcept {
        size_t h1 = std::hash<i16>{}(o.x);
        size_t h2 = std::hash<i16>{}(o.y);
        return h1 ^ (h2 << 1);
    }
};

// =============================================================
// Game Logic
// =============================================================
enum class Direction {
    Right = 0,
    Up    = 1,
    Left  = 2,
    Down  = 3,
};

#define FOR_DIRECTION(var_name)                             \
    for (auto var_name = (Direction)0; (int)(var_name) < 4; \
         var_name      = (Direction)((int)(var_name) + 1))

v2i16 As_Offset(Direction dir) {
    Assert((u8)dir >= 0);
    Assert((u8)dir < 4);
    return v2i16_adjacent_offsets[(unsigned int)dir];
}

Direction Opposite(Direction dir) {
    Assert((u8)dir >= 0);
    Assert((u8)dir < 4);
    return (Direction)(((u8)(dir) + 2) % 4);
}

using Graph_Nodes_Count = u16;

struct Calculated_Graph_Data {
    i16* dist;
    i16* prev;

    std::unordered_map<u16, v2i16> node_index_2_pos;
    std::unordered_map<v2i16, u16> pos_2_node_index;

    v2i16 center;
};

struct Graph : public Non_Copyable {
    Graph_Nodes_Count nodes_count;
    size_t            nodes_allocation_count;
    u8*               nodes;  // 0b0000DLUR

    v2i16 size;
    v2i16 offset;

    Calculated_Graph_Data* data;
};

BF_FORCE_INLINE bool Graph_Contains(const Graph& graph, v2i16 pos) {
    auto off    = pos - graph.offset;
    bool result = Pos_Is_In_Bounds(off, graph.size);
    return result;
}

BF_FORCE_INLINE u8 Graph_Node(const Graph& graph, v2i16 pos) {
    auto off = pos - graph.offset;
    Assert(Pos_Is_In_Bounds(off, graph.size));

    u8* node_ptr = graph.nodes + off.y * graph.size.x + off.x;
    u8  result   = *node_ptr;
    return result;
}

struct Scriptable_Resource;
struct Graph_Segment;

// using Graph_Segment_ID = Bucket_Locator;
// const Graph_Segment_ID No_Graph_Segment_ID(-1, -1);
// using Map_Resource_ID = Bucket_Locator;
// const Map_Resource_ID No_Map_Resource_ID(-1, -1);

using Graph_Segment_ID                          = Entity_ID;
const Graph_Segment_ID Graph_Segment_ID_Missing = Entity_ID_Missing;

using Map_Resource_ID                         = Entity_ID;
const Map_Resource_ID Map_Resource_ID_Missing = Entity_ID_Missing;

using Map_Resource_Booking_ID                                 = u16;
const Map_Resource_Booking_ID Map_Resource_Booking_ID_Missing = 0;

enum class Map_Resource_Booking_Type {
    Construction,
    // Processing,
};

struct Map_Resource_Booking {
    static const Entity_ID component_mask = Component_Mask(4);

    Map_Resource_Booking_Type type;
    Building_ID               building_id;
};

struct Map_Resource {
    static const Entity_ID component_mask = Component_Mask(3);

    Scriptable_Resource* scriptable;

    v2i16 pos;

    Map_Resource_Booking_ID booking;

    Vector<Graph_Segment_ID> transportation_segments;
    Vector<v2i16>            transportation_vertices;

    Human_ID targeted_human;  // optional
    Human_ID carrying_human;  // optional

    Map_Resource() {}

    Map_Resource(Map_Resource&& other)
        : scriptable(other.scriptable)
        , pos(other.pos)
        , booking(other.booking)
        , transportation_segments(std::move(other.transportation_segments))
        , transportation_vertices(std::move(other.transportation_vertices))
        , targeted_human(other.targeted_human)
        , carrying_human(other.carrying_human)  //
    {}
};

// NOTE: Сегмент - это несколько склеенных друг с другом клеток карты,
// на которых может находиться один человек, который перетаскивает предметы.
//
// Сегменты формируются между зданиями, дорогами и флагами.
// Обозначим их клетки следующими символами:
//
// B - Building - Здание.       "Вершинная" клетка
// F - Flag     - Флаг.         "Вершинная" клетка
// r - Road     - Дорога.    Не "вершинная" клетка
// . - Empty    - Пусто.
//
// Сегменты формируются между "вершинными" клетками, соединёнными дорогами.
//
// Примеры:
//
// 1)  rr - не сегмент, т.к. это просто 2 дороги - невершинные клетки.
//
// 2)  BB - не сегмент. Тут 2 здания соединены рядом, но между ними нет дороги.
//
// 3)  BrB - сегмент. 2 здания. Между ними есть дорога.
//
// 4)  BB
//     rr - сегмент. Хоть 2 здания стоят рядом, между ними можно пройти по дороге снизу.
//
// 5)  B.B
//     r.r
//     rrr - сегмент. Аналогично предыдущему, просто дорога чуть длиннее.
//
// 6)  BF - не сегмент. Тут здание и флаг соединены рядом, но между ними нет дороги.
//
// 7)  BrF - сегмент. Здание и флаг, между которыми дорога.
//
// 8)  FrF - сегмент. 2 флага, между которыми дорога.
//
// 9)  BrFrB - это уже 2 разных сегмента. Первый - BrF, второй - FrB.
//             При замене флага (F) на дорогу (r) эти 2 сегмента сольются в один - BrrrB.
//
using Graph_Segment_ID = u32;

struct Graph_Segment {
    static const Entity_ID component_mask = Component_Mask(1);

    Graph_Nodes_Count vertices_count;
    v2i16* vertices;  // NOTE: Вершинные клетки графа (флаги, здания)

    Graph graph;

    Human_ID                 assigned_human_id;  // optional
    Vector<Graph_Segment_ID> linked_segments;

    Queue<Map_Resource> resources_to_transport;
};

struct Graph_Segment_Precalculated_Data {
    // TODO: Reimplement `CalculatedGraphPathData` calculation from the old repo
};

[[nodiscard]] BF_FORCE_INLINE u8 Graph_Node_Has(u8 node, Direction d) {
    Assert((u8)d >= 0);
    Assert((u8)d < 4);
    return node & (u8)((u8)1 << (u8)d);
}

[[nodiscard]] u8 Graph_Node_Mark(u8 node, Direction d, b32 value) {
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

void Graph_Update(Graph& graph, v2i16 pos, Direction dir, bool value) {
    Assert((u8)dir >= 0);
    Assert((u8)dir < 4);
    Assert(graph.offset.x == 0);
    Assert(graph.offset.y == 0);
    auto& node = *(graph.nodes + pos.y * graph.size.x + pos.x);

    bool node_is_zero_but_wont_be_after = (node == 0) && value;
    bool node_is_not_zero_but_will_be
        = (!value) && (node != 0) && (Graph_Node_Mark(node, dir, false) == 0);

    if (node_is_zero_but_wont_be_after)
        graph.nodes_count += 1;
    else if (node_is_not_zero_but_will_be)
        graph.nodes_count -= 1;

    node = Graph_Node_Mark(node, dir, value);
}

using Player_ID = u8;
using Human_ID  = u32;

enum class Building_Type {
    Undefined = 0,
    City_Hall,
    Harvest,
    Plant,
    Fish,
    Produce,
};

struct Scriptable_Building : public Non_Copyable {
    const char*   code;
    Building_Type type;

    Scriptable_Resource* harvestable_resource;

    f32 human_spawning_delay;
    f32 required_construction_points;

    bool can_be_built;

    Vector<std::tuple<Scriptable_Resource*, i16>> construction_resources;

#ifdef BF_CLIENT
    Texture_ID texture;
#endif  // BF_CLIENT
};

enum class Human_Type {
    Transporter,
    Constructor,
    Employee,
};

struct Human_Moving_Component {
    v2i16 pos;
    f32   elapsed;
    f32   progress;
    v2f   from;

    std::optional<v2i16> to;
    Queue<v2i16>         path;
};

enum class Moving_In_The_World_State {
    None = 0,
    Moving_To_The_City_Hall,
    Moving_To_Destination,
};

struct Main_Controller;

struct Moving_In_The_World {
    Moving_In_The_World_State state;
    Main_Controller*          controller;
};

enum class Human_Main_State {
    None = 0,

    // Common
    Moving_In_The_World,

    // Transporter
    Moving_Inside_Segment,
    Moving_Resource,

    // Builder
    Building,

    // Employee
    Employee,
};

struct Map_Resource_To_Book : public Non_Copyable {
    Scriptable_Resource* scriptable;
    u8                   count;
    Building_ID          building_id;

    Map_Resource_To_Book(
        Scriptable_Resource* a_scriptable,
        u8                   a_count,
        Building_ID          a_building_id
    )
        : scriptable(a_scriptable)  //
        , count(a_count)
        , building_id(a_building_id) {}
};

struct Resource_To_Book : public Non_Copyable {
    Scriptable_Resource* scriptable;
    u8                   count;
};

enum class Terrain {
    None = 0,
    Grass,
};

struct Terrain_Tile : public Non_Copyable {
    Terrain terrain;

    int  height;  // NOTE: starts at 0
    bool is_cliff;

    i16 resource_amount;
};

// NOTE: Upon editing ensure that `Validate_Element_Tile` remains correct
enum class Element_Tile_Type {
    None     = 0,
    Road     = 1,
    Building = 2,
    Flag     = 3,
};

struct Element_Tile : public Non_Copyable {
    Element_Tile_Type type;
    Building_ID       building_id;
    Player_ID         player_id;
};

void Validate_Element_Tile(Element_Tile& tile) {
    Assert((int)tile.type >= 0);
    Assert((int)tile.type <= 3);

    if (tile.type == Element_Tile_Type::Building)
        Assert(tile.building_id != Building_ID_Missing);
    else
        Assert(tile.building_id == Building_ID_Missing);
}

struct Scriptable_Resource : public Non_Copyable {
    const char* code;

#ifdef BF_CLIENT
    Texture_ID texture;
    Texture_ID small_texture;
#endif  // BF_CLIENT
};

// NOTE: `scriptable` is `null` when `amount` = 0
struct Terrain_Resource {
    Scriptable_Resource* scriptable;

    u8 amount;
};

enum class Item_To_Build_Type {
    None,
    Road,
    Flag,
    Building,
};

struct Item_To_Build : public Non_Copyable {
    Item_To_Build_Type   type;
    Scriptable_Building* scriptable_building;

    Item_To_Build(Item_To_Build_Type a_type, Scriptable_Building* a_scriptable_building)
        : type(a_type)
        , scriptable_building(a_scriptable_building) {}
};

static const Item_To_Build Item_To_Build_Road(Item_To_Build_Type::Road, nullptr);
static const Item_To_Build Item_To_Build_Flag(Item_To_Build_Type::Flag, nullptr);

enum class Human_Removal_Reason {
    Transporter_Returned_To_City_Hall,
    Employee_Reached_Building,
};

struct Human {
    static const Entity_ID component_mask = Component_Mask(1);

    Human_Moving_Component moving;

    Player_ID  player_id;
    Human_Type type;

    Human_Main_State          state;
    Moving_In_The_World_State state_moving_in_the_world;

    Graph_Segment_ID segment_id;
    Building_ID      building_id;
};

struct Building {
    static const Entity_ID component_mask = Component_Mask(2);

    v2i16                pos;
    Scriptable_Building* scriptable;
    Player_ID            player_id;
};

struct Not_Constructed_Building {
    Human_Constructor_ID constructor;  // optional
    f32                  construction_points;
    // Vector<Resource_To_Book> resources_to_book;
};

struct City_Hall {
    f32 time_since_human_was_created;
};

struct Game_Map_Data {
    f32 human_moving_one_tile_duration;

    Game_Map_Data(f32 a_human_moving_one_tile_duration)
        : human_moving_one_tile_duration(a_human_moving_one_tile_duration) {}
};

struct Human_Data;

struct Game_Map : public Non_Copyable {
    Entity_ID last_entity_id;

    v2i16             size;
    Terrain_Tile*     terrain_tiles;
    Terrain_Resource* terrain_resources;
    Element_Tile*     element_tiles;

    Game_Map_Data* data;
    Human_Data*    human_data;

    Sparse_Array<Graph_Segment_ID, Graph_Segment>       segments;
    Sparse_Array<Building_ID, Building>                 buildings;
    Sparse_Array<Building_ID, Not_Constructed_Building> not_constructed_buildings;
    Sparse_Array<Building_ID, City_Hall>                city_halls;
    Sparse_Array<Human_ID, Human>                       humans;
    Sparse_Array_Of_Ids<Human_ID>                       humans_going_to_the_city_hall;
    // Sparse_Array<Human_ID, Human_Transporter>           transporters;
    // Sparse_Array<Human_ID, Human_Constructor>           constructors;
    Sparse_Array<Human_ID, Human>                humans_to_add;
    Sparse_Array<Human_ID, Human_Removal_Reason> humans_to_remove;
    Sparse_Array<Map_Resource_ID, Map_Resource>  resources;

    Queue<Graph_Segment_ID>      segments_wo_humans;
    Vector<Map_Resource_To_Book> resources_booking_queue;
};

template <typename T>
struct Observer : public Non_Copyable {
    size_t count;
    T*     functions;
};

// Usage:
//     INVOKE_OBSERVER(state.On_Item_Built, (state, game_map, pos, item))
#define INVOKE_OBSERVER(observer, code)                    \
    do {                                                   \
        FOR_RANGE (size_t, i, observer.count) {            \
            auto&    function = *(observer.functions + i); \
            function code;                                 \
        }                                                  \
    } while (0)

// Usage:
//     On_Item_Built__Function((*callbacks[])) = {
//         Renderer__On_Item_Built,
//     };
//     INITIALIZE_OBSERVER_WITH_CALLBACKS(state.On_Item_Built, callbacks, arena);
#define INITIALIZE_OBSERVER_WITH_CALLBACKS(observer, callbacks, arena)                 \
    do {                                                                               \
        (observer).count = sizeof(callbacks) / sizeof(callbacks[0]);                   \
        (observer).functions                                                           \
            = (decltype((observer).functions))(Allocate_((arena), sizeof(callbacks))); \
        memcpy((observer).functions, callbacks, sizeof(callbacks));                    \
    } while (0)

#define On_Item_Built__Function(name_) \
    void name_(Game_State& state, v2i16 pos, const Item_To_Build& item, MCTX)

struct Game_State : public Non_Copyable {
    bool hot_reloaded;
    u16  dll_reloads_count;

    f32 offset_x;
    f32 offset_y;

    v2f      player_pos;
    Game_Map game_map;

    size_t               scriptable_resources_count;
    Scriptable_Resource* scriptable_resources;
    size_t               scriptable_buildings_count;
    Scriptable_Building* scriptable_buildings;

    Arena arena;
    Arena non_persistent_arena;  // Gets flushed on DLL reloads
    Arena trash_arena;           // Use for transient calculations

    Pages pages;

#ifdef BF_CLIENT
    Game_Renderer_State* renderer_state;
#endif  // BF_CLIENT

    Observer<On_Item_Built__Function((*))> On_Item_Built;

    const BFGame::Game_Library* gamelib;
};

struct Game_Memory : public Non_Copyable {
    bool       is_initialized;
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

Building* Get_Building(Game_Map& game_map, Building_ID id) {
    return game_map.buildings.Find(id);
}

#ifdef BF_CLIENT
// =============================================================
// CLIENT. Game Rendering
// =============================================================
using BF_Texture_ID = u32;

struct Loaded_Texture {
    BF_Texture_ID id;
    v2i           size;
    u8*           base;
};

using Tile_ID = u32;

enum class Tile_State_Check {
    Skip,
    Excluded,
    Included,
};

struct Tile_Rule {
    Texture_ID       texture;
    Tile_State_Check states[8];
};

struct Smart_Tile {
    Tile_ID    id;
    Texture_ID fallback_texture;

    int        rules_count;
    Tile_Rule* rules;
};

struct Load_Smart_Tile_Result {
    bool success;
};

struct Tilemap {
    Tile_ID*    tiles;
    Texture_ID* textures;
    v2i         size;

    bool debug_rendering_enabled = true;
};

struct C_Texture {
    Texture_ID id;
    v2f        pos_inside_atlas;  // значения x, y ограничены [0, 1)
    v2i16      size;
};

struct C_Sprite {
    v2f        pos;
    v2f        scale;
    v2f        anchor;
    f32        rotation;
    Texture_ID texture;
    i8         z;
    // left   = pos.x + texture.size.x * (anchor.x - 1)
    // right  = pos.x + texture.size.x *  anchor.x
    // bottom = pos.y + texture.size.y * (anchor.y - 1)
    // top    = pos.y + texture.size.y *  anchor.y
    // max_scaled_off = MAX(t.size.x, t.size.y) * MAX(scale.x, scale.y)
};

// SELECT
// FROM sprites s
// JOIN texture t ON s.texture_id = t.id
// WHERE
//     -- Bounding box cutting
//         s.pos_x + (0 - s.anchor_x) * max_scaled_off >= arg_screen_left
//     AND s.pos_x + (1 - s.anchor_x) * max_scaled_off <= arg_screen_right
//     AND s.pos_y + (0 - s.anchor_y) * max_scaled_off >= arg_screen_bottom
//     AND s.pos_y + (1 - s.anchor_y) * max_scaled_off <= arg_screen_top
//
//     -- Without anchor multiplications
//         s.pos_x + max_scaled_off >= arg_screen_left
//     AND s.pos_x - max_scaled_off <= arg_screen_right
//     AND s.pos_y + max_scaled_off >= arg_screen_bottom
//     AND s.pos_y - max_scaled_off <= arg_screen_top
//
// ORDER BY s.z ASC

struct UI_Sprite_Params {
    bool smart_stretchable;
    v2i  stretch_paddings_h;
    v2i  stretch_paddings_v;
};

struct BF_Color {
    f32 r;
    f32 g;
    f32 b;
};

static constexpr BF_Color BF_Color_White   = {1, 1, 1};
static constexpr BF_Color BF_Color_Black   = {0, 0, 0};
static constexpr BF_Color BF_Color_Red     = {1, 0, 0};
static constexpr BF_Color BF_Color_Green   = {0, 1, 0};
static constexpr BF_Color BF_Color_Blue    = {0, 0, 1};
static constexpr BF_Color BF_Color_Yellow  = {1, 1, 0};
static constexpr BF_Color BF_Color_Cyan    = {0, 1, 1};
static constexpr BF_Color BF_Color_Magenta = {1, 0, 1};

struct Game_UI_State : public Non_Copyable {
    UI_Sprite_Params buildables_panel_params;
    // Loaded_Texture   buildables_panel_background;
    // Loaded_Texture   buildables_placeholder_background;

    Texture_ID buildables_panel_texture;
    Texture_ID buildables_placeholder_texture;

    u16            buildables_count;
    Item_To_Build* buildables;

    i8       selected_buildable_index;
    BF_Color selected_buildable_color;
    BF_Color not_selected_buildable_color;
    v2f      buildables_panel_sprite_anchor;
    v2f      buildables_panel_container_anchor;
    f32      buildables_panel_in_scale;
    f32      scale;

    v2i padding;
    i8  placeholders;
    i16 placeholders_gap;
};

struct Atlas {
    v2f               size;
    Loaded_Texture    texture;
    Vector<C_Texture> textures;
};

struct OpenGL_Framebuffer {
    GLuint id;
    v2i    size;
    GLuint color;  // color_texture_id
};

struct Game_Renderer_State : public Non_Copyable {
    Game_UI_State* ui_state;
    Game_Bitmap*   bitmap;

    Sparse_Array<Entity_ID, C_Sprite> sprites;

    Smart_Tile grass_smart_tile;
    Smart_Tile forest_smart_tile;
    Tile_ID    forest_top_tile_id;
    Tile_ID    flag_tile_id;

    Texture_ID human_texture;
    Texture_ID building_in_progress_texture;
    Texture_ID forest_textures[3];
    Texture_ID grass_textures[17];
    Texture_ID road_textures[16];
    Texture_ID flag_textures[4];

    Atlas atlas;

    int sprites_shader_gl2w_location;

    int tilemap_shader_gl2w_location;
    int tilemap_shader_relscreen2w_location;
    int tilemap_shader_resolution_location;
    int tilemap_shader_color_location;

    int      tilemaps_count;
    Tilemap* tilemaps;

    u8 terrain_tilemaps_count;
    u8 resources_tilemap_index;
    u8 element_tilemap_index;

    v2i mouse_pos;

    bool panning;
    v2f  pan_pos;
    v2f  pan_offset;
    v2i  pan_start_pos;
    f32  zoom_target;
    f32  zoom;

    f32 cell_size;

    bool   shaders_compilation_failed;
    GLuint sprites_shader_program;
    GLuint tilemap_shader_program;

    size_t rendering_indices_buffer_size;
    void*  rendering_indices_buffer;
};
#endif  // BF_CLIENT
