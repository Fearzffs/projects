#include "timer_scheduler/timer_scheduler.hpp"

#include <chrono>
#include <condition_variable>
#include <mutex>
#include <stdexcept>
#include <thread>

#include <gtest/gtest.h>

using namespace std::chrono_literals;

namespace {

class WaitCounter {
public:
    void bump() {
        {
            std::lock_guard lock(mutex_);
            ++count_;
        }
        cv_.notify_all();
    }

    bool wait_for_at_least(int n, std::chrono::milliseconds timeout = 500ms) {
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

TEST(TimerScheduler, RunAfterFiresOnceViaPool) {
    portfolio::ThreadPool pool(2);
    portfolio::TimerScheduler timers(pool);
    WaitCounter hits;

    auto handle = timers.try_run_after(20ms, [&hits] { hits.bump(); });
    ASSERT_TRUE(handle);
    ASSERT_TRUE(hits.wait_for_at_least(1));
    EXPECT_EQ(hits.count(), 1);

    // One-shot should not keep firing.
    EXPECT_FALSE(hits.wait_for_at_least(2, 80ms));
    EXPECT_EQ(hits.count(), 1);

    handle.reset();
    timers.shutdown();
    pool.shutdown();
}

TEST(TimerScheduler, CancelPreventsFire) {
    portfolio::ThreadPool pool(2);
    portfolio::TimerScheduler timers(pool);
    WaitCounter hits;

    auto handle = timers.try_run_after(100ms, [&hits] { hits.bump(); });
    ASSERT_TRUE(handle);
    handle.reset();  // cancel

    EXPECT_FALSE(hits.wait_for_at_least(1, 200ms));
    EXPECT_EQ(hits.count(), 0);

    timers.shutdown();
    pool.shutdown();
}

TEST(TimerScheduler, RunEveryFiresUntilCancelled) {
    portfolio::ThreadPool pool(2);
    portfolio::TimerScheduler timers(pool);
    WaitCounter hits;

    auto handle = timers.try_run_every(30ms, [&hits] { hits.bump(); });
    ASSERT_TRUE(handle);
    ASSERT_TRUE(hits.wait_for_at_least(3, 500ms));
    handle.reset();

    const int after_cancel = hits.count();
    // Give a couple periods; count should stabilize (at most one in-flight).
    std::this_thread::sleep_for(100ms);
    EXPECT_LE(hits.count() - after_cancel, 1);

    timers.shutdown();
    pool.shutdown();
}

TEST(TimerScheduler, ShutdownRejectsNewTimers) {
    portfolio::ThreadPool pool(1);
    portfolio::TimerScheduler timers(pool);
    timers.shutdown();

    auto handle = timers.try_run_after(10ms, [] {});
    EXPECT_FALSE(handle);
    pool.shutdown();
}

TEST(TimerScheduler, ZeroPeriodThrows) {
    portfolio::ThreadPool pool(1);
    portfolio::TimerScheduler timers(pool);
    EXPECT_THROW(timers.try_run_every(0ms, [] {}), std::invalid_argument);
    timers.shutdown();
    pool.shutdown();
}

TEST(TimerScheduler, EarlierTimerPreemptsWait) {
    portfolio::ThreadPool pool(2);
    portfolio::TimerScheduler timers(pool);
    WaitCounter hits;

    // Long timer first — occupies the wait_until.
    auto late = timers.try_run_after(300ms, [&hits] { hits.bump(); });
    ASSERT_TRUE(late);

    // Short timer should still fire soon (notify re-evaluates heap).
    auto early = timers.try_run_after(20ms, [&hits] { hits.bump(); });
    ASSERT_TRUE(early);

    ASSERT_TRUE(hits.wait_for_at_least(1, 200ms));
    early.reset();
    late.reset();
    timers.shutdown();
    pool.shutdown();
}
