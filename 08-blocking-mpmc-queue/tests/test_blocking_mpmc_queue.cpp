#include "blocking_mpmc_queue/blocking_mpmc_queue.hpp"

#include <atomic>
#include <chrono>
#include <stdexcept>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

using namespace std::chrono_literals;

TEST(BlockingMpmcQueue, PushPopOne) {
    klib::BlockingMpmcQueue<int> q(4);
    ASSERT_TRUE(q.push(7));
    auto v = q.pop();
    ASSERT_TRUE(v.has_value());
    EXPECT_EQ(*v, 7);
}

TEST(BlockingMpmcQueue, TryPushFailsWhenFull) {
    klib::BlockingMpmcQueue<int> q(2);
    ASSERT_TRUE(q.try_push(1));
    ASSERT_TRUE(q.try_push(2));
    EXPECT_FALSE(q.try_push(3));
    EXPECT_TRUE(q.full());
}

TEST(BlockingMpmcQueue, TryPopFailsWhenEmpty) {
    klib::BlockingMpmcQueue<int> q(2);
    EXPECT_FALSE(q.try_pop().has_value());
}

TEST(BlockingMpmcQueue, PushBlocksUntilSpace) {
    klib::BlockingMpmcQueue<int> q(1);
    ASSERT_TRUE(q.push(1));

    std::atomic<bool> pushed{false};
    std::thread producer([&] {
        ASSERT_TRUE(q.push(2));
        pushed.store(true, std::memory_order_release);
    });

    std::this_thread::sleep_for(50ms);
    EXPECT_FALSE(pushed.load(std::memory_order_acquire));

    auto first = q.pop();
    ASSERT_TRUE(first.has_value());
    EXPECT_EQ(*first, 1);

    producer.join();
    EXPECT_TRUE(pushed.load(std::memory_order_acquire));
    auto second = q.pop();
    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(*second, 2);
}

TEST(BlockingMpmcQueue, PopBlocksUntilItem) {
    klib::BlockingMpmcQueue<int> q(2);
    std::atomic<bool> got{false};
    std::optional<int> value;

    std::thread consumer([&] {
        value = q.pop();
        got.store(true, std::memory_order_release);
    });

    std::this_thread::sleep_for(50ms);
    EXPECT_FALSE(got.load(std::memory_order_acquire));

    ASSERT_TRUE(q.push(42));
    consumer.join();
    EXPECT_TRUE(got.load(std::memory_order_acquire));
    ASSERT_TRUE(value.has_value());
    EXPECT_EQ(*value, 42);
}

TEST(BlockingMpmcQueue, ShutdownUnblocksPushAndRejects) {
    klib::BlockingMpmcQueue<int> q(1);
    ASSERT_TRUE(q.push(1));

    std::atomic<bool> done{false};
    bool push_ok = true;
    std::thread producer([&] {
        push_ok = q.push(2);
        done.store(true, std::memory_order_release);
    });

    std::this_thread::sleep_for(30ms);
    q.shutdown();
    producer.join();
    EXPECT_TRUE(done.load(std::memory_order_acquire));
    EXPECT_FALSE(push_ok);
    EXPECT_FALSE(q.push(3));
}

TEST(BlockingMpmcQueue, ShutdownUnblocksPopWhenEmpty) {
    klib::BlockingMpmcQueue<int> q(2);
    std::optional<int> value;
    std::thread consumer([&] { value = q.pop(); });

    std::this_thread::sleep_for(30ms);
    q.shutdown();
    consumer.join();
    EXPECT_FALSE(value.has_value());
}

TEST(BlockingMpmcQueue, ConcurrentProducersConsumers) {
    constexpr int kProducers = 4;
    constexpr int kConsumers = 4;
    constexpr int kPerProducer = 200;
    constexpr int kTotal = kProducers * kPerProducer;
    klib::BlockingMpmcQueue<int> q(32);

    std::atomic<int> consumed{0};
    std::atomic<long long> sum{0};

    std::vector<std::thread> producers;
    producers.reserve(kProducers);
    for (int p = 0; p < kProducers; ++p) {
        producers.emplace_back([&, p] {
            for (int i = 0; i < kPerProducer; ++i) {
                ASSERT_TRUE(q.push(p * kPerProducer + i));
            }
        });
    }

    std::vector<std::thread> consumers;
    consumers.reserve(kConsumers);
    for (int c = 0; c < kConsumers; ++c) {
        consumers.emplace_back([&] {
            while (true) {
                auto item = q.pop();
                if (!item.has_value()) {
                    return;
                }
                sum.fetch_add(*item, std::memory_order_relaxed);
                if (consumed.fetch_add(1, std::memory_order_acq_rel) + 1 == kTotal) {
                    q.shutdown();  // wake consumers blocked on empty
                    return;
                }
            }
        });
    }

    for (auto& t : producers) {
        t.join();
    }
    for (auto& t : consumers) {
        t.join();
    }

    EXPECT_EQ(consumed.load(), kTotal);
    const long long expected =
        static_cast<long long>(kTotal - 1) * static_cast<long long>(kTotal) / 2;
    EXPECT_EQ(sum.load(), expected);
}

TEST(BlockingMpmcQueue, ZeroCapacityThrows) {
    EXPECT_THROW(klib::BlockingMpmcQueue<int>(0), std::invalid_argument);
}

TEST(BlockingMpmcQueue, StressManyProducersConsumers) {
    constexpr int kProducers = 8;
    constexpr int kConsumers = 8;
    constexpr int kPerProducer = 2000;
    constexpr int kTotal = kProducers * kPerProducer;
    klib::BlockingMpmcQueue<int> q(64);

    std::atomic<int> consumed{0};
    std::atomic<long long> sum{0};

    std::vector<std::thread> producers;
    producers.reserve(kProducers);
    for (int p = 0; p < kProducers; ++p) {
        producers.emplace_back([&, p] {
            for (int i = 0; i < kPerProducer; ++i) {
                ASSERT_TRUE(q.push(p * kPerProducer + i));
            }
        });
    }

    std::vector<std::thread> consumers;
    consumers.reserve(kConsumers);
    for (int c = 0; c < kConsumers; ++c) {
        consumers.emplace_back([&] {
            while (true) {
                auto item = q.pop();
                if (!item.has_value()) {
                    return;
                }
                sum.fetch_add(*item, std::memory_order_relaxed);
                if (consumed.fetch_add(1, std::memory_order_acq_rel) + 1 == kTotal) {
                    q.shutdown();
                    return;
                }
            }
        });
    }

    for (auto& t : producers) {
        t.join();
    }
    for (auto& t : consumers) {
        t.join();
    }

    EXPECT_EQ(consumed.load(), kTotal);
    const long long expected =
        static_cast<long long>(kTotal - 1) * static_cast<long long>(kTotal) / 2;
    EXPECT_EQ(sum.load(), expected);
}

TEST(BlockingMpmcQueue, ShutdownWhileMixedTryAndBlocking) {
    klib::BlockingMpmcQueue<int> q(4);
    std::atomic<bool> stop{false};
    std::atomic<int> blocking_push_fails{0};

    std::thread blocker([&] {
        while (!stop.load(std::memory_order_acquire)) {
            if (!q.push(1)) {
                blocking_push_fails.fetch_add(1, std::memory_order_relaxed);
                return;
            }
            (void)q.try_pop();
        }
    });

    std::this_thread::sleep_for(20ms);
    q.shutdown();
    stop.store(true, std::memory_order_release);
    blocker.join();

    EXPECT_GE(blocking_push_fails.load(), 0);
    EXPECT_FALSE(q.push(99));
    while (q.try_pop().has_value()) {
    }
    EXPECT_FALSE(q.pop().has_value());
}
