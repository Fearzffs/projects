#pragma once

#include "thread_pool/thread_pool.hpp"

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <stdexcept>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace klib {
namespace detail {

struct TimerSchedulerState {
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;
    using Callback = std::function<void()>;

    struct Entry {
        std::uint64_t id{0};
        TimePoint deadline{};
        std::optional<std::chrono::milliseconds> period;
        Callback callback;
        bool cancelled{false};
    };

    struct HeapItem {
        TimePoint deadline;
        std::uint64_t id;

        bool operator>(const HeapItem& other) const {
            if (deadline != other.deadline) {
                return deadline > other.deadline;
            }
            return id > other.id;
        }
    };

    explicit TimerSchedulerState(ThreadPool& pool) : pool_(pool) {}

    ThreadPool& pool() noexcept { return pool_; }

    std::optional<std::uint64_t> schedule(std::chrono::milliseconds delay,
                                          std::optional<std::chrono::milliseconds> period,
                                          Callback callback) {
        if (period.has_value() && period->count() <= 0) {
            throw std::invalid_argument("TimerScheduler period must be > 0");
        }
        if (delay.count() < 0) {
            throw std::invalid_argument("TimerScheduler delay must be >= 0");
        }

        std::uint64_t id = 0;
        {
            std::lock_guard lock(mutex_);
            if (stopping_) {
                return std::nullopt;
            }
            id = next_id_++;
            Entry entry;
            entry.id = id;
            entry.deadline = Clock::now() + delay;
            entry.period = period;
            entry.callback = std::move(callback);
            entries_.emplace(id, std::move(entry));
            heap_.push(HeapItem{entries_.at(id).deadline, id});
        }
        cv_.notify_one();
        return id;
    }

    void cancel(std::uint64_t id) {
        std::lock_guard lock(mutex_);
        auto it = entries_.find(id);
        if (it == entries_.end()) {
            return;
        }
        it->second.cancelled = true;
        // Lazy removal from heap when the id is popped.
    }

    void clear() {
        std::lock_guard lock(mutex_);
        for (auto& [id, entry] : entries_) {
            (void)id;
            entry.cancelled = true;
        }
        entries_.clear();
        heap_ = {};
    }

    void request_stop() {
        {
            std::lock_guard lock(mutex_);
            stopping_ = true;
        }
        cv_.notify_all();
    }

    void run_loop() {
        std::unique_lock lock(mutex_);
        while (!stopping_) {
            if (heap_.empty()) {
                cv_.wait(lock, [this] { return stopping_ || !heap_.empty(); });
                continue;
            }

            // No predicate: schedule() may notify with an earlier deadline; we
            // must re-read heap_.top() after every wake.
            cv_.wait_until(lock, heap_.top().deadline);
            if (stopping_) {
                break;
            }

            const auto now = Clock::now();
            std::vector<Callback> due;
            while (!heap_.empty() && heap_.top().deadline <= now) {
                const auto item = heap_.top();
                heap_.pop();

                auto it = entries_.find(item.id);
                if (it == entries_.end()) {
                    continue;
                }
                if (it->second.cancelled || it->second.deadline != item.deadline) {
                    // Cancelled or stale heap node after a reschedule.
                    if (it->second.cancelled) {
                        entries_.erase(it);
                    }
                    continue;
                }

                due.push_back(it->second.callback);

                if (it->second.period.has_value()) {
                    it->second.deadline = now + *it->second.period;
                    heap_.push(HeapItem{it->second.deadline, it->second.id});
                } else {
                    entries_.erase(it);
                }
            }

            lock.unlock();
            for (auto& cb : due) {
                // Fire on the pool — never run user work on the timer thread.
                (void)pool_.try_submit(std::move(cb));
            }
            lock.lock();
        }
        entries_.clear();
        heap_ = {};
    }

private:
    ThreadPool& pool_;
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    bool stopping_{false};
    std::uint64_t next_id_{1};
    std::unordered_map<std::uint64_t, Entry> entries_;
    std::priority_queue<HeapItem, std::vector<HeapItem>, std::greater<HeapItem>> heap_;
};

}  // namespace detail

/// RAII handle: destroying or reset() cancels the timer.
class TimerHandle {
public:
    TimerHandle() = default;

    TimerHandle(const TimerHandle&) = delete;
    TimerHandle& operator=(const TimerHandle&) = delete;

    TimerHandle(TimerHandle&& other) noexcept
        : state_(std::move(other.state_)), id_(other.id_) {
        other.id_ = 0;
    }

    TimerHandle& operator=(TimerHandle&& other) noexcept {
        if (this != &other) {
            reset();
            state_ = std::move(other.state_);
            id_ = other.id_;
            other.id_ = 0;
        }
        return *this;
    }

    ~TimerHandle() { reset(); }

    void reset() {
        if (id_ == 0) {
            return;
        }
        if (auto state = state_.lock()) {
            state->cancel(id_);
        }
        id_ = 0;
        state_.reset();
    }

    [[nodiscard]] explicit operator bool() const noexcept { return id_ != 0; }

private:
    friend class TimerScheduler;

    TimerHandle(std::weak_ptr<detail::TimerSchedulerState> state, std::uint64_t id)
        : state_(std::move(state)), id_(id) {}

    std::weak_ptr<detail::TimerSchedulerState> state_;
    std::uint64_t id_{0};
};

/// Schedules callbacks to run later / periodically via a ThreadPool.
///
/// Design notes:
/// - ThreadPool is non-owning and must outlive the TimerScheduler.
/// - try_run_after / try_run_every never block the caller; they return an empty
///   handle if the scheduler is shutting down.
/// - A dedicated timer thread waits until the next deadline, then try_submit
///   the callback to the pool (user work never runs on the timer thread).
/// - TimerHandle is RAII cancel; one-shot timers auto-complete without cancel.
/// - shutdown() stops accepting schedules, drops pending timers, joins the
///   timer thread. Does not shut down the pool.
class TimerScheduler {
public:
    using Callback = detail::TimerSchedulerState::Callback;

    explicit TimerScheduler(ThreadPool& pool)
        : state_(std::make_shared<detail::TimerSchedulerState>(pool)),
          worker_([state = state_] { state->run_loop(); }) {}

    TimerScheduler(const TimerScheduler&) = delete;
    TimerScheduler& operator=(const TimerScheduler&) = delete;
    TimerScheduler(TimerScheduler&&) = delete;
    TimerScheduler& operator=(TimerScheduler&&) = delete;

    ~TimerScheduler() { shutdown(); }

    /// Fire once after `delay` (may be zero). Empty handle if shutting down.
    [[nodiscard]] TimerHandle try_run_after(std::chrono::milliseconds delay, Callback cb) {
        auto id = state_->schedule(delay, std::nullopt, std::move(cb));
        if (!id) {
            return {};
        }
        return TimerHandle(state_, *id);
    }

    /// Fire every `period` (> 0), first shot after one period. Empty if stopping.
    [[nodiscard]] TimerHandle try_run_every(std::chrono::milliseconds period, Callback cb) {
        auto id = state_->schedule(period, period, std::move(cb));
        if (!id) {
            return {};
        }
        return TimerHandle(state_, *id);
    }

    /// Stop accepting timers, drop pending ones, join the timer thread.
    void shutdown() {
        if (!state_) {
            return;
        }
        state_->request_stop();
        if (worker_.joinable()) {
            worker_.join();
        }
    }

private:
    std::shared_ptr<detail::TimerSchedulerState> state_;
    std::thread worker_;
};

}  // namespace klib
