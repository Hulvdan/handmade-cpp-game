#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "bf_game.h"

// #define Root_Allocator_Type                                                     \
//     Freelist<                                                                   \
//         Affix_Allocator<                                                        \
//             Affix_Allocator<Freeable_Malloc_Allocator, Size_Affix, Size_Affix>, \
//             Stoopid_Affix,                                                      \
//             Stoopid_Affix>,                                                     \
//         0,                                                                      \
//         32>

#define Root_Allocator_Type                                                 \
    Affix_Allocator<                                                        \
        Affix_Allocator<Freeable_Malloc_Allocator, Size_Affix, Size_Affix>, \
        Stoopid_Affix,                                                      \
        Stoopid_Affix>

// #define Root_Allocator_Type \
//     Affix_Allocator<Freeable_Malloc_Allocator, Size_Affix, Size_Affix>

// NOLINTBEGIN(bugprone-suspicious-include)
#include "bf_game.cpp"
// NOLINTEND(bugprone-suspicious-include)

// =============================================================
// Memory Setup
// =============================================================
global_var Context _ctx(0, nullptr, nullptr, nullptr, nullptr, nullptr);

#define INITIALIZE_CTX                                                          \
    Assert(root_allocator == nullptr);                                          \
    root_allocator = (Root_Allocator_Type*)malloc(sizeof(Root_Allocator_Type)); \
    std::construct_at(root_allocator);                                          \
                                                                                \
    auto ctx = &_ctx;                                                           \
                                                                                \
    defer {                                                                     \
        CTX_ALLOCATOR;                                                          \
        FREE_ALL;                                                               \
        free(root_allocator);                                                   \
        root_allocator = nullptr;                                               \
    }

global_var std::vector<u8*> virtual_allocations;
global_var std::vector<void*> heap_allocations;

void* heap_allocate(size_t n, size_t alignment) {
    void* result = _aligned_malloc(n, alignment);
    heap_allocations.push_back(result);
    return result;
}
void heap_free(void* ptr) {
    heap_allocations.erase(
        std::remove(heap_allocations.begin(), heap_allocations.end(), ptr),
        heap_allocations.end()
    );
    _aligned_free(ptr);
}

void Free_Allocations() {
    for (auto ptr : virtual_allocations)
        VirtualFree((void*)ptr, 0, MEM_RELEASE);

    virtual_allocations.clear();

    for (auto ptr : heap_allocations) {
        // NOTE: Специально не вызываю heap_free,
        // чтобы не удаляло из массива по ходу итерирования.
        _aligned_free(ptr);
    }

    heap_allocations.clear();
}

// =============================================================
// Tests
// =============================================================
TEST_CASE ("Hash32, EmptyIsCorrect") {
    CHECK(Hash32((u8*)"", 0) == EMPTY_HASH32);
}

TEST_CASE ("Hash32, TestValue") {
    CHECK(Hash32((u8*)"test", 4) == 0xafd071e5);
}

TEST_CASE ("Load_Smart_Tile_Rules, ItWorks") {
    constexpr u64 size         = 512;
    u8            output[size] = {};

    Arena arena{};
    arena.size = size;
    arena.base = output;

    auto rules_data
        = "grass_7\ngrass_1\n| * |\n|*@@|\n| @ |\ngrass_2\n| * |\n|@@@|\n| @ |";
    i32  rules_data_size = 0;
    auto p               = rules_data;
    while (*p++)
        rules_data_size++;

    Smart_Tile tile{};
    auto result = Load_Smart_Tile_Rules(tile, arena, (u8*)rules_data, rules_data_size);

    CHECK(result.success == true);
    CHECK(tile.rules_count == 2);
    CHECK(Hash32((u8*)"test", 4) == 2949673445);
}

TEST_CASE ("Load_Smart_Tile_Rules, ItWorksWithANewlineOnTheEnd") {
    constexpr u64 size         = 512;
    u8            output[size] = {};

    Arena arena{};
    arena.size = size;
    arena.base = output;

    auto rules_data
        = "grass_7\ngrass_1\n| * |\n|*@@|\n| @ |\ngrass_2\n| * |\n|@@@|\n| @ |\n";
    i32  rules_data_size = 0;
    auto p               = rules_data;
    while (*p++)
        rules_data_size++;

    Smart_Tile tile{};
    auto result = Load_Smart_Tile_Rules(tile, arena, (u8*)rules_data, rules_data_size);

    CHECK(result.success == true);
    CHECK(tile.rules_count == 2);
    CHECK(Hash32((u8*)"test", 4) == 2949673445);
}

TEST_CASE ("ProtoTest, Proto") {
    CHECK(0xFF == 255);
    CHECK(0x00FF == 255);
    CHECK(0xFF00 == 65280);

    CHECK(0b11111111 == 255);
    CHECK(0b0000000011111111 == 255);
    CHECK(0b1111111100000000 == 65280);
}

TEST_CASE ("frand, interval") {
    for (int i = 0; i < 10000; i++) {
        auto value = frand();
        CHECK(value >= 0);
        CHECK(value < 1);
    }
}

TEST_CASE ("Move_Towards") {
    CHECK(Move_Towards(0, 1, 0.4f) == 0.4f);
    CHECK(Move_Towards(0, -1, 0.4f) == -0.4f);
    CHECK(Move_Towards(1, 1, 0.4f) == 1);
    CHECK(Move_Towards(-1, -1, 0.4f) == -1);
}

TEST_CASE ("Ceil_To_Power_Of_2") {
    CHECK(Ceil_To_Power_Of_2(1) == 1);
    CHECK(Ceil_To_Power_Of_2(2) == 2);
    CHECK(Ceil_To_Power_Of_2(3) == 4);
    CHECK(Ceil_To_Power_Of_2(4) == 4);
    CHECK(Ceil_To_Power_Of_2(31) == 32);
    CHECK(Ceil_To_Power_Of_2(32) == 32);
    CHECK(Ceil_To_Power_Of_2(65535) == 65536);
    CHECK(Ceil_To_Power_Of_2(65536) == 65536);
    CHECK(Ceil_To_Power_Of_2(2147483647) == 2147483648);
    CHECK(Ceil_To_Power_Of_2(2147483648) == 2147483648);
}

struct Test_Node {
    size_t next;
    u32    id;
    bool   active;

    Test_Node()
        : id(0)
        , next(0)
        , active(false) {}
    Test_Node(u32 a_id)
        : id(a_id)
        , next(0)
        , active(false) {}
};

#define Allocator_Allocate_Macro(allocator_, size_, alignment_) \
    STATEMENT({ (allocator_).Allocate((size_), (alignment_)); })

#define Allocator_Free_Macro(allocator_, key_) STATEMENT({ (allocator_).Free((key_)); })

TEST_CASE ("Align_Forward") {
    CHECK(Align_Forward(nullptr, 8) == nullptr);
    CHECK(Align_Forward((u8*)(2UL), 16) == (u8*)16UL);
    CHECK(Align_Forward((u8*)(4UL), 16) == (u8*)16UL);
    CHECK(Align_Forward((u8*)(4UL), 8) == (u8*)8UL);
    CHECK(Align_Forward((u8*)(8UL), 8) == (u8*)8UL);
}

TEST_CASE ("Opposite") {
    CHECK(Opposite(Direction::Right) == Direction::Left);
    CHECK(Opposite(Direction::Up) == Direction::Down);
    CHECK(Opposite(Direction::Left) == Direction::Right);
    CHECK(Opposite(Direction::Down) == Direction::Up);
}

TEST_CASE ("Opposite") {
    CHECK(Opposite(Direction::Right) == Direction::Left);
    CHECK(Opposite(Direction::Up) == Direction::Down);
    CHECK(Opposite(Direction::Left) == Direction::Right);
    CHECK(Opposite(Direction::Down) == Direction::Up);
}

TEST_CASE ("As_Offset") {
    CHECK(As_Offset(Direction::Right) == v2i16(1, 0));
    CHECK(As_Offset(Direction::Up) == v2i16(0, 1));
    CHECK(As_Offset(Direction::Left) == v2i16(-1, 0));
    CHECK(As_Offset(Direction::Down) == v2i16(0, -1));
}

global_var i32 last_entity_id = 0;

std::tuple<Building_ID, Building*> Global_Make_Building(
    Element_Tile*& element_tiles,
    Arena&         trash_arena,
    Building_Type  type,
    v2i            pos
) {
    auto sb              = Allocate_Zeros_For(trash_arena, Scriptable_Building);
    sb->type             = type;
    auto building        = Allocate_Zeros_For(trash_arena, Building);
    building->scriptable = sb;
    last_entity_id++;
    return {(Building_ID)last_entity_id, building};
}

int Process_Segments(
    Entity_ID&                                      last_entity_id,
    v2i&                                            gsize,
    Element_Tile*&                                  element_tiles,
    Sparse_Array<Graph_Segment_ID, Graph_Segment>*& segments,
    Arena&                                          trash_arena,
    Building_ID&                                    building_sawmill_id,
    Building*&                                      building_sawmill,
    std::vector<const char*>&                       strings,
    MCTX
) {
    CTX_ALLOCATOR;

    // NOTE: Creating `element_tiles`
    gsize.y = strings.size();
    gsize.x = strlen(strings[0]);

    for (auto string_ptr : strings)
        REQUIRE(strlen(string_ptr) == gsize.x);

    auto tiles_count = gsize.x * gsize.y;
    element_tiles    = Allocate_Zeros_Array(trash_arena, Element_Tile, tiles_count);

    {
        using segments_type
            = std::remove_pointer<std::remove_reference<decltype(segments)>::type>::type;
        segments = Allocate_For(trash_arena, segments_type);
        std::construct_at(segments, 32, ctx);
    }

    auto tiles         = Allocate_Zeros_Array(trash_arena, Element_Tile, tiles_count);
    auto Make_Building = [&](Building_Type type, v2i pos) {
        return Global_Make_Building(element_tiles, trash_arena, type, pos);
    };

    FOR_RANGE (int, y, gsize.y) {
        FOR_RANGE (int, x, gsize.x) {
            auto& tile = *(element_tiles + y * gsize.x + x);
            v2i   pos  = {x, y};

            const char symbol = *(strings[gsize.y - y - 1] + x);
            switch (symbol) {
            case 'C': {
                auto [building_id, _] = Make_Building(Building_Type::City_Hall, pos);
                tile.type             = Element_Tile_Type::Building;
                tile.building_id      = building_id;
            } break;

            case 'B': {
                auto [building_id, _] = Make_Building(Building_Type::Produce, pos);
                tile.type             = Element_Tile_Type::Building;
                tile.building_id      = building_id;
            } break;

            case 'S': {
                if (building_sawmill == nullptr) {
                    auto [a, b]         = Make_Building(Building_Type::Produce, pos);
                    building_sawmill_id = a;
                    building_sawmill    = b;
                }

                tile.type        = Element_Tile_Type::Building;
                tile.building_id = building_sawmill_id;
            } break;

            case 'r': {
                tile.type        = Element_Tile_Type::Road;
                tile.building_id = Building_ID_Missing;
            } break;

            case 'F': {
                tile.type        = Element_Tile_Type::Flag;
                tile.building_id = Building_ID_Missing;
            } break;

            case '.': {
                tile.type        = Element_Tile_Type::None;
                tile.building_id = Building_ID_Missing;
            } break;

            default:
                INVALID_PATH;
            }
        }
    }

    // NOTE: Counting segments.
    Build_Graph_Segments(
        last_entity_id,
        gsize,
        element_tiles,
        *segments,
        trash_arena,
        [](Graph_Segments_To_Add&, Graph_Segments_To_Delete&, Context*) {},
        ctx
    );

    int segments_count = 0;
    for (auto _ : Iter(segments))
        segments_count++;

    SANITIZE;

    return segments_count;
};

#define Process_Segments_Macro(...)                       \
    std::vector<const char*> _strings = {__VA_ARGS__};    \
    auto(segments_count)              = Process_Segments( \
        last_entity_id,                      \
        gsize,                               \
        element_tiles,                       \
        segments,                            \
        trash_arena,                         \
        building_sawmill_id,                 \
        building_sawmill,                    \
        _strings,                            \
        ctx                                  \
    );

#define Update_Tiles_Macro(updated_tiles)                                       \
    REQUIRE(segments != nullptr);                                               \
    auto [added_segments_count, removed_segments_count] = Update_Tiles(         \
        gsize,                                                                  \
        element_tiles,                                                          \
        segments,                                                               \
        trash_arena,                                                            \
        (updated_tiles),                                                        \
        [&segments, &trash_arena, &last_entity_id](                             \
            Graph_Segments_To_Add&    segments_to_add,                          \
            Graph_Segments_To_Delete& segments_to_delete,                       \
            MCTX                                                                \
        ) {                                                                     \
            CTX_ALLOCATOR;                                                      \
                                                                                \
            FOR_RANGE (u32, i, segments_to_delete.count) {                      \
                auto [id, segment_ptr] = segments_to_delete.items[i];           \
                auto& segment          = *segment_ptr;                          \
                                                                                \
                FREE(segment.vertices, sizeof(v2i16) * segment.vertices_count); \
                FREE(                                                           \
                    segment.graph.nodes,                                        \
                    sizeof(u8) * segment.graph.nodes_allocation_count           \
                );                                                              \
                segments->Unstable_Remove(id);                                  \
            }                                                                   \
                                                                                \
            FOR_RANGE (u32, i, segments_to_add.count) {                         \
                Add_And_Link_Segment(                                           \
                    last_entity_id,                                             \
                    *segments,                                                  \
                    segments_to_add.items[i],                                   \
                    trash_arena,                                                \
                    ctx                                                         \
                );                                                              \
            }                                                                   \
                                                                                \
            SANITIZE;                                                           \
        },                                                                      \
        ctx                                                                     \
    );

#define Test_Declare_Updated_Tiles(...)                                            \
    Updated_Tiles updated_tiles{};                                                 \
    {                                                                              \
        std::vector<std::tuple<v2i, Tile_Updated_Type>> data = {__VA_ARGS__};      \
        updated_tiles.pos = Allocate_Zeros_Array(trash_arena, v2i16, data.size()); \
        updated_tiles.type                                                         \
            = Allocate_Zeros_Array(trash_arena, Tile_Updated_Type, data.size());   \
        FOR_RANGE (int, i, data.size()) {                                          \
            auto& [pos, type]         = data[i];                                   \
            *(updated_tiles.pos + i)  = pos;                                       \
            *(updated_tiles.type + i) = type;                                      \
        }                                                                          \
        updated_tiles.count = data.size();                                         \
    }

TEST_CASE ("Bit operations") {
    {
        std::vector<std::tuple<u8, u8, u8>> marks = {
            {0b00000000, 0, 0b00000001},
            {0b00000000, 1, 0b00000010},
            {0b00000000, 2, 0b00000100},
            {0b00000000, 7, 0b10000000},
        };
        for (auto& [initial_value, index, after_value] : marks) {
            u8  byte  = initial_value;
            u8* bytes = &byte;
            MARK_BIT(bytes, index);
            CHECK(byte == after_value);
            UNMARK_BIT(bytes, index);
            CHECK(byte == initial_value);
        }
    }

    {
        std::vector<std::tuple<u8, u8, u8>> unmarks = {
            {0b11111111, 0, 0b11111110},
            {0b11111111, 1, 0b11111101},
            {0b11111111, 2, 0b11111011},
            {0b11111111, 7, 0b01111111},
        };
        for (auto& [initial_value, index, after_value] : unmarks) {
            u8  byte  = initial_value;
            u8* bytes = &byte;
            UNMARK_BIT(bytes, index);
            CHECK(byte == after_value);
            MARK_BIT(bytes, index);
            CHECK(byte == initial_value);
        }
    }

    {
        std::vector<u8> bytes = {
            0b00000000,
            0b01000001,
        };
        u8* ptr = bytes.data();
        CHECK((bool)QUERY_BIT(ptr, 0) == false);
        CHECK((bool)QUERY_BIT(ptr, 8) == true);
        CHECK((bool)QUERY_BIT(ptr, 14) == true);
    }
}

TEST_CASE ("Update_Tiles") {
    INITIALIZE_CTX;

    SYSTEM_INFO system_info;
    GetSystemInfo(&system_info);

    Arena trash_arena{};
    auto  trash_size = Megabytes((size_t)2);
    trash_arena.size = trash_size;
    trash_arena.base = new u8[trash_size];

    auto          last_entity_id      = (Entity_ID)0;
    Building*     building_sawmill    = nullptr;
    Building_ID   building_sawmill_id = Building_ID_Missing;
    v2i           gsize               = -v2i_one;
    Element_Tile* element_tiles       = nullptr;

    Sparse_Array<Graph_Segment_ID, Graph_Segment> segments_{32, ctx};

    auto segments = &segments_;

    auto Make_Building = [&element_tiles, &trash_arena](Building_Type type, v2i pos) {
        return Global_Make_Building(element_tiles, trash_arena, type, pos);
    };

    // Agenda:
    // - B  - building
    // - C  - City Hall
    // - F  - Flag
    // - r  - Road
    // - .  - Empty (Element_Tile_Type::None)
    // - SS - Sawmill (one building that spans several tiles)
    SUBCASE("Test_2Buildings_1Road_1Segment") {
        Process_Segments_Macro(
            ".B",  //
            "Cr"   //
        );
        CHECK(segments_count == 1);
    }

    SUBCASE("Test_1Building_1Road_0Segments") {
        Process_Segments_Macro(
            "..",  //
            "Cr"   //
        );
        CHECK(segments_count == 0);
    }

    SUBCASE("Test_2AdjacentBuildings_0Segments") {
        Process_Segments_Macro(
            "..",  //
            "CB"   //
        );
        CHECK(segments_count == 0);
    }

    SUBCASE("Test_4AdjacentBuildings_0Segments") {
        Process_Segments_Macro(
            "BB",  //
            "CB"   //
        );
        CHECK(segments_count == 0);
    }

    SUBCASE("Test_2Buildings_1Flag_0Segments") {
        Process_Segments_Macro(
            ".B",  //
            "CF"   //
        );
        CHECK(segments_count == 0);
    }

    SUBCASE("Test_2Buildings_1Flag_1Road_1Segment") {
        Process_Segments_Macro(
            "FB",  //
            "Cr"   //
        );
        CHECK(segments_count == 1);
    }

    SUBCASE("Test_2Buildings_2Flags_0Segments") {
        Process_Segments_Macro(
            "FF",  //
            "CB"   //
        );
        CHECK(segments_count == 0);
    }

    SUBCASE("Test_2Buildings_2Flags_0Segments") {
        Process_Segments_Macro(
            "FB",  //
            "CF"   //
        );
        CHECK(segments_count == 0);
    }

    SUBCASE("Test_Line_CrrFrB") {
        Process_Segments_Macro("CrrFrB");
        CHECK(segments_count == 2);
    }

    SUBCASE("Test_Line_CrrrrB") {
        Process_Segments_Macro("CrrrrB");
        CHECK(segments_count == 1);
    }

    SUBCASE("Test_Complex3") {
        Process_Segments_Macro(
            "rrrrrr",  //
            "CrrrrB"   //
        );
        CHECK(segments_count == 1);
    }

    SUBCASE("Test_Complex4") {
        Process_Segments_Macro(
            "...B..",  //
            "..rrr.",  //
            "BrrCrB",  //
            "...r..",  //
            "...B.."   //
        );
        CHECK(segments_count == 2);
    }

    SUBCASE("Test_Complex5") {
        Process_Segments_Macro(
            "...B..",  //
            "..rFr.",  //
            "BrrCrB",  //
            "...r..",  //
            "...B.."   //
        );
        CHECK(segments_count == 3);
    }

    SUBCASE("Test_Complex6") {
        Process_Segments_Macro(
            "...B..",  //
            "..rrr.",  //
            "CrrFrB"   //
        );
        CHECK(segments_count == 1);
    }

    SUBCASE("Test_Complex7") {
        Process_Segments_Macro(
            "...B..",  //
            "..rrr.",  //
            "CrrFrB",  //
            "...r..",  //
            "...B.."   //
        );
        CHECK(segments_count == 2);
    }

    SUBCASE("Test_Complex8") {
        Process_Segments_Macro(
            "...B..",  //
            "...r..",  //
            "BrrCrB",  //
            "...r..",  //
            "...B.."   //
        );
        CHECK(segments_count == 4);
    }

    SUBCASE("Test_Complex9") {
        Process_Segments_Macro(
            "...B..",  //
            "..rrr.",  //
            "BrrCrB",  //
            "..rrr.",  //
            "...B.."   //
        );
        CHECK(segments_count == 1);
    }

    SUBCASE("Test_Complex10") {
        Process_Segments_Macro(
            "...B..",  //
            "..rrr.",  //
            "BrrCrB",  //
            "..rr..",  //
            "...B.."   //
        );
        CHECK(segments_count == 1);
    }

    SUBCASE("Test_Complex11") {
        Process_Segments_Macro(
            "...B...",  //
            "..rrrr.",  //
            "CrrSSrB",  //
            "....rr."   //
        );
        CHECK(segments_count == 1);
    }

    SUBCASE("Test_Complex12") {
        Process_Segments_Macro(
            "...B...",  //
            "..rFrr.",  //
            "CrrSSrB",  //
            "....rr."   //
        );
        CHECK(segments_count == 2);
    }

    SUBCASE("Test_Complex13") {
        Process_Segments_Macro(
            "...B...",  //
            "..rFFr.",  //
            "CrrSSrB",  //
            "....rr."   //
        );
        CHECK(segments_count == 2);
    }

    SUBCASE("Test_Complex14") {
        Process_Segments_Macro(
            "...B...",  //
            "..rrFr.",  //
            "CrrSSrB",  //
            "....rr."   //
        );
        CHECK(segments_count == 2);
    }

    SUBCASE("Test_Complex15") {
        Process_Segments_Macro(
            "CrF",  //
            ".rB"   //
        );
        CHECK(segments_count == 1);
    }

    SUBCASE("Test_Complex16") {
        Process_Segments_Macro(
            "CrFr",  //
            ".rSS"   //
        );
        CHECK(segments_count == 2);
    }

    SUBCASE("Test_Complex17") {
        Process_Segments_Macro(
            "Crrr",  //
            ".rSS"   //
        );
        CHECK(segments_count == 1);
    }

    SUBCASE("Test_Complex18") {
        Process_Segments_Macro(
            ".B.",  //
            "CFB",  //
            ".B."   //
        );
        CHECK(segments_count == 0);
    }

    SUBCASE("Test_Complex19") {
        Process_Segments_Macro(
            ".B.",  //
            "CrB",  //
            ".B."   //
        );
        CHECK(segments_count == 1);
    }

    SUBCASE("Test_Complex20") {
        Process_Segments_Macro(
            "..B..",  //
            "..r..",  //
            "CrFrB",  //
            "..r..",  //
            "..B.."   //
        );
        CHECK(segments_count == 4);
    }

    SUBCASE("Test_Line_CrB") {
        Process_Segments_Macro("CrB");
        CHECK(segments_count == 1);
    }

    SUBCASE("Test_Parallel_Lines") {
        Process_Segments_Macro(
            "B.B",  //
            "r.r",  //
            "B.B"   //
        );
        CHECK(segments_count == 2);
    }

    SUBCASE("Test_Horizontal_Lines") {
        Process_Segments_Macro(
            "BrB",  //
            "...",  //
            "BrB"   //
        );
        CHECK(segments_count == 2);
    }

    SUBCASE("Test_Line_CFB") {
        Process_Segments_Macro("CFB");
        CHECK(segments_count == 0);
    }
    SUBCASE("Test_Line_BFCFB") {
        Process_Segments_Macro("BFCFB");
        CHECK(segments_count == 0);
    }

    SUBCASE("Test_Line_BrCrB") {
        Process_Segments_Macro("BrCrB");
        CHECK(segments_count == 2);
    }

    SUBCASE("Test_Line_BrCrB") {
        Process_Segments_Macro("BrCrB");
        CHECK(segments_count == 2);
    }

    // OTHER TEST CASES

    SUBCASE("Test_RoadPlaced_1") {
        CTX_ALLOCATOR;
        Process_Segments_Macro(
            ".B",  //
            ".F",  //
            "Cr"
        );
        CHECK(segments_count == 1);

        auto pos = v2i(0, 1);
        CHECK(GRID_PTR_VALUE(element_tiles, pos).type == Element_Tile_Type::None);
        GRID_PTR_VALUE(element_tiles, pos).type = Element_Tile_Type::Road;

        Test_Declare_Updated_Tiles({pos, Tile_Updated_Type::Road_Placed});

        Update_Tiles_Macro(updated_tiles);

        CHECK(added_segments_count == 1);
        CHECK(removed_segments_count == 0);
    }

    SUBCASE("Test_RoadPlaced_2") {
        Process_Segments_Macro(
            ".B",  //
            "Cr"
        );
        CHECK(segments_count == 1);

        auto pos = v2i(0, 1);
        CHECK(GRID_PTR_VALUE(element_tiles, pos).type == Element_Tile_Type::None);
        GRID_PTR_VALUE(element_tiles, pos).type = Element_Tile_Type::Road;

        Test_Declare_Updated_Tiles({pos, Tile_Updated_Type::Road_Placed});
        Update_Tiles_Macro(updated_tiles);

        CHECK(added_segments_count == 1);
        CHECK(removed_segments_count == 0);
    }

    SUBCASE("Test_RoadPlaced_3") {
        Process_Segments_Macro(
            ".B",  //
            "CF"
        );

        auto pos = v2i(0, 1);
        CHECK(GRID_PTR_VALUE(element_tiles, pos).type == Element_Tile_Type::None);
        GRID_PTR_VALUE(element_tiles, pos).type = Element_Tile_Type::Road;

        Test_Declare_Updated_Tiles({pos, Tile_Updated_Type::Road_Placed});
        Update_Tiles_Macro(updated_tiles);

        CHECK(added_segments_count == 1);
        CHECK(removed_segments_count == 0);
    }

    SUBCASE("Test_RoadPlaced_4") {
        Process_Segments_Macro(
            ".rr",  //
            "Crr"
        );

        {
            auto pos                                = v2i(1, 0);
            GRID_PTR_VALUE(element_tiles, pos).type = Element_Tile_Type::Flag;

            Test_Declare_Updated_Tiles({pos, Tile_Updated_Type::Flag_Placed});
            Update_Tiles_Macro(updated_tiles);

            CHECK(added_segments_count == 0);
            CHECK(removed_segments_count == 0);
        }

        {
            auto pos                                = v2i(2, 1);
            GRID_PTR_VALUE(element_tiles, pos).type = Element_Tile_Type::Flag;

            Test_Declare_Updated_Tiles({pos, Tile_Updated_Type::Flag_Placed});
            Update_Tiles_Macro(updated_tiles);

            CHECK(added_segments_count == 2);
            CHECK(removed_segments_count == 0);
        }

        {
            auto pos                                = v2i(0, 1);
            GRID_PTR_VALUE(element_tiles, pos).type = Element_Tile_Type::Road;

            Test_Declare_Updated_Tiles({pos, Tile_Updated_Type::Road_Placed});
            Update_Tiles_Macro(updated_tiles);

            CHECK(added_segments_count == 1);
            CHECK(removed_segments_count == 1);
        }
    }

    SUBCASE("Test_FlagPlaced_1") {
        Process_Segments_Macro(
            ".B",  //
            ".r",  //
            "Cr"
        );

        auto pos                                = v2i(1, 1);
        GRID_PTR_VALUE(element_tiles, pos).type = Element_Tile_Type::Flag;

        Test_Declare_Updated_Tiles({pos, Tile_Updated_Type::Flag_Placed});
        Update_Tiles_Macro(updated_tiles);

        CHECK(added_segments_count == 1);
        CHECK(removed_segments_count == 1);
    }

    SUBCASE("Test_FlagPlaced_2") {
        Process_Segments_Macro(
            ".B",  //
            ".r",  //
            "CF"
        );
        CHECK(segments_count == 1);

        auto pos                                = v2i(1, 1);
        GRID_PTR_VALUE(element_tiles, pos).type = Element_Tile_Type::Flag;

        Test_Declare_Updated_Tiles({pos, Tile_Updated_Type::Flag_Placed});
        Update_Tiles_Macro(updated_tiles);

        CHECK(added_segments_count == 0);
        CHECK(removed_segments_count == 1);
    }

    SUBCASE("Test_FlagPlaced_3") {
        Process_Segments_Macro(
            ".B",  //
            "CF"
        );

        auto pos                                = v2i(0, 1);
        GRID_PTR_VALUE(element_tiles, pos).type = Element_Tile_Type::Flag;

        Test_Declare_Updated_Tiles({pos, Tile_Updated_Type::Flag_Placed});
        Update_Tiles_Macro(updated_tiles);

        CHECK(added_segments_count == 0);
        CHECK(removed_segments_count == 0);
    }

    SUBCASE("Test_FlagPlaced_4") {
        Process_Segments_Macro("CrrrB");
        CHECK(segments_count == 1);

        auto pos                                = v2i(2, 0);
        GRID_PTR_VALUE(element_tiles, pos).type = Element_Tile_Type::Flag;

        Test_Declare_Updated_Tiles({pos, Tile_Updated_Type::Flag_Placed});
        Update_Tiles_Macro(updated_tiles);

        CHECK(added_segments_count == 2);
        CHECK(removed_segments_count == 1);
    }

    SUBCASE("Test_BuildingPlaced_1") {
        Process_Segments_Macro(
            ".B",  //
            "CF"
        );

        auto pos             = v2i(0, 1);
        auto [bid, building] = Make_Building(Building_Type::Produce, pos);
        GRID_PTR_VALUE(element_tiles, pos).type        = Element_Tile_Type::Building;
        GRID_PTR_VALUE(element_tiles, pos).building_id = bid;

        Test_Declare_Updated_Tiles({pos, Tile_Updated_Type::Building_Placed});
        Update_Tiles_Macro(updated_tiles);

        CHECK(added_segments_count == 0);
        CHECK(removed_segments_count == 0);
    }

    SUBCASE("Test_BuildingPlaced_2") {
        Process_Segments_Macro(
            "..",  //
            "CF"
        );
        CHECK(segments_count == 0);

        auto pos      = v2i(1, 1);
        auto [bid, _] = Make_Building(Building_Type::Produce, pos);
        GRID_PTR_VALUE(element_tiles, pos).type        = Element_Tile_Type::Building;
        GRID_PTR_VALUE(element_tiles, pos).building_id = bid;

        Test_Declare_Updated_Tiles({pos, Tile_Updated_Type::Building_Placed});
        Update_Tiles_Macro(updated_tiles);

        CHECK(added_segments_count == 0);
        CHECK(removed_segments_count == 0);
    }

    SUBCASE("Test_BuildingPlaced_3") {
        Process_Segments_Macro(
            "..",  //
            "Cr"
        );
        CHECK(segments_count == 0);

        auto pos      = v2i(1, 1);
        auto [bid, _] = Make_Building(Building_Type::Produce, pos);
        GRID_PTR_VALUE(element_tiles, pos).type        = Element_Tile_Type::Building;
        GRID_PTR_VALUE(element_tiles, pos).building_id = bid;

        Test_Declare_Updated_Tiles({pos, Tile_Updated_Type::Building_Placed});
        Update_Tiles_Macro(updated_tiles);

        CHECK(added_segments_count == 1);
        CHECK(removed_segments_count == 0);
    }

    SUBCASE("Test_BuildingRemoved_1") {
        Process_Segments_Macro(
            ".B",  //
            "Cr"   //
        );
        CHECK(segments_count == 1);

        auto pos                                       = v2i(1, 1);
        GRID_PTR_VALUE(element_tiles, pos).type        = Element_Tile_Type::None;
        GRID_PTR_VALUE(element_tiles, pos).building_id = Building_ID_Missing;

        Test_Declare_Updated_Tiles({pos, Tile_Updated_Type::Building_Removed});
        Update_Tiles_Macro(updated_tiles);

        CHECK(added_segments_count == 0);
        CHECK(removed_segments_count == 1);
    }

    SUBCASE("Test_BuildingRemoved_2") {
        Process_Segments_Macro(
            ".B.",  //
            "CrB"   //
        );
        CHECK(segments_count == 1);

        auto pos                                       = v2i(1, 1);
        GRID_PTR_VALUE(element_tiles, pos).type        = Element_Tile_Type::None;
        GRID_PTR_VALUE(element_tiles, pos).building_id = Building_ID_Missing;

        Test_Declare_Updated_Tiles({pos, Tile_Updated_Type::Building_Removed});
        Update_Tiles_Macro(updated_tiles);

        CHECK(added_segments_count == 1);
        CHECK(removed_segments_count == 1);
    }

    SUBCASE("Shit_Test") {
        Process_Segments_Macro(
            "...",  //
            "..."
        );
        CHECK(segments_count == 0);

        {
            auto pos                                = v2i(0, 0);
            GRID_PTR_VALUE(element_tiles, pos).type = Element_Tile_Type::Road;
            Test_Declare_Updated_Tiles({pos, Tile_Updated_Type::Road_Placed});
            Update_Tiles_Macro(updated_tiles);

            CHECK(added_segments_count == 0);
            CHECK(removed_segments_count == 0);
        }

        {
            auto pos                                = v2i(1, 0);
            GRID_PTR_VALUE(element_tiles, pos).type = Element_Tile_Type::Road;
            Test_Declare_Updated_Tiles({pos, Tile_Updated_Type::Road_Placed});
            Update_Tiles_Macro(updated_tiles);

            CHECK(added_segments_count == 0);
            CHECK(removed_segments_count == 0);
        }

        {
            auto pos                                = v2i(2, 0);
            GRID_PTR_VALUE(element_tiles, pos).type = Element_Tile_Type::Road;
            Test_Declare_Updated_Tiles({pos, Tile_Updated_Type::Road_Placed});
            Update_Tiles_Macro(updated_tiles);

            CHECK(added_segments_count == 0);
            CHECK(removed_segments_count == 0);
        }

        {
            auto pos                                = v2i(0, 1);
            GRID_PTR_VALUE(element_tiles, pos).type = Element_Tile_Type::Road;
            Test_Declare_Updated_Tiles({pos, Tile_Updated_Type::Road_Placed});
            Update_Tiles_Macro(updated_tiles);

            CHECK(added_segments_count == 0);
            CHECK(removed_segments_count == 0);
        }

        {
            auto pos                                = v2i(1, 1);
            GRID_PTR_VALUE(element_tiles, pos).type = Element_Tile_Type::Road;
            Test_Declare_Updated_Tiles({pos, Tile_Updated_Type::Road_Placed});
            Update_Tiles_Macro(updated_tiles);

            CHECK(added_segments_count == 0);
            CHECK(removed_segments_count == 0);
        }

        {
            auto pos                                = v2i(0, 0);
            GRID_PTR_VALUE(element_tiles, pos).type = Element_Tile_Type::Flag;
            Test_Declare_Updated_Tiles({pos, Tile_Updated_Type::Flag_Placed});
            Update_Tiles_Macro(updated_tiles);

            CHECK(added_segments_count == 0);
            CHECK(removed_segments_count == 0);
        }

        {
            auto pos                                = v2i(1, 1);
            GRID_PTR_VALUE(element_tiles, pos).type = Element_Tile_Type::Flag;
            Test_Declare_Updated_Tiles({pos, Tile_Updated_Type::Flag_Placed});
            Update_Tiles_Macro(updated_tiles);

            CHECK(added_segments_count == 2);
            CHECK(removed_segments_count == 0);
        }

        {
            auto pos                                = v2i(2, 0);
            GRID_PTR_VALUE(element_tiles, pos).type = Element_Tile_Type::Flag;
            Test_Declare_Updated_Tiles({pos, Tile_Updated_Type::Flag_Placed});
            Update_Tiles_Macro(updated_tiles);

            CHECK(added_segments_count == 1);
            CHECK(removed_segments_count == 1);
        }

        {
            auto pos                                = v2i(1, 0);
            GRID_PTR_VALUE(element_tiles, pos).type = Element_Tile_Type::Flag;
            Test_Declare_Updated_Tiles({pos, Tile_Updated_Type::Flag_Placed});
            Update_Tiles_Macro(updated_tiles);

            CHECK(added_segments_count == 0);
            CHECK(removed_segments_count == 1);
        }
    }

    SUBCASE("Shit_Test2") {
        Process_Segments_Macro(
            "rF.",  //
            "FrF"   //
        );
        CHECK(segments_count == 2);

        {
            auto pos = v2i(1, 0);

            GRID_PTR_VALUE(element_tiles, pos).type = Element_Tile_Type::Flag;
            Test_Declare_Updated_Tiles({pos, Tile_Updated_Type::Flag_Placed});
            Update_Tiles_Macro(updated_tiles);

            CHECK(added_segments_count == 0);
            CHECK(removed_segments_count == 1);
        }
    }

    SUBCASE("Shit_Test3") {
        Process_Segments_Macro(
            ".F.",  //
            "FFF"   //
        );
        CHECK(segments_count == 0);

        {
            auto pos = v2i(1, 0);

            GRID_PTR_VALUE(element_tiles, pos).type = Element_Tile_Type::Road;
            Test_Declare_Updated_Tiles({pos, Tile_Updated_Type::Flag_Removed});
            Update_Tiles_Macro(updated_tiles);

            CHECK(added_segments_count == 1);
            CHECK(removed_segments_count == 0);
        }
    }

    delete[] trash_arena.base;
    Free_Allocations();
}

TEST_CASE ("Queue") {
    INITIALIZE_CTX;

    Queue<int> queue{};

    SUBCASE("Enqueue") {
        REQUIRE(queue.count == 0);
        *queue.Enqueue(ctx) = 10;
        REQUIRE(queue.count == 1);
        auto val = queue.Dequeue();
        CHECK(val == 10);
        REQUIRE(queue.count == 0);

        REQUIRE(queue.max_count == 8);
        *queue.Enqueue(ctx) = 1;
        REQUIRE(queue.count == 1);
        *queue.Enqueue(ctx) = 2;
        REQUIRE(queue.count == 2);
        *queue.Enqueue(ctx) = 3;
        REQUIRE(queue.count == 3);
        *queue.Enqueue(ctx) = 4;
        REQUIRE(queue.count == 4);
        *queue.Enqueue(ctx) = 5;
        REQUIRE(queue.count == 5);
        *queue.Enqueue(ctx) = 6;
        REQUIRE(queue.count == 6);
        *queue.Enqueue(ctx) = 7;
        REQUIRE(queue.count == 7);
        *queue.Enqueue(ctx) = 8;
        REQUIRE(queue.count == 8);
        REQUIRE(queue.max_count == 8);
        *queue.Enqueue(ctx) = 9;
        REQUIRE(queue.max_count == 16);
        REQUIRE(queue.count == 9);

        REQUIRE(queue.Dequeue() == 1);
        REQUIRE(queue.count == 8);
        REQUIRE(queue.Dequeue() == 2);
        REQUIRE(queue.count == 7);
        REQUIRE(queue.Dequeue() == 3);
        REQUIRE(queue.count == 6);
        REQUIRE(queue.Dequeue() == 4);
        REQUIRE(queue.count == 5);
        REQUIRE(queue.Dequeue() == 5);
        REQUIRE(queue.count == 4);
        REQUIRE(queue.Dequeue() == 6);
        REQUIRE(queue.count == 3);
        REQUIRE(queue.Dequeue() == 7);
        REQUIRE(queue.count == 2);
        REQUIRE(queue.Dequeue() == 8);
        REQUIRE(queue.count == 1);
        REQUIRE(queue.Dequeue() == 9);
        REQUIRE(queue.count == 0);
    }

    SUBCASE("Bulk_Enqueue") {
        REQUIRE(queue.count == 0);
        int            numbers_[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
        constexpr auto n          = sizeof(numbers_) / sizeof(numbers_[0]);
        int*           numbers    = numbers_;
        REQUIRE(n == 10);
        memcpy(  //
            queue.Bulk_Enqueue(10, ctx),
            numbers,
            sizeof(*numbers) * 10
        );
        REQUIRE(queue.count == 10);
        REQUIRE(queue.max_count == 16);

        REQUIRE(queue.count == 10);
        CHECK(queue.Dequeue() == 1);
        REQUIRE(queue.count == 9);
        CHECK(queue.Dequeue() == 2);
        REQUIRE(queue.count == 8);
        CHECK(queue.Dequeue() == 3);
        REQUIRE(queue.count == 7);
        CHECK(queue.Dequeue() == 4);
        REQUIRE(queue.count == 6);
        CHECK(queue.Dequeue() == 5);
        REQUIRE(queue.count == 5);
        CHECK(queue.Dequeue() == 6);
        REQUIRE(queue.count == 4);
        CHECK(queue.Dequeue() == 7);
        REQUIRE(queue.count == 3);
        CHECK(queue.Dequeue() == 8);
        REQUIRE(queue.count == 2);
        CHECK(queue.Dequeue() == 9);
        REQUIRE(queue.count == 1);
        CHECK(queue.Dequeue() == 10);
        REQUIRE(queue.count == 0);
    }

    Free_Allocations();
}

TEST_CASE ("Array functions") {
    const auto max_count = 10;
    int        arr_arr[max_count];

    i32  count = 0;
    int* arr   = arr_arr;
    Array_Push(arr, count, max_count, 5);
    CHECK(*(arr + 0) == 5);
    Array_Push(arr, count, max_count, 4);
    CHECK(*(arr + 1) == 4);
    Array_Push(arr, count, max_count, 3);
    Array_Push(arr, count, max_count, 2);
    Array_Push(arr, count, max_count, 1);

    Array_Reverse(arr, count);
    CHECK(*(arr + 0) == 1);
    CHECK(*(arr + 1) == 2);
    CHECK(*(arr + 2) == 3);
    CHECK(*(arr + 3) == 4);
    CHECK(*(arr + 4) == 5);
}

TEST_CASE ("Longest_Meaningful_Path") {
    CHECK(Longest_Meaningful_Path({1, 1}) == 1);
    CHECK(Longest_Meaningful_Path({2, 2}) == 3);
    CHECK(Longest_Meaningful_Path({3, 2}) == 5);
    CHECK(Longest_Meaningful_Path({3, 3}) == 7);
    CHECK(Longest_Meaningful_Path({4, 1}) == 4);
    CHECK(Longest_Meaningful_Path({1, 4}) == 4);
    CHECK(Longest_Meaningful_Path({4, 3}) == 9);
    CHECK(Longest_Meaningful_Path({5, 3}) == 11);
    CHECK(Longest_Meaningful_Path({5, 5}) == 17);
}
