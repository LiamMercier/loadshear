#include <gtest/gtest.h>

TEST(ExampleTest, FailCase)
{
    EXPECT_EQ(0, 1);
    ASSERT_EQ(0, 2);
}
