# 11 — Finite State Machine (lite)

Flat **state / event / transition** machine with enter/exit callbacks (C++20).

## Why this project

A widely used control pattern that is still missing from the portfolio — and the natural spine for the showcase demo.

| Choice | This project |
|---|---|
| Shape | Flat FSM (no hierarchy) |
| Dispatch | Sync `handle(event)` on the caller |
| Table | `add_transition(from, event, to)` |
| Hooks | `on_enter` / `on_exit` |
| Unknown edge | Ignored (no throw) |

Shows:

- enum (or any hashable) `State` / `Event` as template parameters
- exit → switch → enter ordering
- self-transitions still fire exit/enter
- CMake + GoogleTest + CI matching the template

## Build & test

```bash
cmake -S 11-fsm -B 11-fsm/build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=g++
cmake --build 11-fsm/build -j"$(nproc)"
cd 11-fsm/build && ctest --output-on-failure
```

## API sketch

```cpp
enum class State { Boot, Loading, Ready };
enum class Event { StartLoad, LoadDone };

// Provide std::hash<State> / std::hash<Event> (enums need specializations).

klib::Fsm<State, Event> fsm;
fsm.add_transition(State::Boot, Event::StartLoad, State::Loading);
fsm.add_transition(State::Loading, Event::LoadDone, State::Ready);

fsm.on_enter(State::Loading, [] { /* ... */ });
fsm.on_exit(State::Boot, [] { /* ... */ });

fsm.start(State::Boot);          // runs on_enter(Boot)
fsm.handle(Event::StartLoad);    // exit Boot → enter Loading
fsm.handle(Event::LoadDone);     // → Ready
```

## Decisions

| Choice | Rationale |
|---|---|
| Sync `handle` | Same mental model as signal Direct; easy to reason about |
| Ignore unknown events | Callers can probe without try/catch; keeps lite |
| Self-transition re-enters | Explicit edge means “do the hooks again” |
| No thread pool | Pure control table; composition later in showcase |
| User supplies `std::hash` | Keeps the header free of enum magic |
