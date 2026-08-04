# 03 — Thread Pool

Fixed-size worker pool with non-blocking `try_submit` and optional completion callbacks (C++20).

## Why this project

Follow-up to the queue projects: **hand off work, keep doing other work, get told when it finishes**.

| Pattern | This pool |
|---|---|
| Blocking `future.get()` on the caller | Not used |
| Completion notification | Optional `on_done` callback on a worker thread |
| Submit back-pressure | `try_submit` returns `false` when the queue is full |

Shows:

- worker threads waiting on `condition_variable`
- bounded task queue (mutex + `deque`)
- non-blocking submit from any thread
- orderly `shutdown()` (drain + join)
- CMake + GoogleTest + CI matching the portfolio template

## Build & test

```bash
cmake -S 03-thread-pool -B 03-thread-pool/build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=g++
cmake --build 03-thread-pool/build -j"$(nproc)"
cd 03-thread-pool/build && ctest --output-on-failure
```

## Debug tests in Cursor / VS Code

1. Open the **repo root** as the workspace folder.
2. `Ctrl+Shift+B` builds the thread pool tests (default task).
3. Run and Debug → **Debug thread_pool_tests (all)** (or one-test / ManyTasksComplete).

## API sketch

```cpp
portfolio::ThreadPool pool(4);  // 4 workers, default max queue 1024

pool.try_submit([] {
    // work runs on a worker thread
}, [] {
    // on_done also runs on a worker thread — set your own event/flag here
});

pool.shutdown();  // stop accepting, drain queue, join workers
```

## Decisions

| Choice | Rationale |
|---|---|
| Callback completion, not `future.get()` | Caller stays non-blocking; matches “notify when done” |
| `try_submit` only | No hidden blocking of the producer thread |
| Callback on worker thread | No second marshal queue; caller decides how to wake main |
| Self-contained mutex/cv queue | Independent folder; blocking wait is for workers only |
| Bounded queue | Back-pressure via `false`; same spirit as ring `try_push` |
| Drain on shutdown | Predictable teardown for tests and demos |
| Exceptions from work discarded | No future exception channel; `on_done` still runs |
