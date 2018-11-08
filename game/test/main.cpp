#include "gtest/gtest.h"

int Add(int a, int b) {
	return a + b;
}

TEST(testCase, test0)
{
	int a = 2;
	EXPECT_EQ(14, Add(a, 10));//EXPECT_EQ是比较两个值是否相等
}

int main(int argc, char ** argv) {
	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
