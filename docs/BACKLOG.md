# Portfolio backlog

Ordered next projects. Do not invent new folders or reorder unless the human asks.

## Done

| Folder | Focus |
|---|---|
| `01-ring-buffer` | Mutex MPMC FIFO |
| `02-spsc-ring-buffer` | Lock-free SPSC FIFO |
| `03-thread-pool` | Non-blocking try_submit + on_done |
| `04-event-bus` | Typed pub/sub on the thread pool |
| `05-async-logger` | Non-blocking logger on SPSC; dedicated writer |
| `06-timer-scheduler` | try_run_after / try_run_every via thread pool |
| `07-arena-allocator` | Bump arena allocate + reset |

## Remaining (in order)

1. **Blocking MPMC queue** — Condition-variable + back-pressure sibling to `01`.
2. **Lite task graph** — “B after A” dependencies on the thread pool.
3. **Signal / slot system** — Connect/emit distinct from `04` event bus (before showcase).
4. **Showcase demo app** — Glue pool + bus + logger + timer (and later pieces) without modifying earlier public APIs.

## Standing decisions

- Prefer non-blocking APIs + callbacks/events over blocking callers (`future.get()`, etc.).
- Thread pool / logger / timer: create at app start, shut down at app end.
- Keep primitives clean; composition belongs in a later showcase, not inside earlier folders.
- Signal/slot must teach something different from `04` (e.g. sync emit, member binds) — not a clone of the event bus.
