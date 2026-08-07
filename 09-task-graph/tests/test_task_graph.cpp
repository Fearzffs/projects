#include "task_graph/task_graph.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>

using namespace std::chrono_literals;

namespace {

class Gate {
public:
    void arrive() {
        {
            std::lock_guard lock(mutex_);
            ++count_;
        }
        cv_.notify_all();
    }

    bool wait_for(int n, std::chrono::milliseconds timeout = 500ms) {
        std::unique_lock lock(mutex_);
        return cv_.wait_for(lock, timeout, [&] { return count_ >= n; });
    }

    int count() const {
        std::lock_guard lock(mutex_);
        return count_;
    }

private:
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    int count_{0};
};

}  // namespace

TEST(TaskGraph, IndependentTasksAllRun) {
    portfolio::ThreadPool pool(4);
    portfolio::TaskGraph graph(pool);
    std::atomic<int> hits{0};

    for (int i = 0; i < 5; ++i) {
        (void)graph.add([&hits] { hits.fetch_add(1, std::memory_order_relaxed); });
    }

    Gate done;
    graph.set_on_complete([&done] { done.arrive(); });
    ASSERT_TRUE(graph.try_run());
    ASSERT_TRUE(done.wait_for(1));
    EXPECT_EQ(hits.load(), 5);
    pool.shutdown();
}

TEST(TaskGraph, PrecedeEnforcesOrder) {
    portfolio::ThreadPool pool(2);
    portfolio::TaskGraph graph(pool);

    std::mutex order_mutex;
    std::vector<int> order;

    auto a = graph.add([&] {
        std::lock_guard lock(order_mutex);
        order.push_back(1);
    });
    auto b = graph.add([&] {
        std::lock_guard lock(order_mutex);
        order.push_back(2);
    });
    graph.precede(a, b);

    Gate done;
    graph.set_on_complete([&done] { done.arrive(); });
    ASSERT_TRUE(graph.try_run());
    ASSERT_TRUE(done.wait_for(1));
    ASSERT_EQ(order.size(), 2u);
    EXPECT_EQ(order[0], 1);
    EXPECT_EQ(order[1], 2);
    pool.shutdown();
}

TEST(TaskGraph, DiamondDependencies) {
    portfolio::ThreadPool pool(4);
    portfolio::TaskGraph graph(pool);

    std::atomic<int> phase{0};
    std::atomic<bool> d_ok{true};

    auto a = graph.add([&] { phase.store(1, std::memory_order_release); });
    auto b = graph.add([&] {
        if (phase.load(std::memory_order_acquire) < 1) {
            d_ok.store(false, std::memory_order_relaxed);
        }
        phase.fetch_or(2, std::memory_order_acq_rel);
    });
    auto c = graph.add([&] {
        if (phase.load(std::memory_order_acquire) < 1) {
            d_ok.store(false, std::memory_order_relaxed);
        }
        phase.fetch_or(4, std::memory_order_acq_rel);
    });
    auto d = graph.add([&] {
        const int p = phase.load(std::memory_order_acquire);
        if ((p & 2) == 0 || (p & 4) == 0) {
            d_ok.store(false, std::memory_order_relaxed);
        }
    });

    graph.precede(a, b);
    graph.precede(a, c);
    graph.precede(b, d);
    graph.precede(c, d);

    Gate done;
    graph.set_on_complete([&done] { done.arrive(); });
    ASSERT_TRUE(graph.try_run());
    ASSERT_TRUE(done.wait_for(1));
    EXPECT_TRUE(d_ok.load());
    pool.shutdown();
}

TEST(TaskGraph, EmptyGraphCompletes) {
    portfolio::ThreadPool pool(1);
    portfolio::TaskGraph graph(pool);
    Gate done;
    graph.set_on_complete([&done] { done.arrive(); });
    ASSERT_TRUE(graph.try_run());
    ASSERT_TRUE(done.wait_for(1, 100ms));
    pool.shutdown();
}

TEST(TaskGraph, AddAfterRunThrows) {
    portfolio::ThreadPool pool(1);
    portfolio::TaskGraph graph(pool);
    (void)graph.add([] {});
    ASSERT_TRUE(graph.try_run());
    graph.wait();
    EXPECT_THROW((void)graph.add([] {}), std::logic_error);
    pool.shutdown();
}

TEST(TaskGraph, SelfPrecedeThrows) {
    portfolio::ThreadPool pool(1);
    portfolio::TaskGraph graph(pool);
    auto a = graph.add([] {});
    EXPECT_THROW(graph.precede(a, a), std::invalid_argument);
    pool.shutdown();
}

TEST(TaskGraph, WaitJoinsWithoutOnComplete) {
    portfolio::ThreadPool pool(2);
    portfolio::TaskGraph graph(pool);
    std::atomic<int> hits{0};
    auto a = graph.add([&] { hits.fetch_add(1, std::memory_order_relaxed); });
    auto b = graph.add([&] { hits.fetch_add(1, std::memory_order_relaxed); });
    graph.precede(a, b);
    ASSERT_TRUE(graph.try_run());
    graph.wait();
    EXPECT_EQ(hits.load(), 2);
    pool.shutdown();
}

TEST(TaskGraph, OnDoneRunsAfterWorkBeforeSuccessor) {
    portfolio::ThreadPool pool(2);
    portfolio::TaskGraph graph(pool);

    std::mutex order_mutex;
    std::vector<int> order;

    auto a = graph.add(
        [&] {
            std::lock_guard lock(order_mutex);
            order.push_back(1);  // work
        },
        [&] {
            std::lock_guard lock(order_mutex);
            order.push_back(2);  // on_done
        });
    auto b = graph.add([&] {
        std::lock_guard lock(order_mutex);
        order.push_back(3);  // successor
    });
    graph.precede(a, b);

    ASSERT_TRUE(graph.try_run());
    graph.wait();
    ASSERT_EQ(order.size(), 3u);
    EXPECT_EQ(order[0], 1);
    EXPECT_EQ(order[1], 2);
    EXPECT_EQ(order[2], 3);
    pool.shutdown();
}

TEST(TaskGraph, OnDoneAndOnCompleteBothFire) {
    portfolio::ThreadPool pool(2);
    portfolio::TaskGraph graph(pool);
    std::atomic<int> per_task{0};
    Gate done;

    (void)graph.add([] {}, [&] { per_task.fetch_add(1, std::memory_order_relaxed); });
    (void)graph.add([] {}, [&] { per_task.fetch_add(1, std::memory_order_relaxed); });
    graph.set_on_complete([&done] { done.arrive(); });

    ASSERT_TRUE(graph.try_run());
    ASSERT_TRUE(done.wait_for(1));
    EXPECT_EQ(per_task.load(), 2);
    pool.shutdown();
}
