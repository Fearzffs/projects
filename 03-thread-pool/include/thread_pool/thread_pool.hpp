#pragma once

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>

namespace klib {

/// Fixed-size thread pool with non-blocking submit and completion callbacks.
///
/// Design notes:
/// - try_submit never blocks the caller; returns false if the queue is full or
///   the pool is shutting down.
/// - Optional on_done runs on a *worker thread* after work finishes (your
///   "task completed" notification). To wake the main thread, set your own
///   flag/event/cv inside on_done — the pool does not block anyone waiting.
/// - Exceptions from work are caught and discarded; on_done still runs.
/// - Exceptions from on_done are also caught and discarded (keeps workers alive).
/// - shutdown() stops accepting work, drains the queue, then joins workers.
/// - Destructor calls shutdown() if not already shut down.
class ThreadPool {
public:
    static constexpr std::size_t kDefaultMaxQueueSize = 1024;

    explicit ThreadPool(std::size_t thread_count,
                        std::size_t max_queue_size = kDefaultMaxQueueSize)
        : thread_count_(thread_count), max_queue_size_(max_queue_size) {
        if (thread_count_ == 0) {
            throw std::invalid_argument("ThreadPool thread_count must be > 0");
        }
        if (max_queue_size_ == 0) {
            throw std::invalid_argument("ThreadPool max_queue_size must be > 0");
        }

        workers_.reserve(thread_count_);
        for (std::size_t i = 0; i < thread_count_; ++i) {
            workers_.emplace_back([this] { worker_loop(); });
        }
    }

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
    ThreadPool(ThreadPool&&) = delete;
    ThreadPool& operator=(ThreadPool&&) = delete;

    ~ThreadPool() { shutdown(); }

    /// Non-blocking. Returns false if shutting down or the queue is full.
    /// on_done (optional) is invoked on a worker thread after work completes.
    template <typename F, typename Done>
    bool try_submit(F&& work, Done&& on_done) {
        Task task = make_task(std::forward<F>(work), std::forward<Done>(on_done));

        {
            std::lock_guard lock(mutex_);
            if (stopping_ || queue_.size() >= max_queue_size_) {
                return false;
            }
            queue_.push_back(std::move(task));
        }
        cv_.notify_one();
        return true;
    }

    template <typename F>
    bool try_submit(F&& work) {
        return try_submit(std::forward<F>(work), [] {});
    }

    /// Stop accepting tasks, finish queued + in-flight work, join workers.
    void shutdown() {
        {
            std::lock_guard lock(mutex_);
            if (stopping_) {
                // Already shutting down / shut down — still join if needed.
            } else {
                stopping_ = true;
            }
        }
        cv_.notify_all();

        for (auto& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }

    [[nodiscard]] std::size_t thread_count() const noexcept { return thread_count_; }

    [[nodiscard]] std::size_t max_queue_size() const noexcept { return max_queue_size_; }

    [[nodiscard]] std::size_t queued_tasks() const {
        std::lock_guard lock(mutex_);
        return queue_.size();
    }

private:
    using Task = std::function<void()>;

    template <typename F, typename Done>
    static Task make_task(F&& work, Done&& on_done) {
        return [work_fn = std::function<void()>(std::forward<F>(work)),
                done_fn = std::function<void()>(std::forward<Done>(on_done))]() mutable {
            try {
                work_fn();
            } catch (...) {
                // Discard — no future/exception channel in this design.
            }
            try {
                done_fn();
            } catch (...) {
                // Keep the worker alive.
            }
        };
    }

    void worker_loop() {
        while (true) {
            Task task;
            {
                std::unique_lock lock(mutex_);
                cv_.wait(lock, [this] { return stopping_ || !queue_.empty(); });
                if (queue_.empty()) {
                    // stopping_ && empty → exit
                    return;
                }
                task = std::move(queue_.front());
                queue_.pop_front();
            }
            task();
        }
    }

    const std::size_t thread_count_;
    const std::size_t max_queue_size_;

    mutable std::mutex mutex_;
    std::condition_variable cv_;
    std::deque<Task> queue_;
    bool stopping_{false};
    std::vector<std::thread> workers_;
};

}  // namespace klib
