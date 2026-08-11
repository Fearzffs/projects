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
| `08-blocking-mpmc-queue` | Bounded MPMC; CV back-pressure |
| `09-task-graph` | Lite DAG on thread pool; `precede` + `try_run` |
| `10-signal-slot` | Sync Signal/Slot; member binds; RAII Connection |
| `11-fsm` | Flat FSM; transitions + enter/exit; sync `handle` |
| `12-showcase-demo` | Telemetry app gluing `01`–`11` (real host metrics) |

## Remaining (in order)

_(empty — portfolio spine complete)_

## Standing decisions

- Prefer non-blocking APIs + callbacks/events over blocking callers (`future.get()`, etc.).
- Thread pool / logger / timer: create at app start, shut down at app end.
- Keep primitives clean; composition belongs in the showcase, not inside earlier folders.
- Signal/slot must teach something different from `04` (e.g. sync emit, member binds) — not a clone of the event bus.
- FSM stays flat (no hierarchical / concurrent regions); showcase owns the story, FSM owns the state table.
