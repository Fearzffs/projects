# 04 — Event Bus

Typed publish/subscribe bus that dispatches every handler through [`03-thread-pool`](../03-thread-pool) (C++20).

## Why this project

Composes with the thread pool: **publish never runs handlers on the caller**.

| Choice | This project |
|---|---|
| Event identity | Typed (`subscribe<Ping>`, `publish(Ping{})`) |
| Dispatch | Always async via `ThreadPool::try_submit` |
| Unsubscribe | RAII `Subscription` |

Shows:

- type-erased listener storage keyed by `std::type_index`
- snapshot-under-lock then unlock before submit
- first portfolio folder that depends on another (`03`)
- CMake + GoogleTest + CI matching the template

## Build & test

```bash
cmake -S 04-event-bus -B 04-event-bus/build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=g++
cmake --build 04-event-bus/build -j"$(nproc)"
cd 04-event-bus/build && ctest --output-on-failure
```

## API sketch

```cpp
portfolio::ThreadPool pool(4);
portfolio::EventBus bus(pool);  // pool must outlive bus

struct PlayerDied { int id; };

auto sub = bus.subscribe<PlayerDied>([](const PlayerDied& e) {
    // runs on a pool worker
});

bus.publish(PlayerDied{42});  // queues handlers; returns immediately
sub.reset();                  // or let Subscription destructor unsubscribe
pool.shutdown();
```

## Decisions

| Choice | Rationale |
|---|---|
| Typed events | Clear API; no stringly topic typos |
| Always async via pool | Publisher never blocked in handlers; matches `03` |
| Snapshot then submit | No bus lock held during handler execution |
| RAII `Subscription` | Hard to leak listeners |
| Non-owning `ThreadPool&` | Pool lifetime stays app-owned |
