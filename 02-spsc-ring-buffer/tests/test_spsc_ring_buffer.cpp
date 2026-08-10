#include "spsc_ring_buffer/spsc_ring_buffer.hpp"

#include <atomic>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

using klib::SpscRingBuffer;

TEST(SpscRingBuffer, RejectsZeroCapacity) {
    EXPECT_THROW(SpscRingBuffer<int>(0), std::invalid_argument);
}

TEST(SpscRingBuffer, RoundsCapacityUpToPowerOfTwo) {
    SpscRingBuffer<int> buffer(5);
    EXPECT_EQ(buffer.capacity(), 8u);

    SpscRingBuffer<int> already_pow2(4);
    EXPECT_EQ(already_pow2.capacity(), 4u);
}

TEST(SpscRingBuffer, PushPopFifoOrder) {
    SpscRingBuffer<int> buffer(4);

    ASSERT_TRUE(buffer.try_push(1));
    ASSERT_TRUE(buffer.try_push(2));
    ASSERT_TRUE(buffer.try_push(3));
    ASSERT_TRUE(buffer.try_push(4));
    EXPECT_FALSE(buffer.try_push(5));
    EXPECT_TRUE(buffer.full());

    EXPECT_EQ(buffer.try_pop(), std::optional<int>{1});
    EXPECT_EQ(buffer.try_pop(), std::optional<int>{2});
    EXPECT_EQ(buffer.try_pop(), std::optional<int>{3});
    EXPECT_EQ(buffer.try_pop(), std::optional<int>{4});
    EXPECT_EQ(buffer.try_pop(), std::nullopt);
    EXPECT_TRUE(buffer.empty());
}

TEST(SpscRingBuffer, WrapAroundPreservesOrder) {
    SpscRingBuffer<int> buffer(2);

    ASSERT_TRUE(buffer.try_push(10));
    ASSERT_TRUE(buffer.try_push(20));
    EXPECT_EQ(buffer.try_pop(), std::optional<int>{10});
    ASSERT_TRUE(buffer.try_push(30));
    EXPECT_EQ(buffer.try_pop(), std::optional<int>{20});
    EXPECT_EQ(buffer.try_pop(), std::optional<int>{30});
    EXPECT_TRUE(buffer.empty());
}

TEST(SpscRingBuffer, MoveOnlyValues) {
    SpscRingBuffer<std::unique_ptr<int>> buffer(2);

    ASSERT_TRUE(buffer.try_push(std::make_unique<int>(7)));
    auto value = buffer.try_pop();
    ASSERT_TRUE(value.has_value());
    ASSERT_NE(value.value(), nullptr);
    EXPECT_EQ(*value.value(), 7);

    ASSERT_TRUE(buffer.try_push(std::move(value.value())));
    EXPECT_EQ(value.value(), nullptr);

    auto again = buffer.try_pop();
    ASSERT_TRUE(again.has_value());
    ASSERT_NE(again.value(), nullptr);
    EXPECT_EQ(*again.value(), 7);
}

TEST(SpscRingBuffer, RejectsPushWhenFull) {
    SpscRingBuffer<std::string> buffer(2);
    ASSERT_TRUE(buffer.try_push("a"));
    ASSERT_TRUE(buffer.try_push("b"));
    EXPECT_FALSE(buffer.try_push("c"));
    EXPECT_EQ(buffer.size(), 2u);
    EXPECT_EQ(buffer.try_pop(), std::optional<std::string>{"a"});
    ASSERT_TRUE(buffer.try_push("c"));
    EXPECT_EQ(buffer.try_pop(), std::optional<std::string>{"b"});
    EXPECT_EQ(buffer.try_pop(), std::optional<std::string>{"c"});
}

TEST(SpscRingBuffer, ConcurrentSingleProducerSingleConsumer) {
    constexpr std::size_t kCapacity = 64;
    // Values 0 .. (kItemCount - 1); sum = (kItemCount - 1) * kItemCount / 2
    constexpr std::uint64_t kItemCount = 100000;

    SpscRingBuffer<std::uint64_t> buffer(kCapacity);
    std::atomic<std::uint64_t> produced{0};
    std::atomic<std::uint64_t> consumed{0};
    std::atomic<std::uint64_t> checksum{0};
    std::atomic<bool> producer_done{false};

    std::thread producer([&] {
        for (std::uint64_t i = 0; i < kItemCount; ++i) {
            while (!buffer.try_push(i)) {
                std::this_thread::yield();
            }
            produced.fetch_add(1, std::memory_order_relaxed);
        }
        producer_done.store(true, std::memory_order_release);
    });

    std::thread consumer([&] {
        std::uint64_t expected = 0;
        while (true) {
            if (auto item = buffer.try_pop()) {
                EXPECT_EQ(*item, expected);
                checksum.fetch_add(*item, std::memory_order_relaxed);
                consumed.fetch_add(1, std::memory_order_relaxed);
                ++expected;
                continue;
            }
            if (producer_done.load(std::memory_order_acquire) && buffer.empty()) {
                break;
            }
            std::this_thread::yield();
        }
    });

    producer.join();
    consumer.join();

    EXPECT_EQ(produced.load(), kItemCount);
    EXPECT_EQ(consumed.load(), kItemCount);
    EXPECT_TRUE(buffer.empty());

    const std::uint64_t expected_sum = (kItemCount - 1) * kItemCount / 2;
    EXPECT_EQ(checksum.load(), expected_sum);
}
