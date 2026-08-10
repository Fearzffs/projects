# 10 — Signal / Slot

Typed observer signals with **Direct** (sync) and **Queued** (connect-thread) delivery (C++20).

## Why this project

Different teaching point from [`04-event-bus`](../04-event-bus):

| | Event bus (`04`) | Signal / slot (`10`) |
|---|---|---|
| Shape | One bus, typed event structs | Per-signal `Signal<Args...>` |
| Dispatch | Always async via thread pool | **Direct** sync, or **Queued** to connect thread |
| Bind | Lambdas / `std::function` | Lambdas **and** `obj` + member pointer |

Shows:

- connect / disconnect (RAII `Connection`)
- `ConnectionType::Direct` vs `ConnectionType::Queued`
- per-thread `SlotDispatcher` + `process()` for Queued
- snapshot-under-lock then invoke unlocked
- CMake + GoogleTest + CI matching the template

## Build & test

```bash
cmake -S 10-signal-slot -B 10-signal-slot/build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=g++
cmake --build 10-signal-slot/build -j"$(nproc)"
cd 10-signal-slot/build && ctest --output-on-failure
```

## API sketch

```cpp
portfolio::Signal<int> health_changed;

struct Hud {
    void on_health(int hp);
};
Hud hud;

// Direct (default): slot runs now on the emitting thread
auto c1 = health_changed.connect([](int hp) { /* ... */ });

// Queued: slot runs on the thread that called connect, after process()
auto c2 = health_changed.connect(
    &hud, &Hud::on_health,
    portfolio::ConnectionType::Queued);

health_changed.emit(80);  // c1 runs now; c2 is posted to connect-thread queue

// On the connect thread (often a loop / UI / worker pump):
portfolio::SlotDispatcher::this_thread().process();
```

## Connection types

| Type | Slot runs on | When |
|---|---|---|
| `Direct` | Thread that called `emit` | Immediately inside `emit` |
| `Queued` | Thread that called `connect` | After that thread calls `SlotDispatcher::this_thread().process()` |

`emit` itself always returns after Direct slots finish and Queued jobs are **posted** (it does not wait for `process()`).

## Decisions

| Choice | Rationale |
|---|---|
| Direct default | Same mental model as classic sync signal/slot |
| Queued via `SlotDispatcher` | Guarantees connect-thread affinity without a full event loop framework |
| Caller must `process()` | Honest: no magic thread hijacking |
| Member bind API | Classic signal/slot teaching point |
| Snapshot then invoke | Safe connect/disconnect during emit |
| No thread pool required | Standalone; showcase can still compose with `03` later |
| RAII `Connection` | Hard to leak slots |
