# C++ Portfolio

Small, finished C++ projects. Each folder is independent and meant to be readable in about ten minutes.

I am a lead C++ developer. This repo is for staying sharp with modern C++ and for working seriously with AI agents — not a beginner tutorial dump.

## Projects

| Folder | Focus |
|---|---|
| [`01-ring-buffer`](01-ring-buffer) | Mutex-synchronized fixed-capacity FIFO, C++20, GoogleTest |
| [`02-spsc-ring-buffer`](02-spsc-ring-buffer) | Lock-free SPSC fixed-capacity FIFO, atomics, GoogleTest |
| [`03-thread-pool`](03-thread-pool) | Fixed worker pool, non-blocking try_submit + completion callback |

## Conventions

- One idea per folder, numbered (`01-…`, `02-…`)
- C++20 unless a project explicitly needs 23
- CMake + tests required before a project is “done”
- Short README in each folder: what / why / how to build / decisions

## Windows + Ubuntu

Same git clone on both machines. Build with **g++ via CMake** on Ubuntu (work) and
**WSL Ubuntu** on Windows (home), keeping the Windows checkout at `D:\workspace\projects`.
See [`AGENTS.md`](AGENTS.md) and `scripts/wsl-finish-setup.ps1`.

## Agent workflow

See [`AGENTS.md`](AGENTS.md). Architecture stays with me; agents implement, build, and test.
