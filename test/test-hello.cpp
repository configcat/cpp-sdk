#include <gtest/gtest.h>
#include "configcat/configcat.hpp"

// Demonstrate some basic assertions.
TEST(HelloTest, BasicAssertions) {
    // Expect two strings not to be equal.
    EXPECT_STRNE("hello", "world");
    // Expect equality.
    EXPECT_EQ(7 * 6, 42);
    // ConfigCat version check.
    EXPECT_EQ(version(), "0.0.1");
}
