#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include <vector>
#include <memory>

#include "bf_game.h"
// NOLINTBEGIN(bugprone-suspicious-include)
#include "bf_game.cpp"
// NOLINTEND(bugprone-suspicious-include)

TEST_CASE("Hash32, EmptyIsCorrect") {
    CHECK(Hash32((u8*)"", 0) == 2166136261);
}

TEST_CASE("Hash32, TestValue") {
    CHECK(Hash32((u8*)"test", 4) == 2949673445);
}

TEST_CASE("Load_Smart_Tile_Rules, ItWorks") {
    constexpr u64 size = 512;
    u8 output[size] = {};

    Arena arena = {};
    arena.size = size;
    arena.base = output;

    auto rules_data =
        "grass_7\ngrass_1\n| * |\n|*@@|\n| @ |\ngrass_2\n| * |\n|@@@|\n| @ |";
    i32 rules_data_size = 0;
    auto p = rules_data;
    while (*p++)
        rules_data_size++;

    Smart_Tile tile = {};
    auto result = Load_Smart_Tile_Rules(tile, arena, (u8*)rules_data, rules_data_size);

    CHECK(result.success == true);
    CHECK(tile.rules_count == 2);
    CHECK(Hash32((u8*)"test", 4) == 2949673445);
}

TEST_CASE("Load_Smart_Tile_Rules, ItWorksWithANewlineOnTheEnd") {
    constexpr u64 size = 512;
    u8 output[size] = {};

    Arena arena = {};
    arena.size = size;
    arena.base = output;

    auto rules_data =
        "grass_7\ngrass_1\n| * |\n|*@@|\n| @ |\ngrass_2\n| * |\n|@@@|\n| @ |\n";
    i32 rules_data_size = 0;
    auto p = rules_data;
    while (*p++)
        rules_data_size++;

    Smart_Tile tile = {};
    auto result = Load_Smart_Tile_Rules(tile, arena, (u8*)rules_data, rules_data_size);

    CHECK(result.success == true);
    CHECK(tile.rules_count == 2);
    CHECK(Hash32((u8*)"test", 4) == 2949673445);
}

TEST_CASE("ProtoTest, Proto") {
    CHECK(0xFF == 255);
    CHECK(0x00FF == 255);
    CHECK(0xFF00 == 65280);

    CHECK(0b11111111 == 255);
    CHECK(0b0000000011111111 == 255);
    CHECK(0b1111111100000000 == 65280);
}

TEST_CASE("frand, interval") {
    for (int i = 0; i < 10000; i++) {
        auto value = frand();
        CHECK(value >= 0);
        CHECK(value < 1);
    }
}

TEST_CASE("Move_Towards") {
    CHECK(Move_Towards(0, 1, 0.4f) == 0.4f);
    CHECK(Move_Towards(0, -1, 0.4f) == -0.4f);
    CHECK(Move_Towards(1, 1, 0.4f) == 1);
    CHECK(Move_Towards(-1, -1, 0.4f) == -1);
}

TEST_CASE("Ceil_To_Power_Of_2") {
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
    u32 id;
    bool active;

    Test_Node() : id(0), next(0), active(false) {}
    Test_Node(u32 a_id) : id(a_id), next(0), active(false) {}
};

#define Linked_List_Push_Back_Macro(nodes_, n_, first_node_index_, node_to_add_)    \
    Linked_List_Push_Back(                                                          \
        rcast<u8*>(nodes_), (n_), (first_node_index_), rcast<u8*>(&(node_to_add_)), \
        offsetof(node_to_add_, active), offsetof(node_to_add_, next),               \
        sizeof(node_to_add_));

#define Linked_List_Remove_At_Macro(                                       \
    nodes_, n_, first_node_index_, index_to_remove_, type_)                \
    Linked_List_Remove_At(                                                 \
        rcast<u8*>(nodes_), (n_), (first_node_index_), (index_to_remove_), \
        offsetof(type_, active), offsetof(type_, next), sizeof(type_));

#define Allocator_Allocate_Macro(allocator_, size_, alignment_) \
    (allocator_).Allocate((size_), (alignment_));

#define Allocator_Free_Macro(allocator_, key_) (allocator_).Free((key_));

TEST_CASE("Linked List") {
    Test_Node* nodes = new Test_Node[10]{};

    size_t count = 0;
    size_t first_node_index = 0;

    {
        auto node_to_add = Test_Node(1);
        auto index =
            Linked_List_Push_Back_Macro(nodes, count, first_node_index, node_to_add);
        CHECK(index == 0);
    }

    CHECK(count == 1);

    {
        auto& node = *(nodes + 0);
        CHECK(node.active);
        CHECK(node.next == size_t_max);
        CHECK(node.id == 1);
    }

    {
        auto node_to_add = Test_Node(2);
        auto index =
            Linked_List_Push_Back_Macro(nodes, count, first_node_index, node_to_add);
        CHECK(index == 1);
    }

    CHECK(count == 2);

    {
        auto& node = *(nodes + 0);
        CHECK(node.active);
        CHECK(node.next == 1);
        CHECK(node.id == 1);
    }
    {
        auto& node = *(nodes + 1);
        CHECK(node.active);
        CHECK(node.id == 2);
        CHECK(node.next == size_t_max);
    }

    Linked_List_Remove_At_Macro(nodes, count, first_node_index, 1, Test_Node);
    CHECK(count == 1);
    {
        auto& node = *(nodes + 0);
        CHECK(node.active);
        CHECK(node.next == size_t_max);
        CHECK(node.id == 1);
    }
    {
        auto& node = *(nodes + 1);
        CHECK(!node.active);
    }

    {
        auto node_to_add = Test_Node(3);
        auto index =
            Linked_List_Push_Back_Macro(nodes, count, first_node_index, node_to_add);
        CHECK(index == 1);
    }
    CHECK(count == 2);
    {
        auto& node = *(nodes + 0);
        CHECK(node.active);
        CHECK(node.next == 1);
        CHECK(node.id == 1);
    }
    {
        auto& node = *(nodes + 1);
        CHECK(node.active);
        CHECK(node.next == size_t_max);
        CHECK(node.id == 3);
    }

    Linked_List_Remove_At_Macro(nodes, count, first_node_index, 0, Test_Node);
    CHECK(count == 1);
    CHECK(first_node_index == 1);
    {
        auto& node = *(nodes + 0);
        CHECK(!node.active);
    }
    {
        auto& node = *(nodes + 1);
        CHECK(node.active);
        CHECK(node.next == size_t_max);
        CHECK(node.id == 3);
    }

    {
        auto node_to_add = Test_Node(4);
        auto index =
            Linked_List_Push_Back_Macro(nodes, count, first_node_index, node_to_add);
        CHECK(index == 0);
    }
    CHECK(count == 2);
    {
        auto& node = *(nodes + 0);
        CHECK(node.active);
        CHECK(node.next == size_t_max);
        CHECK(node.id == 4);
    }
    {
        auto& node = *(nodes + 1);
        CHECK(node.active);
        CHECK(node.next == 0);
        CHECK(node.id == 3);
    }

    Linked_List_Remove_At_Macro(nodes, count, first_node_index, 0, Test_Node);
    CHECK(count == 1);
    CHECK(first_node_index == 1);

    Linked_List_Remove_At_Macro(nodes, count, first_node_index, 1, Test_Node);
    CHECK(count == 0);
    CHECK(first_node_index == 0);
}

TEST_CASE("Align_Forward") {
    CHECK(Align_Forward(nullptr, 8) == nullptr);
    CHECK(Align_Forward((u8*)(2UL), 16) == (u8*)16UL);
    CHECK(Align_Forward((u8*)(4UL), 16) == (u8*)16UL);
    CHECK(Align_Forward((u8*)(4UL), 8) == (u8*)8UL);
    CHECK(Align_Forward((u8*)(8UL), 8) == (u8*)8UL);
}

TEST_CASE("Allocator") {
    u8* toc_buffer = new u8[1024];
    u8* data_buffer = new u8[1024];
    memset(toc_buffer, 0, 1024);

    auto allocator = Allocator(1024, toc_buffer, 1024, data_buffer);
    auto alloc = rcast<Allocation*>(toc_buffer);

    // Tests
    {
        auto [key, ptr] = Allocator_Allocate_Macro(allocator, 12, 1UL);
        CHECK(allocator.first_allocation_index == 0);
        CHECK(allocator.current_allocations_count == 1);
        CHECK(key == 0);
        CHECK(ptr == data_buffer);

        CHECK((alloc + 0)->next == size_t_max);
    }
    {
        auto [key, ptr] = Allocator_Allocate_Macro(allocator, 36, 1UL);
        CHECK(allocator.first_allocation_index == 0);
        CHECK(allocator.current_allocations_count == 2);
        CHECK(key == 1);
        CHECK(ptr == data_buffer + 12);

        CHECK((alloc + 0)->next == 1);
        CHECK((alloc + 1)->next == size_t_max);
    }
    {
        auto [key, ptr] = Allocator_Allocate_Macro(allocator, 12, 1UL);
        CHECK(allocator.first_allocation_index == 0);
        CHECK(allocator.current_allocations_count == 3);
        CHECK(key == 2);
        CHECK(ptr == data_buffer + 48);

        CHECK((alloc + 0)->next == 1);
        CHECK((alloc + 1)->next == 2);
        CHECK((alloc + 2)->next == size_t_max);
    }
    {
        Allocator_Free_Macro(allocator, 2UL);
        CHECK(allocator.first_allocation_index == 0);
        CHECK(allocator.current_allocations_count == 2);
        CHECK((alloc + 0)->next == 1);
        CHECK((alloc + 1)->next == size_t_max);
        CHECK((alloc + 1)->active == true);
        CHECK((alloc + 2)->active == false);
    }
    {
        auto [key, ptr] = Allocator_Allocate_Macro(allocator, 12, 1UL);
        CHECK(allocator.first_allocation_index == 0);
        CHECK(allocator.current_allocations_count == 3);
        CHECK(key == 2);
        CHECK(ptr == data_buffer + 48);
    }
    {
        Allocator_Free_Macro(allocator, 0UL);
        CHECK(allocator.first_allocation_index == 1);
        CHECK(allocator.current_allocations_count == 2);
        CHECK((alloc + 0)->active == false);
        CHECK((alloc + 1)->next == 2);
        CHECK((alloc + 1)->active == true);
        CHECK((alloc + 2)->active == true);
    }
    {
        Allocator_Free_Macro(allocator, 1UL);
        CHECK(allocator.first_allocation_index == 2);
        CHECK(allocator.current_allocations_count == 1);
        CHECK((alloc + 0)->active == false);
        CHECK((alloc + 1)->active == false);
        CHECK((alloc + 0)->active == false);
        CHECK((alloc + 1)->active == false);
        CHECK((alloc + 2)->active == true);
    }
    {
        auto [key, ptr] = Allocator_Allocate_Macro(allocator, 12, 1UL);
        CHECK(allocator.first_allocation_index == 0);
        CHECK(allocator.current_allocations_count == 2);
        CHECK(key == 0);
        CHECK(ptr == data_buffer + 0);
        CHECK((alloc + 0)->active == true);
        CHECK((alloc + 2)->active == true);
        CHECK((alloc + 0)->next == 2);
        CHECK((alloc + 2)->next == size_t_max);
    }
    {
        auto [key, ptr] = Allocator_Allocate_Macro(allocator, 12, 1UL);
        CHECK(allocator.first_allocation_index == 0);
        CHECK(allocator.current_allocations_count == 3);
        CHECK(key == 1);
        CHECK(ptr == data_buffer + 12);
        CHECK((alloc + 0)->active == true);
        CHECK((alloc + 1)->active == true);
        CHECK((alloc + 2)->active == true);
        CHECK((alloc + 0)->next == 1);
        CHECK((alloc + 1)->next == 2);
        CHECK((alloc + 2)->next == size_t_max);
    }
}

TEST_CASE("Opposite") {
    CHECK(Opposite(Direction::Right) == Direction::Left);
    CHECK(Opposite(Direction::Up) == Direction::Down);
    CHECK(Opposite(Direction::Left) == Direction::Right);
    CHECK(Opposite(Direction::Down) == Direction::Up);
}

TEST_CASE("Opposite") {
    CHECK(Opposite(Direction::Right) == Direction::Left);
    CHECK(Opposite(Direction::Up) == Direction::Down);
    CHECK(Opposite(Direction::Left) == Direction::Right);
    CHECK(Opposite(Direction::Down) == Direction::Up);
}

TEST_CASE("As_Offset") {
    CHECK(As_Offset(Direction::Right) == v2i(1, 0));
    CHECK(As_Offset(Direction::Up) == v2i(0, 1));
    CHECK(As_Offset(Direction::Left) == v2i(-1, 0));
    CHECK(As_Offset(Direction::Down) == v2i(0, -1));
}

global OS_Data os_data;
global std::vector<u8*> virtual_allocations;

Allocate_Pages__Function(Win32_Allocate_Pages) {
    Assert(count % os_data.min_pages_per_allocation == 0);
    auto size = os_data.page_size * count;
    auto result = (u8*)VirtualAlloc(
        nullptr, size, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);

    virtual_allocations.push_back(result);
    return result;
}

void Free_Allocations() {
    for (auto ptr : virtual_allocations)
        VirtualFree((void*)ptr, 0, MEM_RELEASE);

    virtual_allocations.clear();
}

Building* Global_Make_Building(
    Element_Tile*& element_tiles,
    Arena& trash_arena,
    Building_Type type,
    v2i pos  //
) {
    auto sb = Allocate_Zeros_For(trash_arena, Scriptable_Building);
    sb->type = type;
    auto building = Allocate_Zeros_For(trash_arena, Building);
    building->scriptable = sb;
    return building;
}

int Process_Segments(
    v2i& gsize,
    Element_Tile*& element_tiles,
    Segment_Manager*& manager,
    Arena& trash_arena,
    Allocator& segment_vertices_allocator,
    Allocator& graph_nodes_allocator,
    Pages& pages,
    Building*& building_sawmill,
    std::vector<const char*> strings  //
) {
    // NOTE(hulvdan): Creating `element_tiles`
    gsize.y = strings.size();
    gsize.x = strlen(strings[0]);

    for (auto string_ptr : strings)
        REQUIRE(strlen(string_ptr) == gsize.x);

    auto tiles_count = gsize.x * gsize.y;
    element_tiles = Allocate_Zeros_Array(trash_arena, Element_Tile, tiles_count);

    {
        auto meta_size = sizeof(Graph_Segment_Page_Meta);
        auto struct_size = sizeof(Graph_Segment);

        auto max_pages_count =
            Ceil_Division(tiles_count * struct_size, os_data.page_size);
        Assert(max_pages_count < 100);
        Assert(max_pages_count > 0);

        manager = Allocate_For(trash_arena, Segment_Manager);
        manager->segment_pages_total = max_pages_count;
        manager->segment_pages_used = 0;
        manager->segment_pages =
            Allocate_Zeros_Array(trash_arena, Page, manager->segment_pages_total);
        manager->max_segments_per_page =
            Assert_Truncate_To_u16((os_data.page_size - meta_size) / struct_size);
        manager->page_meta_offset = os_data.page_size - meta_size;
    }

    auto tiles = Allocate_Zeros_Array(trash_arena, Element_Tile, tiles_count);
    auto Make_Building = [&element_tiles, &trash_arena](Building_Type type, v2i pos) {
        return Global_Make_Building(element_tiles, trash_arena, type, pos);
    };

    FOR_RANGE(int, y, gsize.y) {
        FOR_RANGE(int, x, gsize.x) {
            auto& tile = *(element_tiles + y * gsize.x + x);
            v2i pos = {x, y};

            const char symbol = *(strings[gsize.y - y - 1] + x);
            switch (symbol) {
            case 'C': {
                auto building = Make_Building(Building_Type::City_Hall, pos);
                // new (tile) Element_Tile(Element_Tile_Type::Building, building);
                tile.type = Element_Tile_Type::Building;
                tile.building = building;
            } break;

            case 'B': {
                auto building = Make_Building(Building_Type::Produce, pos);
                tile.type = Element_Tile_Type::Building;
                tile.building = building;
            } break;

            case 'S': {
                if (building_sawmill == nullptr)
                    building_sawmill = Make_Building(Building_Type::Produce, pos);

                tile.type = Element_Tile_Type::Building;
                tile.building = building_sawmill;
            } break;

            case 'r': {
                tile.type = Element_Tile_Type::Road;
                tile.building = nullptr;
            } break;

            case 'F': {
                tile.type = Element_Tile_Type::Flag;
                tile.building = nullptr;
            } break;

            case '.': {
                tile.type = Element_Tile_Type::None;
                tile.building = nullptr;
            } break;

            default:
                INVALID_PATH;
            }
        }
    }

    // NOTE(hulvdan): Counting segments
    Build_Graph_Segments(
        gsize, element_tiles, *manager, trash_arena, segment_vertices_allocator,
        graph_nodes_allocator, pages, os_data  //
    );

    int segments_count = 0;
    for (auto _ : Iter(*manager))
        segments_count++;

    return segments_count;
};

#define Process_Segments_Macro(...)                                             \
    std::vector<const char*> _strings = {__VA_ARGS__};                          \
    auto(segments_count) = Process_Segments(                                    \
        gsize, element_tiles, manager, trash_arena, segment_vertices_allocator, \
        graph_nodes_allocator, pages, building_sawmill, _strings);

#define Update_Tiles_Macro(updated_tiles)                                        \
    REQUIRE(manager != nullptr);                                                 \
    auto [added_segments_count, removed_segments_count] = Update_Tiles(          \
        gsize, element_tiles, *manager, trash_arena, segment_vertices_allocator, \
        graph_nodes_allocator, pages, os_data, (updated_tiles));

#define Test_Declare_Updated_Tiles(...)                                          \
    Updated_Tiles updated_tiles = {};                                            \
    {                                                                            \
        std::vector<tuple<v2i, Tile_Updated_Type>> data = {__VA_ARGS__};         \
        updated_tiles.pos = Allocate_Zeros_Array(trash_arena, v2i, data.size()); \
        updated_tiles.type =                                                     \
            Allocate_Zeros_Array(trash_arena, Tile_Updated_Type, data.size());   \
        FOR_RANGE(int, i, data.size()) {                                         \
            auto& [pos, type] = data[i];                                         \
            *(updated_tiles.pos + i) = pos;                                      \
            *(updated_tiles.type + i) = type;                                    \
        }                                                                        \
        updated_tiles.count = data.size();                                       \
    }

TEST_CASE("Update_Tiles") {
    SYSTEM_INFO system_info;
    GetSystemInfo(&system_info);
    os_data.page_size = system_info.dwPageSize;
    os_data.min_pages_per_allocation =
        system_info.dwAllocationGranularity / os_data.page_size;
    os_data.Allocate_Pages = Win32_Allocate_Pages;

    Arena trash_arena = {};
    auto trash_size = Megabytes((size_t)2);
    trash_arena.size = trash_size;
    trash_arena.base = new u8[trash_size];

    auto segment_vertices_allocator_buf = Allocate_Zeros_For(trash_arena, Allocator);
    new (segment_vertices_allocator_buf) Allocator(
        1024, Allocate_Zeros_Array(trash_arena, u8, 1024),  //
        4096, Allocate_Zeros_Array(trash_arena, u8, 4096));
    auto& segment_vertices_allocator = *segment_vertices_allocator_buf;

    auto graph_nodes_allocator_buf = Allocate_Zeros_For(trash_arena, Allocator);
    new (graph_nodes_allocator_buf) Allocator(
        1024, Allocate_Zeros_Array(trash_arena, u8, 1024),  //
        4096, Allocate_Zeros_Array(trash_arena, u8, 4096));
    auto& graph_nodes_allocator = *graph_nodes_allocator_buf;

    Pages pages = {};
    {
        auto pages_count = Megabytes((size_t)1) / os_data.page_size;
        pages.base = Allocate_Zeros_Array(trash_arena, Page, pages_count);
        pages.in_use = Allocate_Zeros_Array(trash_arena, bool, pages_count);
        pages.total_count_cap = pages_count;
    }

    Building* building_sawmill = nullptr;
    v2i gsize = -v2i_one;
    Element_Tile* element_tiles = nullptr;
    Segment_Manager* manager = nullptr;

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
            "Cr"  //
        );
        CHECK(segments_count == 1);
    }

    SUBCASE("Test_1Building_1Road_0Segments") {
        Process_Segments_Macro(
            "..",  //
            "Cr"  //
        );
        CHECK(segments_count == 0);
    }

    SUBCASE("Test_2AdjacentBuildings_0Segments") {
        Process_Segments_Macro(
            "..",  //
            "CB"  //
        );
        CHECK(segments_count == 0);
    }

    SUBCASE("Test_4AdjacentBuildings_0Segments") {
        Process_Segments_Macro(
            "BB",  //
            "CB"  //
        );
        CHECK(segments_count == 0);
    }

    SUBCASE("Test_2Buildings_1Flag_0Segments") {
        Process_Segments_Macro(
            ".B",  //
            "CF"  //
        );
        CHECK(segments_count == 0);
    }

    SUBCASE("Test_2Buildings_1Flag_1Road_1Segment") {
        Process_Segments_Macro(
            "FB",  //
            "Cr"  //
        );
        CHECK(segments_count == 1);
    }

    SUBCASE("Test_2Buildings_2Flags_0Segments") {
        Process_Segments_Macro(
            "FF",  //
            "CB"  //
        );
        CHECK(segments_count == 0);
    }

    SUBCASE("Test_2Buildings_2Flags_0Segments") {
        Process_Segments_Macro(
            "FB",  //
            "CF"  //
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
            "CrrrrB"  //
        );
        CHECK(segments_count == 1);
    }

    SUBCASE("Test_Complex4") {
        Process_Segments_Macro(
            "...B..",  //
            "..rrr.",  //
            "BrrCrB",  //
            "...r..",  //
            "...B.."  //
        );
        CHECK(segments_count == 2);
    }

    SUBCASE("Test_Complex5") {
        Process_Segments_Macro(
            "...B..",  //
            "..rFr.",  //
            "BrrCrB",  //
            "...r..",  //
            "...B.."  //
        );
        CHECK(segments_count == 3);
    }

    SUBCASE("Test_Complex6") {
        Process_Segments_Macro(
            "...B..",  //
            "..rrr.",  //
            "CrrFrB"  //
        );
        CHECK(segments_count == 1);
    }

    SUBCASE("Test_Complex7") {
        Process_Segments_Macro(
            "...B..",  //
            "..rrr.",  //
            "CrrFrB",  //
            "...r..",  //
            "...B.."  //
        );
        CHECK(segments_count == 2);
    }

    SUBCASE("Test_Complex8") {
        Process_Segments_Macro(
            "...B..",  //
            "...r..",  //
            "BrrCrB",  //
            "...r..",  //
            "...B.."  //
        );
        CHECK(segments_count == 4);
    }

    SUBCASE("Test_Complex9") {
        Process_Segments_Macro(
            "...B..",  //
            "..rrr.",  //
            "BrrCrB",  //
            "..rrr.",  //
            "...B.."  //
        );
        CHECK(segments_count == 1);
    }

    SUBCASE("Test_Complex10") {
        Process_Segments_Macro(
            "...B..",  //
            "..rrr.",  //
            "BrrCrB",  //
            "..rr..",  //
            "...B.."  //
        );
        CHECK(segments_count == 1);
    }

    SUBCASE("Test_Complex11") {
        Process_Segments_Macro(
            "...B...",  //
            "..rrrr.",  //
            "CrrSSrB",  //
            "....rr."  //
        );
        CHECK(segments_count == 1);
    }

    SUBCASE("Test_Complex12") {
        Process_Segments_Macro(
            "...B...",  //
            "..rFrr.",  //
            "CrrSSrB",  //
            "....rr."  //
        );
        CHECK(segments_count == 2);
    }

    SUBCASE("Test_Complex13") {
        Process_Segments_Macro(
            "...B...",  //
            "..rFFr.",  //
            "CrrSSrB",  //
            "....rr."  //
        );
        CHECK(segments_count == 2);
    }

    SUBCASE("Test_Complex14") {
        Process_Segments_Macro(
            "...B...",  //
            "..rrFr.",  //
            "CrrSSrB",  //
            "....rr."  //
        );
        CHECK(segments_count == 2);
    }

    SUBCASE("Test_Complex15") {
        Process_Segments_Macro(
            "CrF",  //
            ".rB"  //
        );
        CHECK(segments_count == 1);
    }

    SUBCASE("Test_Complex16") {
        Process_Segments_Macro(
            "CrFr",  //
            ".rSS"  //
        );
        CHECK(segments_count == 2);
    }

    SUBCASE("Test_Complex17") {
        Process_Segments_Macro(
            "Crrr",  //
            ".rSS"  //
        );
        CHECK(segments_count == 1);
    }

    SUBCASE("Test_Complex18") {
        Process_Segments_Macro(
            ".B.",  //
            "CFB",  //
            ".B."  //
        );
        CHECK(segments_count == 0);
    }

    SUBCASE("Test_Complex19") {
        Process_Segments_Macro(
            ".B.",  //
            "CrB",  //
            ".B."  //
        );
        CHECK(segments_count == 1);
    }

    SUBCASE("Test_Complex20") {
        Process_Segments_Macro(
            "..B..",  //
            "..r..",  //
            "CrFrB",  //
            "..r..",  //
            "..B.."  //
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
            "B.B"  //
        );
        CHECK(segments_count == 2);
    }

    SUBCASE("Test_Horizontal_Lines") {
        Process_Segments_Macro(
            "BrB",  //
            "...",  //
            "BrB"  //
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
        Process_Segments_Macro(
            ".B",  //
            ".F",  //
            "Cr");
        CHECK(segments_count == 1);

        auto pos = v2i(0, 1);
        CHECK(GRID_PTR_VALUE(element_tiles, pos).type == Element_Tile_Type::None);
        GRID_PTR_VALUE(element_tiles, pos).type = Element_Tile_Type::Road;

        Test_Declare_Updated_Tiles(
            {pos, Tile_Updated_Type::Road_Placed},  //
        );
        Update_Tiles_Macro(updated_tiles);

        CHECK(added_segments_count == 1);
        CHECK(removed_segments_count == 0);
    }

    SUBCASE("Test_RoadPlaced_2") {
        Process_Segments_Macro(
            ".B",  //
            "Cr");
        CHECK(segments_count == 1);

        auto pos = v2i(0, 1);
        CHECK(GRID_PTR_VALUE(element_tiles, pos).type == Element_Tile_Type::None);
        GRID_PTR_VALUE(element_tiles, pos).type = Element_Tile_Type::Road;

        Test_Declare_Updated_Tiles(
            {pos, Tile_Updated_Type::Road_Placed},  //
        );
        Update_Tiles_Macro(updated_tiles);

        CHECK(added_segments_count == 1);
        CHECK(removed_segments_count == 0);
    }

    SUBCASE("Test_RoadPlaced_3") {
        Process_Segments_Macro(
            ".B",  //
            "CF");

        auto pos = v2i(0, 1);
        CHECK(GRID_PTR_VALUE(element_tiles, pos).type == Element_Tile_Type::None);
        GRID_PTR_VALUE(element_tiles, pos).type = Element_Tile_Type::Road;

        Test_Declare_Updated_Tiles(
            {pos, Tile_Updated_Type::Road_Placed},  //
        );
        Update_Tiles_Macro(updated_tiles);

        CHECK(added_segments_count == 1);
        CHECK(removed_segments_count == 0);
    }

    SUBCASE("Test_RoadPlaced_4") {
        Process_Segments_Macro(
            ".rr",  //
            "Crr");

        {
            auto pos = v2i(1, 0);
            GRID_PTR_VALUE(element_tiles, pos).type = Element_Tile_Type::Flag;

            Test_Declare_Updated_Tiles(
                {pos, Tile_Updated_Type::Flag_Placed},  //
            );
            Update_Tiles_Macro(updated_tiles);

            CHECK(added_segments_count == 0);
            CHECK(removed_segments_count == 0);
        }

        {
            auto pos = v2i(2, 1);
            GRID_PTR_VALUE(element_tiles, pos).type = Element_Tile_Type::Flag;

            Test_Declare_Updated_Tiles(
                {pos, Tile_Updated_Type::Flag_Placed},  //
            );
            Update_Tiles_Macro(updated_tiles);

            CHECK(added_segments_count == 2);
            CHECK(removed_segments_count == 0);
        }

        {
            auto pos = v2i(0, 1);
            GRID_PTR_VALUE(element_tiles, pos).type = Element_Tile_Type::Road;

            Test_Declare_Updated_Tiles(
                {pos, Tile_Updated_Type::Road_Placed},  //
            );
            Update_Tiles_Macro(updated_tiles);

            CHECK(added_segments_count == 1);
            CHECK(removed_segments_count == 1);
        }
    }

    SUBCASE("Test_FlagPlaced_1") {
        Process_Segments_Macro(
            ".B",  //
            ".r",  //
            "Cr");

        auto pos = v2i(1, 1);
        GRID_PTR_VALUE(element_tiles, pos).type = Element_Tile_Type::Flag;

        Test_Declare_Updated_Tiles(
            {pos, Tile_Updated_Type::Flag_Placed},  //
        );
        Update_Tiles_Macro(updated_tiles);

        CHECK(added_segments_count == 1);
        CHECK(removed_segments_count == 1);
    }

    SUBCASE("Test_FlagPlaced_2") {
        Process_Segments_Macro(
            ".B",  //
            ".r",  //
            "CF");
        CHECK(segments_count == 1);

        auto pos = v2i(1, 1);
        GRID_PTR_VALUE(element_tiles, pos).type = Element_Tile_Type::Flag;

        Test_Declare_Updated_Tiles(
            {pos, Tile_Updated_Type::Flag_Placed},  //
        );
        Update_Tiles_Macro(updated_tiles);

        CHECK(added_segments_count == 0);
        CHECK(removed_segments_count == 1);
    }

    SUBCASE("Test_FlagPlaced_3") {
        Process_Segments_Macro(
            ".B",  //
            "CF");

        auto pos = v2i(0, 1);
        GRID_PTR_VALUE(element_tiles, pos).type = Element_Tile_Type::Flag;

        Test_Declare_Updated_Tiles(
            {pos, Tile_Updated_Type::Flag_Placed},  //
        );
        Update_Tiles_Macro(updated_tiles);

        CHECK(added_segments_count == 0);
        CHECK(removed_segments_count == 0);
    }

    SUBCASE("Test_FlagPlaced_4") {
        Process_Segments_Macro("CrrrB");
        CHECK(segments_count == 1);

        auto pos = v2i(2, 0);
        GRID_PTR_VALUE(element_tiles, pos).type = Element_Tile_Type::Flag;

        Test_Declare_Updated_Tiles(
            {pos, Tile_Updated_Type::Flag_Placed},  //
        );
        Update_Tiles_Macro(updated_tiles);

        CHECK(added_segments_count == 2);
        CHECK(removed_segments_count == 1);
    }

    SUBCASE("Test_BuildingPlaced_1") {
        Process_Segments_Macro(
            ".B",  //
            "CF");

        auto pos = v2i(0, 1);
        auto building = Make_Building(Building_Type::Produce, pos);
        GRID_PTR_VALUE(element_tiles, pos).type = Element_Tile_Type::Building;
        GRID_PTR_VALUE(element_tiles, pos).building = building;

        Test_Declare_Updated_Tiles(
            {pos, Tile_Updated_Type::Building_Placed},  //
        );
        Update_Tiles_Macro(updated_tiles);

        CHECK(added_segments_count == 0);
        CHECK(removed_segments_count == 0);
    }

    SUBCASE("Test_BuildingPlaced_2") {
        Process_Segments_Macro(
            "..",  //
            "CF");
        CHECK(segments_count == 0);

        auto pos = v2i(1, 1);
        auto building = Make_Building(Building_Type::Produce, pos);
        GRID_PTR_VALUE(element_tiles, pos).type = Element_Tile_Type::Building;
        GRID_PTR_VALUE(element_tiles, pos).building = building;

        Test_Declare_Updated_Tiles(
            {pos, Tile_Updated_Type::Building_Placed},  //
        );
        Update_Tiles_Macro(updated_tiles);

        CHECK(added_segments_count == 0);
        CHECK(removed_segments_count == 0);
    }

    SUBCASE("Test_BuildingPlaced_3") {
        Process_Segments_Macro(
            "..",  //
            "Cr");
        CHECK(segments_count == 0);

        auto pos = v2i(1, 1);
        auto building = Make_Building(Building_Type::Produce, pos);
        GRID_PTR_VALUE(element_tiles, pos).type = Element_Tile_Type::Building;
        GRID_PTR_VALUE(element_tiles, pos).building = building;

        Test_Declare_Updated_Tiles(
            {pos, Tile_Updated_Type::Building_Placed},  //
        );
        Update_Tiles_Macro(updated_tiles);

        CHECK(added_segments_count == 1);
        CHECK(removed_segments_count == 0);
    }

    SUBCASE("Test_BuildingRemoved_1") {
        Process_Segments_Macro(
            ".B",  //
            "Cr"  //
        );
        CHECK(segments_count == 1);

        auto pos = v2i(1, 1);
        GRID_PTR_VALUE(element_tiles, pos).type = Element_Tile_Type::None;
        GRID_PTR_VALUE(element_tiles, pos).building = nullptr;

        Test_Declare_Updated_Tiles(
            {pos, Tile_Updated_Type::Building_Removed},  //
        );
        Update_Tiles_Macro(updated_tiles);

        CHECK(added_segments_count == 0);
        CHECK(removed_segments_count == 1);
    }

    SUBCASE("Test_BuildingRemoved_2") {
        Process_Segments_Macro(
            ".B.",  //
            "CrB"  //
        );
        CHECK(segments_count == 1);

        auto pos = v2i(1, 1);
        GRID_PTR_VALUE(element_tiles, pos).type = Element_Tile_Type::None;
        GRID_PTR_VALUE(element_tiles, pos).building = nullptr;

        Test_Declare_Updated_Tiles(
            {pos, Tile_Updated_Type::Building_Removed},  //
        );
        Update_Tiles_Macro(updated_tiles);

        CHECK(added_segments_count == 1);
        CHECK(removed_segments_count == 1);
    }

    SUBCASE("Shit_Test") {
        Process_Segments_Macro(
            "...",  //
            "...");
        CHECK(segments_count == 0);

        {
            auto pos = v2i(0, 0);
            GRID_PTR_VALUE(element_tiles, pos).type = Element_Tile_Type::Road;

            Test_Declare_Updated_Tiles(
                {pos, Tile_Updated_Type::Road_Placed},  //
            );
            Update_Tiles_Macro(updated_tiles);

            CHECK(added_segments_count == 0);
            CHECK(removed_segments_count == 0);
        }

        {
            auto pos = v2i(1, 0);
            GRID_PTR_VALUE(element_tiles, pos).type = Element_Tile_Type::Road;

            Test_Declare_Updated_Tiles(
                {pos, Tile_Updated_Type::Road_Placed},  //
            );
            Update_Tiles_Macro(updated_tiles);

            CHECK(added_segments_count == 0);
            CHECK(removed_segments_count == 0);
        }

        {
            auto pos = v2i(2, 0);
            GRID_PTR_VALUE(element_tiles, pos).type = Element_Tile_Type::Road;

            Test_Declare_Updated_Tiles(
                {pos, Tile_Updated_Type::Road_Placed},  //
            );
            Update_Tiles_Macro(updated_tiles);

            CHECK(added_segments_count == 0);
            CHECK(removed_segments_count == 0);
        }

        {
            auto pos = v2i(0, 1);
            GRID_PTR_VALUE(element_tiles, pos).type = Element_Tile_Type::Road;

            Test_Declare_Updated_Tiles(
                {pos, Tile_Updated_Type::Road_Placed},  //
            );
            Update_Tiles_Macro(updated_tiles);

            CHECK(added_segments_count == 0);
            CHECK(removed_segments_count == 0);
        }

        {
            auto pos = v2i(1, 1);
            GRID_PTR_VALUE(element_tiles, pos).type = Element_Tile_Type::Road;

            Test_Declare_Updated_Tiles(
                {pos, Tile_Updated_Type::Road_Placed},  //
            );
            Update_Tiles_Macro(updated_tiles);

            CHECK(added_segments_count == 0);
            CHECK(removed_segments_count == 0);
        }

        {
            auto pos = v2i(0, 0);
            GRID_PTR_VALUE(element_tiles, pos).type = Element_Tile_Type::Flag;

            Test_Declare_Updated_Tiles(
                {pos, Tile_Updated_Type::Flag_Placed},  //
            );
            Update_Tiles_Macro(updated_tiles);

            CHECK(added_segments_count == 0);
            CHECK(removed_segments_count == 0);
        }

        {
            auto pos = v2i(1, 1);
            GRID_PTR_VALUE(element_tiles, pos).type = Element_Tile_Type::Flag;

            Test_Declare_Updated_Tiles(
                {pos, Tile_Updated_Type::Flag_Placed},  //
            );
            Update_Tiles_Macro(updated_tiles);

            CHECK(added_segments_count == 2);
            CHECK(removed_segments_count == 0);
        }

        {
            auto pos = v2i(2, 0);
            GRID_PTR_VALUE(element_tiles, pos).type = Element_Tile_Type::Flag;

            Test_Declare_Updated_Tiles(
                {pos, Tile_Updated_Type::Flag_Placed},  //
            );
            Update_Tiles_Macro(updated_tiles);

            CHECK(added_segments_count == 1);
            CHECK(removed_segments_count == 1);
        }

        {
            auto pos = v2i(1, 0);
            GRID_PTR_VALUE(element_tiles, pos).type = Element_Tile_Type::Flag;

            Test_Declare_Updated_Tiles(
                {pos, Tile_Updated_Type::Flag_Placed},  //
            );
            Update_Tiles_Macro(updated_tiles);

            CHECK(added_segments_count == 0);
            CHECK(removed_segments_count == 1);
        }
    }

    SUBCASE("Shit_Test2") {
        Process_Segments_Macro(
            "rF.",  //
            "FrF"  //
        );
        CHECK(segments_count == 2);

        {
            auto pos = v2i(1, 0);
            GRID_PTR_VALUE(element_tiles, pos).type = Element_Tile_Type::Flag;

            Test_Declare_Updated_Tiles(
                {pos, Tile_Updated_Type::Flag_Placed},  //
            );
            Update_Tiles_Macro(updated_tiles);

            CHECK(added_segments_count == 0);
            CHECK(removed_segments_count == 1);
        }
    }

    SUBCASE("Shit_Test3") {
        Process_Segments_Macro(
            ".F.",  //
            "FFF"  //
        );
        CHECK(segments_count == 0);

        {
            auto pos = v2i(1, 0);
            GRID_PTR_VALUE(element_tiles, pos).type = Element_Tile_Type::Road;

            Test_Declare_Updated_Tiles(
                {pos, Tile_Updated_Type::Flag_Removed},  //
            );
            Update_Tiles_Macro(updated_tiles);

            CHECK(added_segments_count == 1);
            CHECK(removed_segments_count == 0);
        }
    }

    delete[] trash_arena.base;
    Free_Allocations();
}
