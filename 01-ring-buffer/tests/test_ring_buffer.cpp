#include "ring_buffer/ring_buffer.hpp"

#include <atomic>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

using portfolio::RingBuffer;

TEST(RingBuffer, RejectsZeroCapacity) {
    EXPECT_THROW(RingBuffer<int>(0), std::invalid_argument);
}

TEST(RingBuffer, PushPopFifoOrder) {
    RingBuffer<int> buffer(3);

    ASSERT_TRUE(buffer.try_push(1));
    ASSERT_TRUE(buffer.try_push(2));
    ASSERT_TRUE(buffer.try_push(3));
    EXPECT_FALSE(buffer.try_push(4));
    EXPECT_TRUE(buffer.full());

    EXPECT_EQ(buffer.try_pop(), std::optional<int>{1});
    EXPECT_EQ(buffer.try_pop(), std::optional<int>{2});
    EXPECT_EQ(buffer.try_pop(), std::optional<int>{3});
    EXPECT_EQ(buffer.try_pop(), std::nullopt);
    EXPECT_TRUE(buffer.empty());
}

TEST(RingBuffer, WrapAroundPreservesOrder) {
    RingBuffer<int> buffer(2);

    ASSERT_TRUE(buffer.try_push(10));
    ASSERT_TRUE(buffer.try_push(20));
    EXPECT_EQ(buffer.try_pop(), std::optional<int>{10});
    ASSERT_TRUE(buffer.try_push(30));
    EXPECT_EQ(buffer.try_pop(), std::optional<int>{20});
    EXPECT_EQ(buffer.try_pop(), std::optional<int>{30});
    EXPECT_TRUE(buffer.empty());
}

TEST(RingBuffer, MoveOnlyValues) {
    RingBuffer<std::unique_ptr<int>> buffer(2);

    ASSERT_TRUE(buffer.try_push(std::make_unique<int>(7)));
    auto value = buffer.try_pop();
    ASSERT_TRUE(value.has_value());
    ASSERT_NE(value.value(), nullptr);
    EXPECT_EQ(*value.value(), 7);
}

TEST(RingBuffer, ClearEmptiesBuffer) {
    RingBuffer<std::string> buffer(4);
    ASSERT_TRUE(buffer.try_push("a"));
    ASSERT_TRUE(buffer.try_push("b"));
    buffer.clear();
    EXPECT_TRUE(buffer.empty());
    EXPECT_EQ(buffer.size(), 0u);
    EXPECT_EQ(buffer.try_pop(), std::nullopt);
}

TEST(RingBuffer, ConcurrentProducersConsumers) {
    constexpr std::size_t kCapacity = 64;
    constexpr int kItemsPerProducer = 1000;
    constexpr int kProducers = 4;
    constexpr int kConsumers = 4;

    RingBuffer<int> buffer(kCapacity);
    std::atomic<int> produced{0};
    std::atomic<int> consumed{0};
    std::atomic<bool> producers_done{false};

    std::vector<std::thread> producers;
    producers.reserve(kProducers);
    for (int p = 0; p < kProducers; ++p) {
        producers.emplace_back([&, p] {
            for (int i = 0; i < kItemsPerProducer; ++i) {
                const int value = p * kItemsPerProducer + i;
                while (!buffer.try_push(value)) {
                    std::this_thread::yield();
                }
                produced.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    std::vector<std::thread> consumers;
    consumers.reserve(kConsumers);
    for (int c = 0; c < kConsumers; ++c) {
        consumers.emplace_back([&] {
            while (true) {
                if (auto item = buffer.try_pop()) {
                    consumed.fetch_add(1, std::memory_order_relaxed);
                    continue;
                }
                if (producers_done.load(std::memory_order_acquire) &&
                    buffer.empty()) {
                    break;
                }
                std::this_thread::yield();
            }
        });
    }

    for (auto& t : producers) {
        t.join();
    }
    producers_done.store(true, std::memory_order_release);

    for (auto& t : consumers) {
        t.join();
    }

    EXPECT_EQ(produced.load(), kProducers * kItemsPerProducer);
    EXPECT_EQ(consumed.load(), kProducers * kItemsPerProducer);
    EXPECT_TRUE(buffer.empty());
}
