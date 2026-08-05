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

## Remaining (in order)

1. **Timer / scheduler** — `run_after` / `run_every`; compose with pool and/or event bus.
2. **Arena / bump allocator** — Lifetime/memory axis (separate from concurrency).
3. **Blocking MPMC queue** — Condition-variable + back-pressure sibling to `01`.
4. **Lite task graph** — “B after A” dependencies on the thread pool.
5. **Showcase demo app** — Glue pool + bus + logger (and later pieces) without modifying `01`–`05` public APIs.

## Standing decisions

- Prefer non-blocking APIs + callbacks/events over blocking callers (`future.get()`, etc.).
- Thread pool / logger: create at app start, shut down at app end.
- Keep primitives clean; composition belongs in a later showcase, not inside `01`–`05`.
