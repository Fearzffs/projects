#pragma once

#include <functional>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace klib {

/// Flat finite state machine: transitions + enter/exit callbacks.
///
/// Design notes:
/// - State and Event are user types (typically enums) usable as unordered_map keys.
/// - handle(event) is synchronous on the caller thread.
/// - Unknown (state, event) pairs are ignored (no transition).
/// - start(initial) runs on_enter(initial); further start() throws.
/// - No hierarchy / orthogonal regions; no thread pool dependency.
template <typename State, typename Event>
class Fsm {
public:
    using Action = std::function<void()>;

    Fsm() = default;

    Fsm(const Fsm&) = delete;
    Fsm& operator=(const Fsm&) = delete;
    Fsm(Fsm&&) = delete;
    Fsm& operator=(Fsm&&) = delete;

    void add_transition(State from, Event event, State to) {
        transitions_[{from, event}] = to;
    }

    void on_enter(State state, Action action) {
        enter_[state] = std::move(action);
    }

    void on_exit(State state, Action action) {
        exit_[state] = std::move(action);
    }

    /// Enter the initial state (runs on_enter). Call once before handle().
    void start(State initial) {
        if (current_.has_value()) {
            throw std::logic_error("Fsm::start called more than once");
        }
        current_ = initial;
        run_enter(*current_);
    }

    /// Process an event. If a transition exists: on_exit(current) → switch → on_enter(next).
    /// Unknown transitions are ignored (current state unchanged).
    void handle(Event event) {
        if (!current_.has_value()) {
            throw std::logic_error("Fsm::handle before start");
        }
        const auto it = transitions_.find({*current_, event});
        if (it == transitions_.end()) {
            return;
        }
        const State next = it->second;
        if (next == *current_) {
            // Self-transition: still run exit then enter (explicit table edge).
            run_exit(*current_);
            run_enter(next);
            return;
        }
        run_exit(*current_);
        current_ = next;
        run_enter(*current_);
    }

    [[nodiscard]] bool started() const noexcept { return current_.has_value(); }

    [[nodiscard]] State current() const {
        if (!current_.has_value()) {
            throw std::logic_error("Fsm::current before start");
        }
        return *current_;
    }

private:
    struct Key {
        State state;
        Event event;

        bool operator==(const Key& other) const {
            return state == other.state && event == other.event;
        }
    };

    struct KeyHash {
        std::size_t operator()(const Key& key) const noexcept {
            const auto h1 = std::hash<State>{}(key.state);
            const auto h2 = std::hash<Event>{}(key.event);
            return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
        }
    };

    void run_enter(State state) {
        const auto it = enter_.find(state);
        if (it != enter_.end() && it->second) {
            try {
                it->second();
            } catch (...) {
            }
        }
    }

    void run_exit(State state) {
        const auto it = exit_.find(state);
        if (it != exit_.end() && it->second) {
            try {
                it->second();
            } catch (...) {
            }
        }
    }

    std::optional<State> current_;
    std::unordered_map<Key, State, KeyHash> transitions_;
    std::unordered_map<State, Action> enter_;
    std::unordered_map<State, Action> exit_;
};

}  // namespace klib
