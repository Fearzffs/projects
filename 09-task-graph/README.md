# 09 — Task Graph (lite)

Run tasks on [`03-thread-pool`](../03-thread-pool) with **dependencies**: B starts only after A finishes (C++20).

## Why this project

Composes the pool into a small DAG executor — the “job system lite” step before a showcase app.

| Choice | This project |
|---|---|
| Edges | `precede(A, B)` → A before B |
| Kickoff | Non-blocking `try_run()` |
| Execution | `ThreadPool::try_submit` |
| Per-task done | Optional `on_done` after work (before successors) |
| Graph done | `on_complete` and/or `wait()` |

Shows:

- predecessor refcounts + successor lists
- submitting newly-ready nodes from a task’s completion callback
- building the graph, then freezing it after `try_run`
- CMake + GoogleTest + CI matching the template

## Build & test

```bash
cmake -S 09-task-graph -B 09-task-graph/build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=g++
cmake --build 09-task-graph/build -j"$(nproc)"
cd 09-task-graph/build && ctest --output-on-failure
```

## API sketch

```cpp
portfolio::ThreadPool pool(4);
portfolio::TaskGraph graph(pool);

auto a = graph.add(
    [] { /* work */ },
    [] { /* this task finished */ });
auto b = graph.add([] { /* ... */ });
graph.precede(a, b);   // A (work + on_done) then B

graph.set_on_complete([] { /* whole graph finished */ });
graph.try_run();
graph.wait();
pool.shutdown();
```

## Decisions

| Choice | Rationale |
|---|---|
| Refcount deps | Simple, classic; enough for a lite graph |
| Per-task `on_done` | Same idea as `ThreadPool`; successors wait for it |
| Graph `on_complete` | One place for “DAG finished”; multi-listener fan-out stays signal/slot |
| No cycle detection | Keeps the folder small; cycles strand tasks |
| Freeze after `try_run` | Avoid races with mutating edges under fire |
| Pool non-owning | Same lifetime story as event bus / timer |
| `wait()` available | Tests and app exit; async path uses `on_complete` |
