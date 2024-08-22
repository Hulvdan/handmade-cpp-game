//----------------------------------------------------------------------------------
// Forward Declarations.
//----------------------------------------------------------------------------------
//
// SHIT: Oh, god, I hate this shit
struct Game;
struct Human;
struct Human_Data;
struct Building;

#if BF_CLIENT
struct Renderer;
struct Loaded_Texture;
#endif

using Entity_ID                   = u32;
const Entity_ID Entity_ID_Missing = 1 << 31;

constexpr Entity_ID Component_Mask(Entity_ID component_number) {
    return component_number << 22;
}

using Human_ID                  = Entity_ID;
const Human_ID Human_ID_Missing = Entity_ID_Missing;

using Human_Constructor_ID                              = Human_ID;
const Human_Constructor_ID Human_Constructor_ID_Missing = Entity_ID_Missing;

using Building_ID                     = Entity_ID;
const Building_ID Building_ID_Missing = Entity_ID_Missing;

using Texture_ID                        = u32;
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

//----------------------------------------------------------------------------------
// Game Logic.
//----------------------------------------------------------------------------------
enum class Direction {
    Right = 0,
    Up    = 1,
    Left  = 2,
    Down  = 3,
};

Direction v2i16_To_Direction(v2i16 value) {
    if (value == v2i16_right)
        return Direction::Right;
    if (value == v2i16_up)
        return Direction::Up;
    if (value == v2i16_left)
        return Direction::Left;
    if (value == v2i16_down)
        return Direction::Down;

    INVALID_PATH;
    return Direction::Right;
}

// NOLINTBEGIN(bugprone-macro-parentheses)
#define FOR_DIRECTION(var_name)                             \
    for (auto var_name = (Direction)0; (int)(var_name) < 4; \
         var_name      = (Direction)((int)(var_name) + 1))
// NOLINTEND(bugprone-macro-parentheses)

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

struct Calculated_Graph_Data {
    i16* dist = {};
    i16* prev = {};

    std::unordered_map<u16, v2i16> node_index_2_pos = {};
    std::unordered_map<v2i16, u16> pos_2_node_index = {};

    v2i16 center = {};
};

struct Graph {
    u16 non_zero_nodes_count = {};

    // 0  0   0000
    // 1  1   0001 R
    // 2  2   0010 U
    // 3  3   0011 RU
    // 4  4   0100 L
    // 5  5   0101 RL
    // 6  6   0110 LU
    // 7  7   0111 RUL
    // 8  8   1000 D
    // 9  9   1001 RD
    // a  10  1010 UD
    // b  11  1011 RUD
    // c  12  1100 LD
    // d  13  1101 RLD
    // e  14  1110 ULD
    // f  15  1111 RULD
    u8* nodes = {};  // 0b0000DLUR

    v2i16 size   = {};
    v2i16 offset = {};

    Calculated_Graph_Data* data = {};
};

wchar_t Graph_Node_To_String(u8 node) {
    Assert(node < 16);
    //
    //         0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F
    // U+250x  ─  ━  │  ┃  ┄  ┅  ┆  ┇  ┈  ┉  ┊  ┋  ┌  ┍  ┎  ┏
    // U+251x  ┐  ┑  ┒  ┓  └  ┕  ┖  ┗  ┘  ┙  ┚  ┛  ├  ┝  ┞  ┟
    // U+252x  ┠  ┡  ┢  ┣  ┤  ┥  ┦  ┧  ┨  ┩  ┪  ┫  ┬  ┭  ┮  ┯
    // U+253x  ┰  ┱  ┲  ┳  ┴  ┵  ┶  ┷  ┸  ┹  ┺  ┻  ┼  ┽  ┾  ┿
    // U+254x  ╀  ╁  ╂  ╃  ╄  ╅  ╆  ╇  ╈  ╉  ╊  ╋  ╌  ╍  ╎  ╏
    // U+255x  ═  ║  ╒  ╓  ╔  ╕  ╖  ╗  ╘  ╙  ╚  ╛  ╜  ╝  ╞  ╟
    // U+256x  ╠  ╡  ╢  ╣  ╤  ╥  ╦  ╧  ╨  ╩  ╪  ╫  ╬  ╭  ╮  ╯
    // U+257x  ╰  ╱  ╲  ╳  ╴  ╵  ╶  ╷  ╸  ╹  ╺  ╻  ╼  ╽  ╾  ╿
    //
    wchar_t borders[] = {
        L' ',       // 0
        L'\u257A',  // 1
        L'\u2579',  // 2
        L'\u2517',  // 3
        L'\u2578',  // 4
        L'\u2501',  // 5
        L'\u251B',  // 6
        L'\u253B',  // 7
        L'\u257B',  // 8
        L'\u250F',  // 9
        L'\u2503',  // 10
        L'\u2523',  // 11
        L'\u2513',  // 12
        L'\u2533',  // 13
        L'\u252B',  // 14
        L'\u254B',  // 15
    };
    auto result = borders[node];
    return result;
}

const wchar_t* Graph_To_String(Graph& graph, Arena& arena) {
    auto result = Allocate_Zeros_Array(
        arena, wchar_t, sizeof(wchar_t) * (graph.size.x + 1) * graph.size.y + 1
    );

    FOR_RANGE (int, y_, graph.size.y) {
        const auto y = graph.size.y - y_ - 1;

        auto r = result + y_ * graph.size.x + y_;

        FOR_RANGE (int, x, graph.size.x) {
            u8 node = graph.nodes[y * graph.size.x + x];

            *r = Graph_Node_To_String(node);
            r++;

            if (x == graph.size.x - 1)
                *r = L'\n';
        }
    }
    return result;
}

void DEBUG_Print_Graph(Graph& graph, Arena& arena) {
    TEMP_USAGE(arena);
    auto res = Graph_To_String(graph, arena);
    ::OutputDebugStringW(res);
}

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
// using World_Resource_ID = Bucket_Locator;
// const World_Resource_ID No_World_Resource_ID(-1, -1);

using Graph_Segment_ID                          = Entity_ID;
const Graph_Segment_ID Graph_Segment_ID_Missing = Entity_ID_Missing;

using World_Resource_ID                           = Entity_ID;
const World_Resource_ID World_Resource_ID_Missing = Entity_ID_Missing;

using World_Resource_Booking_ID                                   = u16;
const World_Resource_Booking_ID World_Resource_Booking_ID_Missing = 0;

enum class World_Resource_Booking_Type {
    Construction,
    // Processing,
};

struct World_Resource_Booking {
    static const Entity_ID component_mask = Component_Mask(4);

    World_Resource_Booking_Type type        = {};
    Building_ID                 building_id = {};
};

struct World_Resource {
    static const Entity_ID component_mask = Component_Mask(3);

    Scriptable_Resource* scriptable = {};

    v2i16 pos = {};

    World_Resource_Booking_ID booking_id = {};

    Vector<Graph_Segment_ID> transportation_segment_ids = {};
    Vector<v2i16>            transportation_vertices    = {};

    Human_ID targeted_human_id = {};  // optional
    Human_ID carrying_human_id = {};  // optional
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

    u16 vertices_count = {};
    v2i16* vertices = {};  // NOTE: Вершинные клетки графа (флаги, здания)

    Graph graph = {};

    Human_ID                  assigned_human_id   = {};  // optional
    Vector<Graph_Segment_ID>  linked_segment_ids  = {};
    Vector<World_Resource_ID> linked_resource_ids = {};

    Queue<World_Resource_ID> resource_ids_to_transport = {};
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
        graph.non_zero_nodes_count += 1;
    else if (node_is_not_zero_but_will_be)
        graph.non_zero_nodes_count -= 1;

    node = Graph_Node_Mark(node, dir, value);
}

using Human_ID = u32;

enum class Building_Type {
    Undefined = 0,
    City_Hall,
    Harvest,
    Plant,
    Fish,
    Produce,
};

struct Scriptable_Building {
    const char*   code = {};
    Building_Type type = {};

    Scriptable_Resource* harvestable_resource = {};

    f32 human_spawning_delay = {};
    f32 construction_points  = {};

    bool can_be_built = {};

    Vector<std::tuple<Scriptable_Resource*, i16>> construction_resources = {};

#if BF_CLIENT
    Texture_ID texture_id = {};
#endif
};

enum class Human_Type {
    Transporter,
    Constructor,
    Employee,
};

struct Human_Moving_Component {
    v2i16 pos      = {};
    f32   elapsed  = {};
    f32   progress = {};
    v2f   from     = {};

    std::optional<v2i16> to   = {};
    Queue<v2i16>         path = {};
};

enum class Moving_In_The_World_State {
    None = 0,
    Moving_To_The_City_Hall,
    Moving_To_Destination,
};

enum class Moving_Resources_Substate {
    None = 0,
    Moving_To_Resource,
    Picking_Up_Resource,
    Moving_Resource,
    Placing_Resource,
};

struct World_Resource_To_Book : public Equatable<World_Resource_To_Book> {
    Scriptable_Resource* scriptable  = {};
    u8                   count       = {};
    Building_ID          building_id = {};

    [[nodiscard]] bool Equal_To(const World_Resource_To_Book& other) const {
        auto result
            = (scriptable == other.scriptable  //
               && count == other.count         //
               && building_id == other.building_id);
        return result;
    }
};

struct Resource_To_Book {
    Scriptable_Resource* scriptable = {};
    u8                   count      = {};
};

enum class Terrain {
    None = 0,
    Grass,
};

struct Terrain_Tile {
    Terrain terrain = {};

    int  height   = {};  // NOTE: starts at 0
    bool is_cliff = {};

    i16 resource_amount = {};
};

// NOTE: Upon editing ensure that `Validate_Element_Tile` remains correct
enum class Element_Tile_Type {
    None     = 0,
    Road     = 1,
    Building = 2,
    Flag     = 3,
};

struct Element_Tile {
    Element_Tile_Type type        = {};
    Building_ID       building_id = {};
};

void Validate_Element_Tile(Element_Tile& tile) {
    Assert((int)tile.type >= 0);
    Assert((int)tile.type <= 3);

    if (tile.type == Element_Tile_Type::Building)
        Assert(tile.building_id != Building_ID_Missing);
    else
        Assert(tile.building_id == Building_ID_Missing);
}

struct Scriptable_Resource {
    const char* code = {};

#if BF_CLIENT
    Texture_ID texture_id       = {};
    Texture_ID small_texture_id = {};
#endif
};

// NOTE: `scriptable` is `null` when `amount` = 0
struct Terrain_Resource {
    Scriptable_Resource* scriptable = {};

    u8 amount = {};
};

#define Item_To_Build_Table \
    X(None, "None")         \
    X(Road, "Road")         \
    X(Flag, "Flag")         \
    X(Building, "Building")

enum class Item_To_Build_Type {
#define X(name, name_string) name,
    Item_To_Build_Table
#undef X
};

static const char* Item_To_Build_Type_Names[] = {
#define X(name, name_string) name_string,
    Item_To_Build_Table
#undef X
};

struct Item_To_Build {
    Item_To_Build_Type   type                = {};
    Scriptable_Building* scriptable_building = {};
};

#define Item_To_Build_Road Item_To_Build(Item_To_Build_Type::Road, nullptr)
#define Item_To_Build_Flag Item_To_Build(Item_To_Build_Type::Flag, nullptr)

enum class Human_Removal_Reason {
    Transporter_Returned_To_City_Hall,
    Employee_Reached_Building,
};

#define NOLINT_UNUSED_PARAMETERS(code)        \
    /* NOLINTBEGIN(misc-unused-parameters) */ \
    code /* NOLINTEND(misc-unused-parameters) */

#define HumanState_OnEnter_function(name_)                                         \
    NOLINT_UNUSED_PARAMETERS(                                                      \
        void name_(Human_State& state, Human& human, const Human_Data& data, MCTX) \
    )

#define HumanState_OnExit_function(name_)                                          \
    NOLINT_UNUSED_PARAMETERS(                                                      \
        void name_(Human_State& state, Human& human, const Human_Data& data, MCTX) \
    )

#define HumanState_Update_function(name_)                                      \
    NOLINT_UNUSED_PARAMETERS(void name_(                                       \
        Human_State& state, Human& human, const Human_Data& data, f32 dt, MCTX \
    ))

#define HumanState_OnCurrentSegmentChanged_function(name_)                         \
    NOLINT_UNUSED_PARAMETERS(                                                      \
        void name_(Human_State& state, Human& human, const Human_Data& data, MCTX) \
    )

#define HumanState_OnMovedToTheNextTile_function(name_)                            \
    NOLINT_UNUSED_PARAMETERS(                                                      \
        void name_(Human_State& state, Human& human, const Human_Data& data, MCTX) \
    )

#define HumanState_UpdateStates_function(name_) \
    NOLINT_UNUSED_PARAMETERS(void name_(        \
        Human_State&      state,                \
        Human&            human,                \
        const Human_Data& data,                 \
        Building_ID       old_building_id,      \
        Building*         old_building,         \
        MCTX                                    \
    ))

#define Human_States_Table \
    X(MovingInTheWorld)    \
    X(MovingInsideSegment) \
    X(MovingResources)     \
    X(Construction)        \
    X(Employee)

enum class Human_States {
    None = -1,
#define X(state_name) state_name,
    Human_States_Table
#undef X
        COUNT
};

struct Human_State {
    HumanState_OnEnter_function((*OnEnter))                                 = {};
    HumanState_OnExit_function((*OnExit))                                   = {};
    HumanState_Update_function((*Update))                                   = {};
    HumanState_OnCurrentSegmentChanged_function((*OnCurrentSegmentChanged)) = {};
    HumanState_OnMovedToTheNextTile_function((*OnMovedToTheNextTile))       = {};
    HumanState_UpdateStates_function((*UpdateStates))                       = {};
};

global_var Human_State human_states[(int)Human_States::COUNT] = {};

struct Human {
    static const Entity_ID component_mask = Component_Mask(1);

    Human_Moving_Component moving = {};

    Human_Type type = {};

    Human_States              state                     = {};
    Moving_In_The_World_State state_moving_in_the_world = {};
    Moving_Resources_Substate substate_moving_resources = {};

    Graph_Segment_ID  segment_id  = {};
    Building_ID       building_id = {};
    World_Resource_ID resource_id = {};

    f64 action_started_at = {};
    f32 action_progress   = {};
};

struct Building {
    static const Entity_ID component_mask = Component_Mask(2);

    v2i16                pos        = {};
    Scriptable_Building* scriptable = {};

    Human_Constructor_ID constructor_id                = {};  // optional
    f32                  remaining_construction_points = {};
};

struct City_Hall {
    f32 time_since_human_was_created = {};
};

struct World_Data {
    f32 humans_moving_one_tile_duration = {};
    f32 humans_picking_up_duration      = {};
    f32 humans_placing_duration         = {};
};

struct World {
    Entity_ID last_entity_id = {};

    v2i16             size              = {};
    Terrain_Tile*     terrain_tiles     = {};
    Terrain_Resource* terrain_resources = {};
    Element_Tile*     element_tiles     = {};

    World_Data  data       = {};
    Human_Data* human_data = {};

    Sparse_Array<Graph_Segment_ID, Graph_Segment> segments                     = {};
    Sparse_Array<Building_ID, Building>           buildings                    = {};
    Sparse_Array_Of_Ids<Building_ID>              not_constructed_building_ids = {};
    Sparse_Array<Building_ID, City_Hall>          city_halls                   = {};
    Sparse_Array<Human_ID, Human>                 humans                       = {};
    Sparse_Array_Of_Ids<Human_ID>                 human_ids_going_to_city_hall = {};
    // Sparse_Array<Human_ID, Human_Transporter>           transporters = {};
    // Sparse_Array<Human_ID, Human_Constructor>           constructors = {};
    Sparse_Array<Human_ID, Human>                humans_to_add    = {};
    Sparse_Array<Human_ID, Human_Removal_Reason> humans_to_remove = {};

    Sparse_Array<World_Resource_ID, World_Resource>                 resources = {};
    Sparse_Array<World_Resource_Booking_ID, World_Resource_Booking> resource_bookings
        = {};

    Queue<Graph_Segment_ID>        segment_ids_wo_humans = {};
    Vector<World_Resource_To_Book> resources_to_book     = {};
};

#define On_Item_Built_function(name_) \
    void name_(Game& game, v2i16 pos, const Item_To_Build& item, MCTX)

#define On_Human_Created_function(name_) \
    void name_(Game& game, const Human_ID& id, Human& human, MCTX)

#define On_Human_Removed_function(name_)                                                \
    void name_(                                                                         \
        Game& game, const Human_ID& id, Human& human, Human_Removal_Reason reason, MCTX \
    )

On_Item_Built_function(On_Item_Built);
On_Human_Created_function(On_Human_Created);
On_Human_Removed_function(On_Human_Removed);

struct Editor_Data {
    bool changed = {};

    Perlin_Params terrain_perlin     = {};
    int           terrain_max_height = {};

    Perlin_Params forest_perlin     = {};
    f32           forest_threshold  = {};
    int           forest_max_amount = {};
};

Editor_Data Default_Editor_Data() {
    Editor_Data result{};

    result.terrain_perlin.octaves      = 9;
    result.terrain_perlin.scaling_bias = 2.0f;
    result.terrain_perlin.seed         = 0;
    result.terrain_max_height          = 6;

    result.forest_perlin.octaves      = 7;
    result.forest_perlin.scaling_bias = 0.38f;
    result.forest_perlin.seed         = 0;
    result.forest_threshold           = 0.54f;
    result.forest_max_amount          = 5;

    return result;
}

struct Game {
    bool hot_reloaded      = {};
    u16  dll_reloads_count = {};

    f32 offset_x = {};
    f32 offset_y = {};

    v2f   player_pos = {};
    World world      = {};

    Editor_Data editor_data = {};

    size_t               scriptable_resources_count = {};
    Scriptable_Resource* scriptable_resources       = {};
    size_t               scriptable_buildings_count = {};
    Scriptable_Building* scriptable_buildings       = {};

    Arena arena                = {};
    Arena non_persistent_arena = {};  // Gets flushed on DLL reloads
    Arena trash_arena          = {};  // Use for transient calculations

#if BF_CLIENT
    Renderer* renderer = {};
#endif

    const BFGame::Game_Library* gamelib = {};
};

struct Game_Memory {
    bool is_initialized = {};
    Game game           = {};
};

enum class Tile_Updated_Type {
    Road_Placed,
    Flag_Placed,
    Flag_Removed,
    Road_Removed,
    Building_Placed,
    Building_Removed,
};

Building* Get_Building(World& world, Building_ID id) {
    return world.buildings.Find(id);
}

#if BF_CLIENT

//----------------------------------------------------------------------------------
// CLIENT. Game Rendering.
//----------------------------------------------------------------------------------
using BF_Texture_ID = u32;

struct Loaded_Texture {
    BF_Texture_ID id   = {};
    v2u           size = {};
    u8*           base = {};
};

using Tile_ID = u32;

enum class Tile_State_Condition {
    Skip     = 0,
    Excluded = 1,
    Included = 2,
};

struct Tile_Rule {
    Texture_ID           texture_id = {};
    Tile_State_Condition states[8]  = {};
};

struct Smart_Tile {
    Tile_ID    id                  = {};
    Texture_ID fallback_texture_id = {};

    u32        rules_count = {};
    Tile_Rule* rules       = {};
};

struct Tilemap {
    Tile_ID*    tile_ids    = {};
    Texture_ID* texture_ids = {};
    v2i         size        = {};

    bool debug_rendering_enabled = {};
};

struct C_Texture {
    Texture_ID id          = {};
    v2f   pos_inside_atlas = {};  // значения x, y ограничены [0, 1)
    v2i16 size             = {};
};

struct C_Sprite {
    v2f        pos        = {};
    v2f        scale      = {};
    v2f        anchor     = {};
    f32        rotation   = {};
    Texture_ID texture_id = {};
    i8         z          = {};
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
    bool smart_stretchable  = {};
    v2i  stretch_paddings_h = {};
    v2i  stretch_paddings_v = {};
};

struct BF_Color {
    union {
        struct {
            f32 r;
            f32 g;
            f32 b;
        };
        v3f rgb;
    };
};

static constexpr BF_Color BF_Color_White   = {1, 1, 1};
static constexpr BF_Color BF_Color_Black   = {0, 0, 0};
static constexpr BF_Color BF_Color_Red     = {1, 0, 0};
static constexpr BF_Color BF_Color_Green   = {0, 1, 0};
static constexpr BF_Color BF_Color_Blue    = {0, 0, 1};
static constexpr BF_Color BF_Color_Yellow  = {1, 1, 0};
static constexpr BF_Color BF_Color_Cyan    = {0, 1, 1};
static constexpr BF_Color BF_Color_Magenta = {1, 0, 1};

struct Game_UI_State {
    UI_Sprite_Params buildables_panel_params = {};
    // Loaded_Texture   buildables_panel_background;
    // Loaded_Texture   buildables_placeholder_background;

    Texture_ID buildables_panel_texture_id       = {};
    Texture_ID buildables_placeholder_texture_id = {};

    u32            buildables_count = {};
    Item_To_Build* buildables       = {};

    i32      selected_buildable_index          = {};
    BF_Color selected_buildable_color          = {};
    BF_Color not_selected_buildable_color      = {};
    v2f      buildables_panel_sprite_anchor    = {};
    v2f      buildables_panel_container_anchor = {};
    f32      buildables_panel_in_scale         = {};
    f32      scale                             = {};

    v2i padding          = {};
    u32 placeholders     = {};
    u32 placeholders_gap = {};
};

struct Atlas {
    v2f               size     = {};
    Loaded_Texture    texture  = {};
    Vector<C_Texture> textures = {};
};

struct OpenGL_Framebuffer {
    GLuint id    = {};
    v2i    size  = {};
    GLuint color = {};  // color_texture_id
};

struct Renderer {
    Game_UI_State* ui_state = {};
    Game_Bitmap*   bitmap   = {};

    Sparse_Array<Entity_ID, C_Sprite> sprites = {};

    Smart_Tile grass_smart_tile   = {};
    Smart_Tile forest_smart_tile  = {};
    Tile_ID    forest_top_tile_id = {};
    Tile_ID    flag_tile_id       = {};

    Texture_ID human_texture_id                = {};
    Texture_ID building_in_progress_texture_id = {};
    Texture_ID forest_texture_ids[3]           = {};
    Texture_ID grass_texture_ids[17]           = {};
    Texture_ID road_texture_ids[16]            = {};
    Texture_ID flag_texture_ids[4]             = {};

    Atlas atlas = {};

    int sprites_shader_gl2w_location = {};

    int tilemap_shader_gl2w_location        = {};
    int tilemap_shader_relscreen2w_location = {};
    int tilemap_shader_resolution_location  = {};
    int tilemap_shader_color_location       = {};

    int      tilemaps_count = {};
    Tilemap* tilemaps       = {};

    u8 terrain_tilemaps_count  = {};
    u8 resources_tilemap_index = {};
    u8 element_tilemap_index   = {};

    v2i mouse_pos = {};

    bool panning       = {};
    v2f  pan_pos       = {};
    v2f  pan_offset    = {};
    v2i  pan_start_pos = {};
    f32  zoom_target   = {};
    f32  zoom          = {};

    f32 cell_size = {};

    bool   shaders_compilation_failed = {};
    GLuint sprites_shader_program     = {};
    GLuint tilemap_shader_program     = {};

    size_t rendering_indices_buffer_size = {};
    void*  rendering_indices_buffer      = {};
};
#endif
