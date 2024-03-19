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

TEST_CASE("Update_Tiles") {
    // void Update_Tiles(
    //     v2i gsize,
    //     Element_Tile* element_tiles,
    //     Segment_Manager& segment_manager,
    //     Arena& trash_arena,
    //     Allocator& segment_vertices_allocator,
    //     Allocator& graph_nodes_allocator,
    //     Pages& pages,
    //     OS_Data& os_data,
    //     const Updated_Tiles& updated_tiles  //
    // )

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
    trash_arena.name = "trash";

    u8 segment_vertices_toc[1024] = {};
    u8 segment_vertices_data[1024] = {};
    auto segment_vertices_allocator = std::make_unique<Allocator>(
        1024, segment_vertices_toc, 1024, segment_vertices_data);

    u8 graph_nodes_toc[1024] = {};
    u8 graph_nodes_data[1024] = {};
    auto graph_nodes_allocator =
        std::make_unique<Allocator>(1024, graph_nodes_toc, 1024, graph_nodes_data);

    Segment_Manager manager = {};
    manager.segment_pages = nullptr;  // TODO:
    manager.segment_pages_used = 0;
    manager.segment_pages_total = 0;  // TODO:
    manager.max_segments_per_page = os_data.page_size / sizeof(Graph_Segment);

    // SUBCASE("1") {
    //     // TODO:
    //     //          auto element_tiles = Parse_As_Element_Tiles(R"map(
    //     //  BrB
    //     //          )map");
    //
    //     // Update_Tiles();
    //
    //     // int segments_count = 0;
    //     // for (auto _ : Iter(manager))
    //     //     segments_count++;
    //     //
    //     // CHECK(segments_count == 2);
    // }
    //
    // SUBCASE("2") {
    //     CHECK(false);
    // }

    delete[] trash_arena.base;
    Free_Allocations();
}
