#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#define BF_CLIENT

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
