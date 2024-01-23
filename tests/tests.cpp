#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "range.h"

#include "bf_base.h"

// NOLINTBEGIN(bugprone-suspicious-include)
#include "bf_hash.cpp"
#include "bf_strings.cpp"
#include "bf_rand.cpp"

#define BF_CLIENT
#include "bfc_tilemap.cpp"
// NOLINTEND(bugprone-suspicious-include)

TEST_CASE("Hash32, EmptyIsCorrect")
{
    CHECK(Hash32((u8*)"", 0) == 2166136261);
}

TEST_CASE("Hash32, TestValue")
{
    CHECK(Hash32((u8*)"test", 4) == 2949673445);
}

TEST_CASE("LoadSmartTileRules, ItWorks")
{
    constexpr u64 size = 512;
    u8 output[size] = {};

    auto rules_data = "grass_7\ngrass_1\n| * |\n|*@@|\n| @ |\ngrass_2\n| * |\n|@@@|\n| @ |";
    i32 rules_data_size = 0;
    auto p = rules_data;
    while (*p++)
        rules_data_size++;

    SmartTile tile = {};
    auto result = LoadSmartTileRules(tile, output, size, (u8*)rules_data, rules_data_size);

    CHECK(result.success == true);
    CHECK(tile.rules_count == 2);
    CHECK(Hash32((u8*)"test", 4) == 2949673445);
}

TEST_CASE("LoadSmartTileRules, ItWorksWithANewlineOnTheEnd")
{
    constexpr u64 size = 512;
    u8 output[size] = {};

    auto rules_data = "grass_7\ngrass_1\n| * |\n|*@@|\n| @ |\ngrass_2\n| * |\n|@@@|\n| @ |\n";
    i32 rules_data_size = 0;
    auto p = rules_data;
    while (*p++)
        rules_data_size++;

    SmartTile tile = {};
    auto result = LoadSmartTileRules(tile, output, size, (u8*)rules_data, rules_data_size);

    CHECK(result.success == true);
    CHECK(tile.rules_count == 2);
    CHECK(Hash32((u8*)"test", 4) == 2949673445);
}

TEST_CASE("ProtoTest, Proto")
{
    CHECK(0xFF == 255);
    CHECK(0x00FF == 255);
    CHECK(0xFF00 == 65280);

    CHECK(0b11111111 == 255);
    CHECK(0b0000000011111111 == 255);
    CHECK(0b1111111100000000 == 65280);
}

TEST_CASE("frand, interval")
{
    for (int i = 0; i < 10000; i++) {
        auto value = frand();
        CHECK(value >= 0);
        CHECK(value < 1);
    }
}
