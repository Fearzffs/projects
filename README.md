# C++ Portfolio

Small, finished C++ projects. Each folder is independent and meant to be readable in about ten minutes.

I am a lead C++ developer. This repo is for staying sharp with modern C++ and for working seriously with AI agents — not a beginner tutorial dump.

## Projects

| Folder | Focus |
|---|---|
| [`01-ring-buffer`](01-ring-buffer) | Mutex-synchronized fixed-capacity FIFO, C++20, GoogleTest |

## Conventions

- One idea per folder, numbered (`01-…`, `02-…`)
- C++20 unless a project explicitly needs 23
- CMake + tests required before a project is “done”
- Short README in each folder: what / why / how to build / decisions

## Agent workflow

See [`AGENTS.md`](AGENTS.md). Architecture stays with me; agents implement, build, and test.
