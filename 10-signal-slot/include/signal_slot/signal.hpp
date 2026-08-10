#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>
#include <tuple>
#include <utility>
#include <vector>

namespace klib {

/// How a slot is invoked relative to emit.
///
/// - Direct: slot runs immediately on the thread that called emit (current default).
/// - Queued: slot is posted to the SlotDispatcher of the thread that called
///   connect; that thread must call SlotDispatcher::this_thread().process().
enum class ConnectionType {
    Direct,
    Queued,
};

/// Per-thread queue used by ConnectionType::Queued.
///
/// Capture happens at connect() time. The connecting thread (or whatever thread
/// owns that dispatcher) must call process() to run posted slots.
class SlotDispatcher {
public:
    [[nodiscard]] static SlotDispatcher& this_thread() {
        thread_local SlotDispatcher instance;
        return instance;
    }

    void post(std::function<void()> job) {
        {
            std::lock_guard lock(mutex_);
            queue_.push_back(std::move(job));
        }
    }

    /// Run all jobs currently queued on this dispatcher. Returns how many ran.
    std::size_t process() {
        std::vector<std::function<void()>> batch;
        {
            std::lock_guard lock(mutex_);
            batch.swap(queue_);
        }
        for (auto& job : batch) {
            try {
                if (job) {
                    job();
                }
            } catch (...) {
            }
        }
        return batch.size();
    }

    [[nodiscard]] std::size_t pending() const {
        std::lock_guard lock(mutex_);
        return queue_.size();
    }

private:
    mutable std::mutex mutex_;
    std::vector<std::function<void()>> queue_;
};

/// RAII handle: destroying or reset() disconnects the slot.
class Connection {
public:
    Connection() = default;

    Connection(const Connection&) = delete;
    Connection& operator=(const Connection&) = delete;

    Connection(Connection&& other) noexcept : disconnect_(std::move(other.disconnect_)) {
        other.disconnect_ = nullptr;
    }

    Connection& operator=(Connection&& other) noexcept {
        if (this != &other) {
            reset();
            disconnect_ = std::move(other.disconnect_);
            other.disconnect_ = nullptr;
        }
        return *this;
    }

    ~Connection() { reset(); }

    void reset() {
        if (disconnect_) {
            disconnect_();
            disconnect_ = nullptr;
        }
    }

    [[nodiscard]] explicit operator bool() const noexcept {
        return static_cast<bool>(disconnect_);
    }

private:
    template <typename...>
    friend class Signal;

    explicit Connection(std::function<void()> disconnect)
        : disconnect_(std::move(disconnect)) {}

    std::function<void()> disconnect_;
};

/// Typed observer signal: connect slots; Direct emit is synchronous on caller.
///
/// Design notes:
/// - Distinct from EventBus: no thread pool required.
/// - ConnectionType::Direct — slot runs on the emitting thread (immediate).
/// - ConnectionType::Queued — slot posted to the connect-thread's SlotDispatcher;
///   that thread must process() the queue (guarantees slot thread == connect thread).
/// - Snapshot-under-lock then invoke unlocked.
/// - Slot exceptions are caught and discarded.
template <typename... Args>
class Signal {
public:
    Signal() : state_(std::make_shared<State>()) {}

    Signal(const Signal&) = delete;
    Signal& operator=(const Signal&) = delete;
    Signal(Signal&&) = delete;
    Signal& operator=(Signal&&) = delete;

    template <typename F>
    [[nodiscard]] Connection connect(F&& slot,
                                     ConnectionType type = ConnectionType::Direct) {
        std::function<void(Args...)> fn(std::forward<F>(slot));
        SlotDispatcher* dispatcher = nullptr;
        if (type == ConnectionType::Queued) {
            dispatcher = &SlotDispatcher::this_thread();
        }
        const std::uint64_t id = state_->add(std::move(fn), type, dispatcher);
        std::weak_ptr<State> weak = state_;
        return Connection([weak, id] {
            if (auto state = weak.lock()) {
                state->remove(id);
            }
        });
    }

    /// Member bind: calls (obj->*method)(args...) on emit. obj must be non-null.
    template <typename T>
    [[nodiscard]] Connection connect(T* obj, void (T::*method)(Args...),
                                     ConnectionType type = ConnectionType::Direct) {
        if (obj == nullptr) {
            throw std::invalid_argument("Signal::connect: null object");
        }
        if (method == nullptr) {
            throw std::invalid_argument("Signal::connect: null method");
        }
        return connect(
            [obj, method](Args... args) {
                (obj->*method)(std::forward<Args>(args)...);
            },
            type);
    }

    /// Invoke connected slots according to each connection's type.
    void emit(const Args&... args) const {
        const auto snap = state_->snapshot();
        for (const auto& entry : snap) {
            try {
                if (entry.type == ConnectionType::Direct || entry.dispatcher == nullptr) {
                    entry.fn(args...);
                } else {
                    // Copy args into the posted job so emit can return immediately.
                    entry.dispatcher->post([fn = entry.fn, packed = std::make_tuple(args...)]() mutable {
                        std::apply(fn, std::move(packed));
                    });
                }
            } catch (...) {
            }
        }
    }

    [[nodiscard]] std::size_t slot_count() const { return state_->size(); }

private:
    struct State {
        struct Slot {
            std::uint64_t id;
            ConnectionType type{ConnectionType::Direct};
            SlotDispatcher* dispatcher{nullptr};
            std::function<void(Args...)> fn;
        };

        std::uint64_t add(std::function<void(Args...)> fn, ConnectionType type,
                          SlotDispatcher* dispatcher) {
            std::lock_guard lock(mutex_);
            const std::uint64_t id = next_id_++;
            slots_.push_back(Slot{id, type, dispatcher, std::move(fn)});
            return id;
        }

        void remove(std::uint64_t id) {
            std::lock_guard lock(mutex_);
            for (auto it = slots_.begin(); it != slots_.end(); ++it) {
                if (it->id == id) {
                    slots_.erase(it);
                    return;
                }
            }
        }

        std::vector<Slot> snapshot() const {
            std::lock_guard lock(mutex_);
            return slots_;
        }

        std::size_t size() const {
            std::lock_guard lock(mutex_);
            return slots_.size();
        }

        mutable std::mutex mutex_;
        std::uint64_t next_id_{1};
        std::vector<Slot> slots_;
    };

    std::shared_ptr<State> state_;
};

}  // namespace klib
