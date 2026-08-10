#pragma once

#include <atomic>
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace klib {

namespace detail {

inline constexpr std::size_t cache_line_size = 64;

[[nodiscard]] inline std::size_t next_power_of_two(std::size_t n) noexcept {
    if (n <= 1) {
        return 1;
    }
    --n;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    if constexpr (sizeof(std::size_t) > 4) {
        n |= n >> 32;
    }
    ++n;
    return n;
}

[[nodiscard]] inline std::size_t checked_capacity(std::size_t capacity) {
    if (capacity == 0) {
        throw std::invalid_argument("SpscRingBuffer capacity must be > 0");
    }
    return next_power_of_two(capacity);
}

}  // namespace detail

/// Lock-free fixed-capacity FIFO for exactly one producer and one consumer.
///
/// Design notes:
/// - Capacity is rounded up to the next power of two (mask instead of modulo).
/// - try_push fails when full; no auto-overwrite (would race with the consumer).
/// - try_pop fails when empty; no try_pop_back (producer/consumer roles stay fixed).
/// - write/read indices live on separate cache lines to avoid false sharing.
/// - size()/empty()/full() are snapshots; safe for tests after both sides join.
/// - Not exception-safe for throwing T move/copy during push/pop; prefer noexcept T.
/// - Copy and move are deleted: atomics + concurrent use do not compose with moves.
template <typename T>
class SpscRingBuffer {
    static_assert(std::is_default_constructible_v<T>,
                  "SpscRingBuffer requires default-constructible T for slot clearing");

public:
    explicit SpscRingBuffer(std::size_t capacity)
        : capacity_(detail::checked_capacity(capacity)),
          mask_(capacity_ - 1),
          buffer_(capacity_) {}

    SpscRingBuffer(const SpscRingBuffer&) = delete;
    SpscRingBuffer& operator=(const SpscRingBuffer&) = delete;
    SpscRingBuffer(SpscRingBuffer&&) = delete;
    SpscRingBuffer& operator=(SpscRingBuffer&&) = delete;

    [[nodiscard]] std::size_t capacity() const noexcept { return capacity_; }

    /// Approximate item count. Exact after producer and consumer have joined.
    [[nodiscard]] std::size_t size() const noexcept {
        const auto write = write_pos_.load(std::memory_order_acquire);
        const auto read = read_pos_.load(std::memory_order_acquire);
        return write - read;
    }

    [[nodiscard]] bool empty() const noexcept { return size() == 0; }

    [[nodiscard]] bool full() const noexcept { return size() == capacity_; }

    /// Producer only. Returns false if the buffer is full.
    template <typename U>
    bool try_push(U&& value) {
        const auto write = write_pos_.load(std::memory_order_relaxed);
        const auto read = read_pos_.load(std::memory_order_acquire);
        if (write - read == capacity_) {
            return false;
        }
        buffer_[write & mask_] = std::forward<U>(value);
        write_pos_.store(write + 1, std::memory_order_release);
        return true;
    }

    /// Consumer only. Returns nullopt if the buffer is empty.
    [[nodiscard]] std::optional<T> try_pop() {
        const auto read = read_pos_.load(std::memory_order_relaxed);
        const auto write = write_pos_.load(std::memory_order_acquire);
        if (read == write) {
            return std::nullopt;
        }
        T value = std::move(buffer_[read & mask_]);
        buffer_[read & mask_] = T{};
        read_pos_.store(read + 1, std::memory_order_release);
        return value;
    }

private:
    alignas(detail::cache_line_size) std::atomic<std::size_t> write_pos_{0};
    alignas(detail::cache_line_size) std::atomic<std::size_t> read_pos_{0};

    const std::size_t capacity_;
    const std::size_t mask_;
    std::vector<T> buffer_;
};

}  // namespace klib
