#pragma once

// ============================================================= //
//                     Forward Declarations                      //
// ============================================================= //
// SHIT: Oh, god, I hate this shit
struct Game_State;

#ifdef BF_CLIENT
struct Game_Renderer_State;
struct Loaded_Texture;
#endif

// TODO: Куда-то перенести эту функцию
template <>
struct std::hash<v2i16> {
    size_t operator()(const v2i16& o) const noexcept {
        size_t h1 = std::hash<i16>{}(o.x);
        size_t h2 = std::hash<i16>{}(o.y);
        return h1 ^ (h2 << 1);
    }
};

// ============================================================= //
//                          Game Logic                           //
// ============================================================= //
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

struct Human;
struct Building;
struct Scriptable_Resource;
struct Graph_Segment;

enum class Map_Resource_Booking_Type {
    Construction,
    Processing,
};

struct Map_Resource_Booking {
    Map_Resource_Booking_Type type;
    Building*                 building;
    i32                       priority;
};

struct Map_Resource {
    using ID = u32;

    ID                   id;
    Scriptable_Resource* scriptable;

    v2i16 pos;

    Map_Resource_Booking* booking;

    Vector<Graph_Segment*> transportation_segments;
    Vector<v2i16>          transportation_vertices;

    Human* targeted_human;
    Human* carrying_human;

    Bucket_Locator locator;
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
struct Graph_Segment : public Non_Copyable {
    Graph_Nodes_Count vertices_count;
    v2i16* vertices;  // NOTE: Вершинные клетки графа (флаги, здания)

    Graph          graph;
    Bucket_Locator locator;

    Human*                      assigned_human;
    std::vector<Graph_Segment*> linked_segments;

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

using Scriptable_Resource_ID = u16;

global_var Scriptable_Resource_ID global_forest_resource_id = 1;

using Player_ID = u8;
using Human_ID  = u32;

enum class Building_Type {
    City_Hall,
    Harvest,
    Plant,
    Fish,
    Produce,
};

struct Scriptable_Building : public Non_Copyable {
    const char*   name;
    Building_Type type;

#ifdef BF_CLIENT
    Loaded_Texture* texture;
#endif  // BF_CLIENT

    Scriptable_Resource_ID harvestable_resource_id;

    f32 human_spawning_delay;
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

    toptional<v2i16> to;
    Queue<v2i16>     path;
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

struct Building;

struct Human : public Non_Copyable {
    Player_ID              player_id;
    Human_Moving_Component moving;

    Graph_Segment* segment;
    Human_Type     type;

    Human_Main_State          state;
    Moving_In_The_World_State state_moving_in_the_world;

    // Human_ID id;
    Building* building;
    // Moving_Resources_State state_moving_resources;
    //
    // f32 moving_resources__picking_up_resource_elapsed;
    // f32 moving_resources__picking_up_resource_progress;
    // f32 moving_resources__placing_resource_elapsed;
    // f32 moving_resources__placing_resource_progress;
    //
    // Map_Resource* moving_resources__targeted_resource;
    //
    // f32 harvesting_elapsed;
    // i32 current_behaviour_id = -1;
    //
    // Employee_Behaviour_Set behaviour_set;
    // f32 processing_elapsed;

    Human(const Human&& o) {
        player_id                 = std::move(o.player_id);
        moving                    = std::move(o.moving);
        segment                   = std::move(o.segment);
        type                      = std::move(o.type);
        state                     = std::move(o.state);
        state_moving_in_the_world = std::move(o.state_moving_in_the_world);
        building                  = std::move(o.building);
    }

    Human& operator=(Human&& o) {
        player_id                 = std::move(o.player_id);
        moving                    = std::move(o.moving);
        segment                   = std::move(o.segment);
        type                      = std::move(o.type);
        state                     = std::move(o.state);
        state_moving_in_the_world = std::move(o.state_moving_in_the_world);
        building                  = std::move(o.building);
        return *this;
    }
};

struct Resource_To_Book : public Non_Copyable {
    Scriptable_Resource_ID scriptable_id;
    u8                     amount;
};

struct Building : public Non_Copyable {
    using ID = u32;

    ID        id;
    Player_ID player_id;

    Human* constructor;
    Human* employee;

    Scriptable_Building* scriptable;

    size_t            resources_to_book_count;
    Resource_To_Book* resources_to_book;
    v2i16             pos;

    f32 time_since_human_was_created;

    bool employee_is_inside;
    bool is_constructed;

    // Bucket_Locator locator;
    // f32 time_since_item_was_placed;
};

enum class Terrain {
    None = 0,
    Grass,
};

struct Terrain_Tile : public Non_Copyable {
    Terrain terrain;
    // NOTE: Height starts at 0
    int  height;
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
    Building*         building;
    Player_ID         player_id;
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

#ifdef BF_CLIENT
    // TODO: Исползовать handle-ы текстур? Что-то вроде хешей,
    // а игра бы подгружала бы эти текстуры асинхронно
    Loaded_Texture* texture;
    Loaded_Texture* small_texture;
#endif  // BF_CLIENT
};

// NOTE: `scriptable` is `null` when `amount` = 0
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
    Item_To_Build_Type   type;
    Scriptable_Building* scriptable_building;

    Item_To_Build(Item_To_Build_Type a_type, Scriptable_Building* a_scriptable_building)
        : type(a_type), scriptable_building(a_scriptable_building) {}
};

static const Item_To_Build Item_To_Build_Road(Item_To_Build_Type::Road, nullptr);
static const Item_To_Build Item_To_Build_Flag(Item_To_Build_Type::Flag, nullptr);

enum class Human_Removal_Reason {
    Transporter_Returned_To_City_Hall,
    Employee_Reached_Building,
};

struct Game_Map_Data {
    f32 human_moving_one_tile_duration;

    Game_Map_Data(f32 a_human_moving_one_tile_duration)
        : human_moving_one_tile_duration(a_human_moving_one_tile_duration) {}
};
struct Human_Data;

struct Game_Map : public Non_Copyable {
    v2i16             size;
    Terrain_Tile*     terrain_tiles;
    Terrain_Resource* terrain_resources;
    Element_Tile*     element_tiles;

    Bucket_Array<Building>      buildings;
    Bucket_Array<Graph_Segment> segments;
    Bucket_Array<Human>         humans;

    Bucket_Array<Human> humans_to_add;

    struct Human_To_Remove {
        Human_Removal_Reason reason;
        Human*               human;
        Bucket_Locator       locator;
    };
    std::vector<Human_To_Remove> humans_to_remove;

    // NOTE: Выделяем pool, т.к. эти ресурсы могут перемещаться из контейнера
    // карты к чувачкам в руки, в здания, а также обратно в `resources`.
    // TODO: !!! Pool
    Bucket_Array<Map_Resource>     resources_pool;
    Grid_Of_Vectors<Map_Resource*> resources;

    Queue<Graph_Segment*> segments_wo_humans;

    Game_Map_Data* data;
    Human_Data*    human_data;
};

template <typename T>
struct Observer : public Non_Copyable {
    size_t count;
    T*     functions;
};

// Usage:
//     INVOKE_OBSERVER(state.On_Item_Built, (state, game_map, pos, item))
#define INVOKE_OBSERVER(observer, code)                    \
    {                                                      \
        FOR_RANGE (size_t, i, observer.count) {            \
            auto&    function = *(observer.functions + i); \
            function code;                                 \
        }                                                  \
    }

// Usage:
//     On_Item_Built__Function((*callbacks[])) = {
//         Renderer__On_Item_Built,
//     };
//     INITIALIZE_OBSERVER_WITH_CALLBACKS(state.On_Item_Built, callbacks, arena);
#define INITIALIZE_OBSERVER_WITH_CALLBACKS(observer, callbacks, arena)                 \
    {                                                                                  \
        (observer).count = sizeof(callbacks) / sizeof(callbacks[0]);                   \
        (observer).functions                                                           \
            = (decltype((observer).functions))(Allocate_((arena), sizeof(callbacks))); \
        memcpy((observer).functions, callbacks, sizeof(callbacks));                    \
    }

#define On_Item_Built__Function(name_) \
    void name_(Game_State& state, v2i16 pos, const Item_To_Build& item)

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

    Scriptable_Building* scriptable_building_city_hall;
    Scriptable_Building* scriptable_building_lumberjacks_hut;

    Arena arena;
    Arena non_persistent_arena;  // Gets flushed on DLL reloads
    Arena trash_arena;           // Use for transient calculations

    Pages pages;

#ifdef BF_CLIENT
    Game_Renderer_State* renderer_state;
#endif  // BF_CLIENT

    Observer<On_Item_Built__Function((*))> On_Item_Built;
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

#ifdef BF_CLIENT
// ============================================================= //
//                    CLIENT. Game Rendering                     //
// ============================================================= //
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
    BF_Texture_ID    texture_id;
    Tile_State_Check states[8];
};

struct Smart_Tile {
    Tile_ID       id;
    BF_Texture_ID fallback_texture_id;

    int        rules_count;
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
    Loaded_Texture   buildables_panel_background;
    Loaded_Texture   buildables_placeholder_background;

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

struct Game_Renderer_State : public Non_Copyable {
    Game_UI_State* ui_state;
    Game_Bitmap*   bitmap;

    Smart_Tile grass_smart_tile;
    Smart_Tile forest_smart_tile;
    Tile_ID    forest_top_tile_id;

    Loaded_Texture human_texture;
    Loaded_Texture grass_textures[17];
    Loaded_Texture forest_textures[3];
    Loaded_Texture road_textures[16];
    Loaded_Texture flag_textures[4];

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

    bool  shaders_compilation_failed;
    GLint ui_shader_program;
};

// struct Texture_Meta_Info {
//     const char*   filename;
//     BF_Texture_ID key;
//
//     u32 width;
//     u32 height;
// };
//
// global_var HashMap<BF_Texture_ID, Texture_Meta_Info> texture_id_2_texture_meta_info;
//
// struct Texture_2_Atlas_Binding {
//     BF_Texture_ID texture_key;
//     BF_Atlas_ID   atlas_key;
// };
//
// struct Atlas {
//     Vector<BF_Texture_ID> textures;
// };
#endif  // BF_CLIENT
