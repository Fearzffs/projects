# 08 — Blocking MPMC Queue

Fixed-capacity multi-producer / multi-consumer queue with **blocking back-pressure** (C++20).

Sibling to [`01-ring-buffer`](../01-ring-buffer): same MPMC idea, different full/empty policy.

## Why this project

| | `01` RingBuffer | `08` BlockingMpmcQueue |
|---|---|---|
| Full | `try_push` false or overwrite | **`push` waits** |
| Empty | `try_pop` nullopt | **`pop` waits** |
| Storage | ring `vector` | `std::deque` + logical capacity |
| Sync | mutex only | mutex + **two condition variables** |

Shows:

- `not_full` / `not_empty` CV split (classic bounded buffer)
- wait with predicates (spurious wakeups / lost wakeup)
- `shutdown()` to unblock waiters at app teardown
- still offers `try_push` / `try_pop` for non-blocking paths

## Build & test

```bash
cmake -S 08-blocking-mpmc-queue -B 08-blocking-mpmc-queue/build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=g++
cmake --build 08-blocking-mpmc-queue/build -j"$(nproc)"
cd 08-blocking-mpmc-queue/build && ctest --output-on-failure
```

## API sketch

```cpp
klib::BlockingMpmcQueue<int> q(8);

q.push(1);                 // waits if full
auto v = q.pop();          // waits if empty

q.try_push(2);             // never waits
q.try_pop();

q.shutdown();              // wake waiters; push fails; pop drains then nullopt
```

## Thread / lifetime contracts

- **MPMC:** many producers and consumers may call `push` / `pop` / `try_*` concurrently.
- **`shutdown()`:** call once from a controlling thread at app teardown; wakes waiters. After shutdown, `push` fails; `pop` drains then returns nullopt.
- **Do not** destroy the queue while other threads still call into it without having observed shutdown (join producers/consumers first, or ensure they exit on failed push / empty pop after shutdown).
- Prefer shutdown order with siblings: stop timers that feed the queue → shutdown queue → then pool that runs callbacks (see showcase).

## Decisions

| Choice | Rationale |
|---|---|
| Blocking `push`/`pop` | Teach CV back-pressure (the point of this folder) |
| Keep `try_*` | Match portfolio non-blocking style when you need it |
| Two CVs | Producers sleep on full; consumers on empty — fewer useless wakes |
| `deque` | Simple; contrast with `01` ring indexing |
| No overwrite | Full means wait, not drop |
| `shutdown` | Same “app end” story as pool/logger |
