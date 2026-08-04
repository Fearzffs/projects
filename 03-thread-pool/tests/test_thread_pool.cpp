#include "thread_pool/thread_pool.hpp"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <stdexcept>
#include <thread>

#include <gtest/gtest.h>

using portfolio::ThreadPool;

TEST(ThreadPool, RejectsZeroThreadCount) {
    EXPECT_THROW(ThreadPool(0), std::invalid_argument);
}

TEST(ThreadPool, RejectsZeroMaxQueueSize) {
    EXPECT_THROW(ThreadPool(1, 0), std::invalid_argument);
}

TEST(ThreadPool, RunsWorkAndCompletionCallback) {
    ThreadPool pool(2);
    std::atomic<bool> work_ran{false};
    std::atomic<bool> done_saw_work{false};
    std::atomic<int> done_count{0};

    ASSERT_TRUE(pool.try_submit(
        [&] { work_ran.store(true, std::memory_order_release); },
        [&] {
            done_saw_work.store(work_ran.load(std::memory_order_acquire),
                                std::memory_order_relaxed);
            done_count.fetch_add(1, std::memory_order_relaxed);
        }));

    // Drain + join: waits until work and on_done have finished (not just queue empty).
    pool.shutdown();

    EXPECT_TRUE(work_ran.load());
    EXPECT_TRUE(done_saw_work.load());  // on_done ran after work set the flag
    EXPECT_EQ(done_count.load(), 1);
}

TEST(ThreadPool, ManyTasksComplete) {
    constexpr int kTasks = 1000;
    ThreadPool pool(4);
    std::atomic<int> work_count{0};
    std::atomic<int> done_count{0};

    for (int i = 0; i < kTasks; ++i) {
        while (!pool.try_submit(
            [&] { work_count.fetch_add(1, std::memory_order_relaxed); },
            [&] { done_count.fetch_add(1, std::memory_order_relaxed); })) {
            std::this_thread::yield();
        }
    }

    pool.shutdown();

    EXPECT_EQ(work_count.load(), kTasks);
    EXPECT_EQ(done_count.load(), kTasks);
    EXPECT_EQ(pool.queued_tasks(), 0u);
}

TEST(ThreadPool, RejectsSubmitWhenQueueFull) {
    ThreadPool pool(1, /*max_queue_size=*/1);
    std::atomic<bool> release_worker{false};
    std::mutex started_mutex;
    std::condition_variable started_cv;
    bool worker_started = false;

    // Occupy the single worker so the next task stays queued.
    ASSERT_TRUE(pool.try_submit([&] {
        {
            std::lock_guard lock(started_mutex);
            worker_started = true;
        }
        started_cv.notify_one();
        while (!release_worker.load(std::memory_order_acquire)) {
            std::this_thread::yield();
        }
    }));

    {
        std::unique_lock lock(started_mutex);
        started_cv.wait(lock, [&] { return worker_started; });
    }

    // Fill the one-slot queue.
    ASSERT_TRUE(pool.try_submit([] {}));
    EXPECT_EQ(pool.queued_tasks(), 1u);
    EXPECT_FALSE(pool.try_submit([] {}));

    release_worker.store(true, std::memory_order_release);
    pool.shutdown();
}

TEST(ThreadPool, RejectsSubmitAfterShutdown) {
    ThreadPool pool(2);
    pool.shutdown();
    EXPECT_FALSE(pool.try_submit([] {}));
}

TEST(ThreadPool, WorkExceptionStillInvokesOnDone) {
    ThreadPool pool(1);
    std::atomic<bool> done{false};

    ASSERT_TRUE(pool.try_submit(
        [] { throw std::runtime_error("boom"); },
        [&] { done.store(true, std::memory_order_release); }));

    pool.shutdown();
    EXPECT_TRUE(done.load(std::memory_order_acquire));
}

TEST(ThreadPool, NestedSubmitFromCallback) {
    ThreadPool pool(2);
    std::mutex done_mutex;
    std::condition_variable done_cv;
    int done_count = 0;

    ASSERT_TRUE(pool.try_submit(
        [] {},
        [&] {
            EXPECT_TRUE(pool.try_submit(
                [] {},
                [&] {
                    {
                        std::lock_guard lock(done_mutex);
                        ++done_count;
                    }
                    done_cv.notify_one();
                }));
            {
                std::lock_guard lock(done_mutex);
                ++done_count;
            }
            done_cv.notify_one();
        }));

    // Same pattern as production: on_done signals; test waits without sleep-polling.
    // Must reach 2 *before* shutdown — otherwise stopping_ rejects the nested submit.
    {
        std::unique_lock lock(done_mutex);
        done_cv.wait(lock, [&] { return done_count == 2; });
    }
    pool.shutdown();
    EXPECT_EQ(done_count, 2);
}

TEST(ThreadPool, ThreadCountAccessor) {
    ThreadPool pool(3);
    EXPECT_EQ(pool.thread_count(), 3u);
    EXPECT_EQ(pool.max_queue_size(), ThreadPool::kDefaultMaxQueueSize);
}
