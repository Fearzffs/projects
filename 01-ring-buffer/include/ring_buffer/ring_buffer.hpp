#pragma once

#include <cstddef>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

namespace portfolio {

/// Fixed-capacity FIFO ring buffer with mutex synchronization.
///
/// Design notes:
/// - Capacity is fixed at construction; no reallocation under contention.
/// - One empty slot is *not* reserved; full/empty are tracked with a size counter.
/// - Suitable for multi-producer / multi-consumer workloads where simplicity
///   and correctness matter more than lock-free throughput.
/// - Not exception-safe for throwing T move/copy during push/pop; prefer noexcept T.
template <typename T>
class RingBuffer {
public:
    explicit RingBuffer(std::size_t capacity)
        : capacity_(capacity), buffer_(capacity) {
        if (capacity == 0) {
            throw std::invalid_argument("RingBuffer capacity must be > 0");
        }
    }

    RingBuffer(const RingBuffer&) = delete;
    RingBuffer& operator=(const RingBuffer&) = delete;

    RingBuffer(RingBuffer&& other) noexcept {
        std::scoped_lock lock(other.mutex_);
        capacity_ = other.capacity_;
        head_ = other.head_;
        tail_ = other.tail_;
        size_ = other.size_;
        buffer_ = std::move(other.buffer_);
        other.head_ = 0;
        other.tail_ = 0;
        other.size_ = 0;
    }

    RingBuffer& operator=(RingBuffer&& other) noexcept {
        if (this == &other) {
            return *this;
        }
        std::scoped_lock lock(mutex_, other.mutex_);
        capacity_ = other.capacity_;
        head_ = other.head_;
        tail_ = other.tail_;
        size_ = other.size_;
        buffer_ = std::move(other.buffer_);
        other.head_ = 0;
        other.tail_ = 0;
        other.size_ = 0;
        return *this;
    }

    [[nodiscard]] std::size_t capacity() const noexcept { return capacity_; }

    [[nodiscard]] std::size_t size() const {
        std::scoped_lock lock(mutex_);
        return size_;
    }

    [[nodiscard]] bool empty() const {
        std::scoped_lock lock(mutex_);
        return size_ == 0;
    }

    [[nodiscard]] bool full() const {
        std::scoped_lock lock(mutex_);
        return size_ == capacity_;
    }

    /// Returns false if the buffer is full.
    bool try_push(const T& value) {
        std::scoped_lock lock(mutex_);
        if (size_ == capacity_) {
            return false;
        }
        buffer_[tail_] = value;
        advance_tail();
        return true;
    }

    /// Returns false if the buffer is full.
    bool try_push(T&& value) {
        std::scoped_lock lock(mutex_);
        if (size_ == capacity_) {
            return false;
        }
        buffer_[tail_] = std::move(value);
        advance_tail();
        return true;
    }

    /// Returns nullopt if the buffer is empty.
    [[nodiscard]] std::optional<T> try_pop() {
        std::scoped_lock lock(mutex_);
        if (size_ == 0) {
            return std::nullopt;
        }
        T value = std::move(buffer_[head_]);
        buffer_[head_] = T{};
        advance_head();
        return value;
    }

    void clear() {
        std::scoped_lock lock(mutex_);
        while (size_ > 0) {
            buffer_[head_] = T{};
            advance_head();
        }
    }

private:
    void advance_head() noexcept {
        head_ = (head_ + 1) % capacity_;
        --size_;
    }

    void advance_tail() noexcept {
        tail_ = (tail_ + 1) % capacity_;
        ++size_;
    }

    mutable std::mutex mutex_;
    std::size_t capacity_{0};
    std::size_t head_{0};
    std::size_t tail_{0};
    std::size_t size_{0};
    std::vector<T> buffer_;
};

}  // namespace portfolio
