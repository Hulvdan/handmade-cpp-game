#include <gtest/gtest.h>

#include "bf_base.h"

// NOLINTBEGIN(bugprone-suspicious-include)
#include "bf_hash.cpp"
#include "bf_strings.cpp"
#include "bf_tilemap.cpp"
// NOLINTEND(bugprone-suspicious-include)

TEST(Hash32, EmptyIsCorrect)
{
    EXPECT_EQ(Hash32((u8*)"", 0), 2166136261);
}
TEST(Hash32, TestValue)
{
    EXPECT_EQ(Hash32((u8*)"test", 4), 2949673445);
}

TEST(LoadSmartTileRules, ItWorks)
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

    EXPECT_EQ(result.success, true);
    EXPECT_EQ(tile.rules_count, 2);
    EXPECT_EQ(Hash32((u8*)"test", 4), 2949673445);
}

TEST(LoadSmartTileRules, ItWorksWithANewlineOnTheEnd)
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

    EXPECT_EQ(result.success, true);
    EXPECT_EQ(tile.rules_count, 2);
    EXPECT_EQ(Hash32((u8*)"test", 4), 2949673445);
}
