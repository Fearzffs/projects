# 06 — Timer Scheduler

Schedules one-shot and periodic callbacks that fire through [`03-thread-pool`](../03-thread-pool) (C++20).

## Why this project

Compose time with the pool: **waiting for a deadline must not block workers or callers**.

| Choice | This project |
|---|---|
| API | Non-blocking `try_run_after` / `try_run_every` |
| Execution | Dedicated timer thread + `ThreadPool::try_submit` |
| Cancel | RAII `TimerHandle` |

Shows:

- min-heap of deadlines + condition_variable `wait_until`
- publishing work onto `03` (same pattern as `04`)
- first portfolio folder that depends on the pool for timed dispatch
- CMake + GoogleTest + CI matching the template

## Build & test

```bash
cmake -S 06-timer-scheduler -B 06-timer-scheduler/build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=g++
cmake --build 06-timer-scheduler/build -j"$(nproc)"
cd 06-timer-scheduler/build && ctest --output-on-failure
```

## API sketch

```cpp
portfolio::ThreadPool pool(4);
portfolio::TimerScheduler timers(pool);  // pool must outlive timers

auto once = timers.try_run_after(50ms, [] {
    // runs on a pool worker
});

auto tick = timers.try_run_every(100ms, [] { /* ... */ });
tick.reset();   // cancel periodic
timers.shutdown();
pool.shutdown();
```

## Decisions

| Choice | Rationale |
|---|---|
| Timer thread ≠ user work | Sleep/wait stays off the pool; callbacks use `try_submit` |
| Non-blocking schedule | Matches portfolio style; empty handle if shutting down |
| RAII `TimerHandle` | Hard to leak live timers |
| Non-owning `ThreadPool&` | Pool lifetime stays app-owned |
| `run_every` first fire after one period | Simple, predictable; no “fire immediately” mode yet |
