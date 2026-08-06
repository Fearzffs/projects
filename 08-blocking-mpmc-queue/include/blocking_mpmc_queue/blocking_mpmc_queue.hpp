#pragma once

#include <condition_variable>
#include <cstddef>
#include <deque>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <utility>

namespace portfolio {

/// Fixed-capacity multi-producer / multi-consumer queue with blocking back-pressure.
///
/// Contrast with `01` RingBuffer (mutex + try_* / overwrite):
/// - push waits while full; pop waits while empty (condition variables).
/// - No overwrite: producers block instead of dropping.
/// - try_push / try_pop never wait; return false / nullopt when they cannot proceed.
/// - shutdown() wakes waiters; further push fails; pop drains then returns nullopt.
/// - Storage is std::deque (simple); capacity is a logical max, not a ring mask.
/// - Not exception-safe for throwing T during push/pop; prefer noexcept T.
template <typename T>
class BlockingMpmcQueue {
public:
    explicit BlockingMpmcQueue(std::size_t capacity) : capacity_(capacity) {
        if (capacity_ == 0) {
            throw std::invalid_argument("BlockingMpmcQueue capacity must be > 0");
        }
    }

    BlockingMpmcQueue(const BlockingMpmcQueue&) = delete;
    BlockingMpmcQueue& operator=(const BlockingMpmcQueue&) = delete;
    BlockingMpmcQueue(BlockingMpmcQueue&&) = delete;
    BlockingMpmcQueue& operator=(BlockingMpmcQueue&&) = delete;

    ~BlockingMpmcQueue() { shutdown(); }

    [[nodiscard]] std::size_t capacity() const noexcept { return capacity_; }

    [[nodiscard]] std::size_t size() const {
        std::lock_guard lock(mutex_);
        return queue_.size();
    }

    [[nodiscard]] bool empty() const {
        std::lock_guard lock(mutex_);
        return queue_.empty();
    }

    [[nodiscard]] bool full() const {
        std::lock_guard lock(mutex_);
        return queue_.size() >= capacity_;
    }

    /// Block while full. Returns false if shutdown was requested before/while waiting.
    template <typename U>
    bool push(U&& value) {
        {
            std::unique_lock lock(mutex_);
            not_full_.wait(lock, [this] {
                return stopping_ || queue_.size() < capacity_;
            });
            if (stopping_) {
                return false;
            }
            queue_.push_back(std::forward<U>(value));
        }
        not_empty_.notify_one();
        return true;
    }

    /// Block while empty. Returns nullopt if shutdown and the queue is drained.
    [[nodiscard]] std::optional<T> pop() {
        std::optional<T> out;
        {
            std::unique_lock lock(mutex_);
            not_empty_.wait(lock, [this] {
                return stopping_ || !queue_.empty();
            });
            if (queue_.empty()) {
                return std::nullopt;
            }
            out.emplace(std::move(queue_.front()));
            queue_.pop_front();
        }
        not_full_.notify_one();
        return out;
    }

    /// Non-blocking. false if full or shutting down.
    template <typename U>
    bool try_push(U&& value) {
        {
            std::lock_guard lock(mutex_);
            if (stopping_ || queue_.size() >= capacity_) {
                return false;
            }
            queue_.push_back(std::forward<U>(value));
        }
        not_empty_.notify_one();
        return true;
    }

    /// Non-blocking. nullopt if empty (does not wait; ignores shutdown except emptiness).
    [[nodiscard]] std::optional<T> try_pop() {
        std::optional<T> out;
        {
            std::lock_guard lock(mutex_);
            if (queue_.empty()) {
                return std::nullopt;
            }
            out.emplace(std::move(queue_.front()));
            queue_.pop_front();
        }
        not_full_.notify_one();
        return out;
    }

    /// Wake all waiters; reject new push; allow pop to finish draining.
    void shutdown() {
        {
            std::lock_guard lock(mutex_);
            if (stopping_) {
                return;
            }
            stopping_ = true;
        }
        not_empty_.notify_all();
        not_full_.notify_all();
    }

    [[nodiscard]] bool stopping() const {
        std::lock_guard lock(mutex_);
        return stopping_;
    }

private:
    const std::size_t capacity_;
    mutable std::mutex mutex_;
    std::condition_variable not_empty_;
    std::condition_variable not_full_;
    std::deque<T> queue_;
    bool stopping_{false};
};

}  // namespace portfolio
