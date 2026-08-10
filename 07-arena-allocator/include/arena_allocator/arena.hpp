#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>

namespace klib {
namespace detail {

[[nodiscard]] inline bool is_power_of_two(std::size_t n) noexcept {
    return n != 0 && (n & (n - 1)) == 0;
}

}  // namespace detail

/// Fixed-capacity bump (arena) allocator.
///
/// Design notes:
/// - Allocations advance a cursor; there is no per-block free.
/// - reset() rewinds the cursor for reuse. It does *not* call destructors —
///   destroy objects yourself (or only store trivially destructible data)
///   before reset if that matters.
/// - try_allocate returns nullptr when the remaining space (after alignment)
///   cannot satisfy the request — non-throwing, matches portfolio try_* style.
/// - Not thread-safe; external synchronization required for shared use.
/// - Copy/move deleted: owning a unique backing buffer.
class Arena {
public:
    explicit Arena(std::size_t capacity_bytes)
        : buffer_(capacity_bytes), offset_(0) {
        if (capacity_bytes == 0) {
            throw std::invalid_argument("Arena capacity must be > 0");
        }
    }

    Arena(const Arena&) = delete;
    Arena& operator=(const Arena&) = delete;
    Arena(Arena&&) = delete;
    Arena& operator=(Arena&&) = delete;

    /// Allocate `size` bytes aligned to `alignment` (power of two).
    [[nodiscard]] void* try_allocate(std::size_t size,
                                     std::size_t alignment = alignof(std::max_align_t)) {
        if (size == 0) {
            return nullptr;
        }
        if (!detail::is_power_of_two(alignment)) {
            throw std::invalid_argument("Arena alignment must be a power of two");
        }

        const auto base = reinterpret_cast<std::uintptr_t>(buffer_.data());
        const auto current = base + offset_;
        const auto aligned = (current + alignment - 1) & ~(static_cast<std::uintptr_t>(alignment) - 1);
        const auto padding = static_cast<std::size_t>(aligned - current);
        if (padding > capacity() - offset_) {
            return nullptr;
        }
        if (size > capacity() - offset_ - padding) {
            return nullptr;
        }

        offset_ += padding + size;
        return reinterpret_cast<void*>(aligned);
    }

    /// Typed allocate (uninitialized). Prefer try_create when construction matters.
    template <typename T>
    [[nodiscard]] T* try_allocate(std::size_t count = 1) {
        static_assert(!std::is_void_v<T>, "Arena::try_allocate<T> requires a non-void T");
        if (count == 0) {
            return nullptr;
        }
        if (count > (std::size_t{0} - 1) / sizeof(T)) {
            return nullptr;  // overflow
        }
        return static_cast<T*>(try_allocate(sizeof(T) * count, alignof(T)));
    }

    /// Allocate and placement-new construct one T. Returns nullptr if OOM.
    template <typename T, typename... Args>
    [[nodiscard]] T* try_create(Args&&... args) {
        void* mem = try_allocate(sizeof(T), alignof(T));
        if (mem == nullptr) {
            return nullptr;
        }
        return ::new (mem) T(std::forward<Args>(args)...);
    }

    /// Rewind the bump cursor. Does not run destructors.
    void reset() noexcept { offset_ = 0; }

    [[nodiscard]] std::size_t capacity() const noexcept { return buffer_.size(); }

    [[nodiscard]] std::size_t used() const noexcept { return offset_; }

    [[nodiscard]] std::size_t remaining() const noexcept { return capacity() - offset_; }

private:
    std::vector<std::byte> buffer_;
    std::size_t offset_;
};

}  // namespace klib
