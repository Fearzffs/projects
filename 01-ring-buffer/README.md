# 01 — Ring Buffer

Mutex-synchronized fixed-capacity FIFO ring buffer in modern C++20.

## Why this project

A small, complete library that shows:

- clear public API (`try_push` / `try_pop`, no blocking surprises)
- concurrency correctness under multi-producer / multi-consumer load
- CMake + GoogleTest + CI as the template for later portfolio folders

## Build & test

```bash
cmake -S 01-ring-buffer -B 01-ring-buffer/build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=g++
cmake --build 01-ring-buffer/build -j"$(nproc)"
ctest --test-dir 01-ring-buffer/build --output-on-failure
```

## API sketch

```cpp
portfolio::RingBuffer<int> q(8);
q.try_push(42);
auto item = q.try_pop();  // std::optional<int>
```

## Decisions

| Choice | Rationale |
|---|---|
| Mutex, not lock-free | Correctness and readability first; lock-free can be a follow-up project |
| Size counter (not “empty slot”) | Full capacity usable; full/empty are unambiguous |
| `try_*` only | Callers control back-pressure; no hidden blocking |
| Header-only | Easy to drop into other experiments |
