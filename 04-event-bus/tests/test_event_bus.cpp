#include "event_bus/event_bus.hpp"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

using portfolio::EventBus;
using portfolio::ThreadPool;

namespace {

struct Ping {
    int value{0};
};

struct Pong {
    int value{0};
};

}  // namespace

TEST(EventBus, PublishDeliversToTypedSubscriber) {
    ThreadPool pool(2);
    EventBus bus(pool);
    std::atomic<int> seen{0};

    auto sub = bus.subscribe<Ping>([&](const Ping& e) {
        seen.store(e.value, std::memory_order_relaxed);
    });

    ASSERT_TRUE(bus.publish(Ping{7}));
    pool.shutdown();

    EXPECT_EQ(seen.load(), 7);
    EXPECT_EQ(bus.subscriber_count(), 1u);
}

TEST(EventBus, MultipleSubscribersSameType) {
    ThreadPool pool(4);
    EventBus bus(pool);
    std::atomic<int> hits{0};

    auto a = bus.subscribe<Ping>([&](const Ping&) {
        hits.fetch_add(1, std::memory_order_relaxed);
    });
    auto b = bus.subscribe<Ping>([&](const Ping&) {
        hits.fetch_add(1, std::memory_order_relaxed);
    });

    ASSERT_TRUE(bus.publish(Ping{1}));
    pool.shutdown();

    EXPECT_EQ(hits.load(), 2);
}

TEST(EventBus, DifferentTypesDoNotCrossDeliver) {
    ThreadPool pool(2);
    EventBus bus(pool);
    std::atomic<int> ping_hits{0};
    std::atomic<int> pong_hits{0};

    auto ping_sub = bus.subscribe<Ping>([&](const Ping&) {
        ping_hits.fetch_add(1, std::memory_order_relaxed);
    });
    auto pong_sub = bus.subscribe<Pong>([&](const Pong&) {
        pong_hits.fetch_add(1, std::memory_order_relaxed);
    });

    ASSERT_TRUE(bus.publish(Ping{1}));
    pool.shutdown();

    EXPECT_EQ(ping_hits.load(), 1);
    EXPECT_EQ(pong_hits.load(), 0);
}

TEST(EventBus, SubscriptionResetUnsubscribes) {
    ThreadPool pool(2);
    EventBus bus(pool);
    std::atomic<int> hits{0};

    auto sub = bus.subscribe<Ping>([&](const Ping&) {
        hits.fetch_add(1, std::memory_order_relaxed);
    });
    sub.reset();
    EXPECT_EQ(bus.subscriber_count(), 0u);

    ASSERT_TRUE(bus.publish(Ping{1}));
    pool.shutdown();

    EXPECT_EQ(hits.load(), 0);
}

TEST(EventBus, PublishWithNoSubscribersReturnsTrue) {
    ThreadPool pool(1);
    EventBus bus(pool);
    EXPECT_TRUE(bus.publish(Ping{1}));
    pool.shutdown();
}

TEST(EventBus, PublishReturnsFalseWhenPoolQueueFull) {
    // One worker blocked + queue size 1 → further submit fails.
    ThreadPool pool(1, /*max_queue_size=*/1);
    EventBus bus(pool);

    std::atomic<bool> release_worker{false};
    std::mutex started_mutex;
    std::condition_variable started_cv;
    bool worker_started = false;

    auto blocker = bus.subscribe<Ping>([&](const Ping&) {
        {
            std::lock_guard lock(started_mutex);
            worker_started = true;
        }
        started_cv.notify_one();
        while (!release_worker.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }
    });

    ASSERT_TRUE(bus.publish(Ping{1}));  // occupies worker
    {
        std::unique_lock lock(started_mutex);
        started_cv.wait(lock, [&] { return worker_started; });
    }

    // Fill the single queue slot with a second publish (one subscriber → one task).
    ASSERT_TRUE(bus.publish(Ping{2}));
    // Next publish cannot queue → false.
    EXPECT_FALSE(bus.publish(Ping{3}));

    release_worker.store(true, std::memory_order_release);
    pool.shutdown();
}

TEST(EventBus, ConcurrentSubscribeAndPublish) {
    ThreadPool pool(4, /*max_queue_size=*/4096);
    EventBus bus(pool);
    std::atomic<int> hits{0};
    std::vector<portfolio::Subscription> subs;
    subs.reserve(8);

    for (int i = 0; i < 8; ++i) {
        subs.push_back(bus.subscribe<Ping>([&](const Ping&) {
            hits.fetch_add(1, std::memory_order_relaxed);
        }));
    }

    std::vector<std::thread> publishers;
    publishers.reserve(4);
    for (int p = 0; p < 4; ++p) {
        publishers.emplace_back([&] {
            for (int i = 0; i < 50; ++i) {
                while (!bus.publish(Ping{i})) {
                    std::this_thread::yield();
                }
            }
        });
    }
    for (auto& t : publishers) {
        t.join();
    }

    pool.shutdown();
    EXPECT_EQ(hits.load(), 8 * 4 * 50);
}
