#include <gtest/gtest.h>

#include <algorithm>
#include <fixed_vector.hpp>

TEST(FixedVectorTest, lvalue_constructible) { FixedVector<int> a {1, 2, 3}; }

TEST(FixedVectorTest, rvalue_constructible) {
	(void)FixedVector<int> {1, 2, 3};
}

TEST(FixedVectorTest, copy_constructible) {
	FixedVector<int> a {1, 2, 3};
	FixedVector<int> b {a};

	ASSERT_EQ(a.size(), 3);
	ASSERT_TRUE(a[0] == 1 and a[1] == 2 and a[2] == 3);
	ASSERT_EQ(b.size(), 3);
	ASSERT_TRUE(b[0] == 1 and b[1] == 2 and b[2] == 3);
}

TEST(FixedVectorTest, default_constructible) { FixedVector<int> a {}; }

TEST(FixedVectorTest, many_constructible) {
	FixedVector<std::string> a {3, "a"};
	ASSERT_EQ(a.size(), 3);
	ASSERT_TRUE(a[0] == "a" and a[1] == "a" and a[2] == "a");
}

TEST(FixedVectorTest, move_constructible) {
	FixedVector<int> a {1, 2, 3};
	FixedVector<int> b {std::move(a)};
}

TEST(FixedVectorTest, copy_assignable) {
	FixedVector<int> a {1, 2, 3};
	FixedVector<int> b = a;
}

TEST(FixedVectorTest, move_assignable) {
	FixedVector<int> a {1, 2, 3};
	FixedVector<int> b = std::move(a);
}

TEST(FixedVectorTest, transforming) {
	FixedVector<std::string> a {3, "a"};
	FixedVector<int> b {1, 2, 3};
	std::transform(b.begin(), b.end(), a.begin(), [](auto x) {
		return std::to_string(x);
	});
	ASSERT_EQ(a.size(), 3);
	ASSERT_TRUE(a[0] == "1" and a[1] == "2" and a[2] == "3");
}
