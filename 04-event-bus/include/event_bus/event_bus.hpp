#pragma once

#include "thread_pool/thread_pool.hpp"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <utility>
#include <vector>

namespace portfolio {
namespace detail {

struct EventBusState {
    explicit EventBusState(ThreadPool& pool) : pool_(pool) {}

    ThreadPool& pool() noexcept { return pool_; }

    std::uint64_t subscribe(std::type_index type, std::function<void(const void*)> fn) {
        std::lock_guard lock(mutex_);
        const std::uint64_t id = next_id_++;
        handlers_[type].push_back(HandlerEntry{id, std::move(fn)});
        id_to_type_.emplace(id, type);
        return id;
    }

    void unsubscribe(std::uint64_t id) {
        std::lock_guard lock(mutex_);
        auto type_it = id_to_type_.find(id);
        if (type_it == id_to_type_.end()) {
            return;
        }
        const std::type_index type = type_it->second;
        id_to_type_.erase(type_it);

        auto list_it = handlers_.find(type);
        if (list_it == handlers_.end()) {
            return;
        }
        auto& list = list_it->second;
        list.erase(std::remove_if(list.begin(), list.end(),
                                  [id](const HandlerEntry& e) { return e.id == id; }),
                   list.end());
        if (list.empty()) {
            handlers_.erase(list_it);
        }
    }

    void clear() {
        std::lock_guard lock(mutex_);
        handlers_.clear();
        id_to_type_.clear();
    }

    std::vector<std::function<void(const void*)>> snapshot(std::type_index type) const {
        std::lock_guard lock(mutex_);
        std::vector<std::function<void(const void*)>> out;
        auto it = handlers_.find(type);
        if (it == handlers_.end()) {
            return out;
        }
        out.reserve(it->second.size());
        for (const auto& entry : it->second) {
            out.push_back(entry.fn);
        }
        return out;
    }

    std::size_t subscriber_count() const {
        std::lock_guard lock(mutex_);
        return id_to_type_.size();
    }

private:
    struct HandlerEntry {
        std::uint64_t id;
        std::function<void(const void*)> fn;
    };

    ThreadPool& pool_;
    mutable std::mutex mutex_;
    std::uint64_t next_id_{1};
    std::unordered_map<std::type_index, std::vector<HandlerEntry>> handlers_;
    std::unordered_map<std::uint64_t, std::type_index> id_to_type_;
};

}  // namespace detail

/// RAII handle: destroying or reset() unsubscribes the handler.
class Subscription {
public:
    Subscription() = default;

    Subscription(const Subscription&) = delete;
    Subscription& operator=(const Subscription&) = delete;

    Subscription(Subscription&& other) noexcept
        : state_(std::move(other.state_)), id_(other.id_) {
        other.id_ = 0;
    }

    Subscription& operator=(Subscription&& other) noexcept {
        if (this != &other) {
            reset();
            state_ = std::move(other.state_);
            id_ = other.id_;
            other.id_ = 0;
        }
        return *this;
    }

    ~Subscription() { reset(); }

    void reset() {
        if (id_ == 0) {
            return;
        }
        if (auto state = state_.lock()) {
            state->unsubscribe(id_);
        }
        id_ = 0;
        state_.reset();
    }

    [[nodiscard]] explicit operator bool() const noexcept { return id_ != 0; }

private:
    friend class EventBus;

    Subscription(std::weak_ptr<detail::EventBusState> state, std::uint64_t id)
        : state_(std::move(state)), id_(id) {}

    std::weak_ptr<detail::EventBusState> state_;
    std::uint64_t id_{0};
};

/// Typed publish/subscribe bus; every handler runs via ThreadPool::try_submit.
///
/// Design notes:
/// - ThreadPool is non-owning and must outlive the EventBus.
/// - publish() never invokes handlers on the caller thread.
/// - Handlers are snapshotted under a mutex, then each is queued with a copy of
///   the event so the publisher can return immediately.
/// - Subscription is RAII; the bus clears listeners on destruction (weak_ptr
///   makes late Subscription::reset safe).
class EventBus {
public:
    explicit EventBus(ThreadPool& pool)
        : state_(std::make_shared<detail::EventBusState>(pool)) {}

    EventBus(const EventBus&) = delete;
    EventBus& operator=(const EventBus&) = delete;
    EventBus(EventBus&&) = delete;
    EventBus& operator=(EventBus&&) = delete;

    ~EventBus() {
        if (state_) {
            state_->clear();
        }
    }

    /// Subscribe to Event. handler signature: void(const Event&).
    template <typename Event, typename F>
    Subscription subscribe(F&& handler) {
        const std::type_index type(typeid(Event));
        auto trampoline =
            [handler_fn =
                 std::function<void(const Event&)>(std::forward<F>(handler))](
                const void* event_ptr) {
                handler_fn(*static_cast<const Event*>(event_ptr));
            };

        const std::uint64_t id = state_->subscribe(type, std::move(trampoline));
        return Subscription(state_, id);
    }

    /// Queue one pool task per current subscriber. Returns false if any
    /// try_submit failed; still attempts the rest. Zero subscribers → true.
    template <typename Event>
    bool publish(const Event& event) {
        auto handlers = state_->snapshot(std::type_index(typeid(Event)));
        bool all_queued = true;
        for (const auto& handler : handlers) {
            Event copy = event;
            if (!state_->pool().try_submit([handler, copy]() mutable {
                    handler(&copy);
                })) {
                all_queued = false;
            }
        }
        return all_queued;
    }

    [[nodiscard]] std::size_t subscriber_count() const {
        return state_->subscriber_count();
    }

private:
    std::shared_ptr<detail::EventBusState> state_;
};

}  // namespace portfolio
