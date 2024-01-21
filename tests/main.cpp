#include <gtest/gtest.h>

#include "bf_base.h"
#include "bf_hash.cpp"

TEST(Hash32, EmptyIsCorrect)
{
    EXPECT_EQ(Hash32((u8*)"", 0), 2166136261);
}
TEST(Hash32, TestValue)
{
    EXPECT_EQ(Hash32((u8*)"test", 4), 2949673445);
}
