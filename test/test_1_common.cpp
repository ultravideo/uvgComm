#include "../src/common.h"

#include <gtest/gtest.h>


TEST(CommonTest, random) {
    QString r0 = generateRandomString(0);
    QString r1 = generateRandomString(1);
    QString r2 = generateRandomString(2);
    QString r100 = generateRandomString(100);

    EXPECT_EQ(r0.length(), 0);
    EXPECT_EQ(r1.length(), 1);
    EXPECT_EQ(r2.length(), 2);
    EXPECT_EQ(r100.length(), 100);
}
