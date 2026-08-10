# C++ Portfolio

Small, finished C++ projects. Each folder is independent and meant to be readable in about ten minutes.

I am a lead C++ developer. This repo is for staying sharp with modern C++ and for working seriously with AI agents — not a beginner tutorial dump.

## Projects

| Folder | Focus |
|---|---|
| [`01-ring-buffer`](01-ring-buffer) | Mutex-synchronized fixed-capacity FIFO, C++20, GoogleTest |
| [`02-spsc-ring-buffer`](02-spsc-ring-buffer) | Lock-free SPSC fixed-capacity FIFO, atomics, GoogleTest |
| [`03-thread-pool`](03-thread-pool) | Fixed worker pool, non-blocking try_submit + completion callback |
| [`04-event-bus`](04-event-bus) | Typed pub/sub; handlers dispatched via `03` thread pool |
| [`05-async-logger`](05-async-logger) | Non-blocking logger on `02` SPSC; dedicated writer thread |
| [`06-timer-scheduler`](06-timer-scheduler) | `try_run_after` / `try_run_every`; fires via `03` thread pool |
| [`07-arena-allocator`](07-arena-allocator) | Bump arena: fast allocate, bulk reset, no per-block free |
| [`08-blocking-mpmc-queue`](08-blocking-mpmc-queue) | Bounded MPMC with CV back-pressure (`push`/`pop` wait) |
| [`09-task-graph`](09-task-graph) | Lite task DAG on `03` pool (`precede` / `try_run`) |
| [`10-signal-slot`](10-signal-slot) | Sync signal/slot; member binds (distinct from `04` bus) |
| [`11-fsm`](11-fsm) | Flat FSM; transitions + enter/exit; sync `handle` |

## Conventions

- One idea per folder, numbered (`01-…`, `02-…`)
- C++20 unless a project explicitly needs 23
- CMake + tests required before a project is “done”
- Short README in each folder: what / why / how to build / decisions

## Windows + Ubuntu

Same git clone on both machines. Build with **g++ via CMake** on Ubuntu (work) and
**WSL Ubuntu** on Windows (home), keeping the Windows checkout at `D:\workspace\projects`.
See [`AGENTS.md`](AGENTS.md) and `scripts/wsl-finish-setup.ps1`.

## Backlog

Ordered remaining projects: [`docs/BACKLOG.md`](docs/BACKLOG.md). Next after `11`: **showcase demo**.

## Agent workflow

See [`AGENTS.md`](AGENTS.md). Architecture stays with me; agents implement, build, and test.
