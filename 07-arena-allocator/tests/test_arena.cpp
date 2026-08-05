#include "arena_allocator/arena.hpp"

#include <cstdint>
#include <stdexcept>
#include <string>

#include <gtest/gtest.h>

namespace {

bool is_aligned(const void* p, std::size_t alignment) {
    return (reinterpret_cast<std::uintptr_t>(p) % alignment) == 0;
}

}  // namespace

TEST(Arena, AllocatesAndTracksUsed) {
    portfolio::Arena arena(64);
    void* a = arena.try_allocate(16);
    ASSERT_NE(a, nullptr);
    EXPECT_GE(arena.used(), 16u);
    EXPECT_EQ(arena.remaining(), arena.capacity() - arena.used());

    void* b = arena.try_allocate(8);
    ASSERT_NE(b, nullptr);
    EXPECT_NE(a, b);
}

TEST(Arena, ReturnsNullWhenFull) {
    portfolio::Arena arena(32);
    ASSERT_NE(arena.try_allocate(32), nullptr);
    EXPECT_EQ(arena.try_allocate(1), nullptr);
}

TEST(Arena, ResetAllowsReuse) {
    portfolio::Arena arena(32);
    void* first = arena.try_allocate(32);
    ASSERT_NE(first, nullptr);
    EXPECT_EQ(arena.try_allocate(1), nullptr);

    arena.reset();
    EXPECT_EQ(arena.used(), 0u);
    void* again = arena.try_allocate(32);
    ASSERT_NE(again, nullptr);
    EXPECT_EQ(again, first);  // same backing storage, cursor rewound
}

TEST(Arena, HonorsAlignment) {
    portfolio::Arena arena(128);
    // Force a misaligned cursor relative to 16, then request 16-byte align.
    ASSERT_NE(arena.try_allocate(1), nullptr);
    void* p = arena.try_allocate(8, 16);
    ASSERT_NE(p, nullptr);
    EXPECT_TRUE(is_aligned(p, 16));
}

TEST(Arena, TypedAllocateAndCreate) {
    portfolio::Arena arena(256);
    int* nums = arena.try_allocate<int>(4);
    ASSERT_NE(nums, nullptr);
    EXPECT_TRUE(is_aligned(nums, alignof(int)));

    auto* s = arena.try_create<std::string>("arena");
    ASSERT_NE(s, nullptr);
    EXPECT_EQ(*s, "arena");
    s->~basic_string();  // explicit destroy before reset (non-trivial)
    arena.reset();
}

TEST(Arena, ZeroCapacityThrows) {
    EXPECT_THROW(portfolio::Arena(0), std::invalid_argument);
}

TEST(Arena, BadAlignmentThrows) {
    portfolio::Arena arena(64);
    EXPECT_THROW((void)arena.try_allocate(8, 3), std::invalid_argument);
}

TEST(Arena, ZeroSizeAllocateReturnsNull) {
    portfolio::Arena arena(64);
    EXPECT_EQ(arena.try_allocate(0), nullptr);
}
