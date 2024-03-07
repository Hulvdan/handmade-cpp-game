#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

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

    auto rules_data = "grass_7\ngrass_1\n| * |\n|*@@|\n| @ |\ngrass_2\n| * |\n|@@@|\n| @ |";
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

    auto rules_data = "grass_7\ngrass_1\n| * |\n|*@@|\n| @ |\ngrass_2\n| * |\n|@@@|\n| @ |\n";
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

#define Linked_List_Add_Macro(nodes_, n_, first_node_index_, node_to_add_)                       \
    {                                                                                            \
        Linked_List_Add(                                                                         \
            rcast<u8*>(nodes_), (n_), (first_node_index_), rcast<u8*>(&(node_to_add_)),          \
            offsetof(node_to_add_, active), offsetof(node_to_add_, next), sizeof(node_to_add_)); \
    }

TEST_CASE("Linked List") {
    Test_Node* nodes = new Test_Node[10]{};

    size_t count = 0;
    size_t first_node_index = 0;

    {
        auto node_to_add = Test_Node(1);
        Linked_List_Add_Macro(nodes, count, first_node_index, node_to_add);
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
        Linked_List_Add_Macro(nodes, count, first_node_index, node_to_add);
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
}
