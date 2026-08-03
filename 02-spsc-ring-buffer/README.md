# 02 — SPSC Ring Buffer

Lock-free fixed-capacity FIFO for exactly one producer thread and one consumer thread (C++20).

## Why this project

Follow-up to [`01-ring-buffer`](../01-ring-buffer): same FIFO shape, different tradeoff.

| | `01` mutex ring | `02` SPSC lock-free |
|---|---|---|
| Threads | many producers + many consumers | exactly 1 + 1 |
| Sync | `std::mutex` | atomics + `acquire`/`release` |
| Full buffer | optional overwrite | `try_push` returns false |
| Goal | flexible correctness | low-latency hot path |

Shows:

- why the SPSC restriction exists (each index is single-writer)
- cache-line separation of read/write cursors (false sharing)
- power-of-two capacity with bitwise mask
- CMake + GoogleTest + CI matching the portfolio template

## Build & test

```bash
cmake -S 02-spsc-ring-buffer -B 02-spsc-ring-buffer/build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=g++
cmake --build 02-spsc-ring-buffer/build -j"$(nproc)"
cd 02-spsc-ring-buffer/build && ctest --output-on-failure
```

## Debug tests in Cursor / VS Code

1. Extensions (`Ctrl+Shift+X`) → install **Anysphere C/C++** (`@id:anysphere.cpptools`). Optional: `@id:vadimcn.vscode-lldb` (CodeLLDB).
2. Open the **repo root** as the workspace folder (the folder that contains `.vscode/` and `02-spsc-ring-buffer/`).
3. Set a breakpoint in `tests/test_spsc_ring_buffer.cpp` or `include/spsc_ring_buffer/spsc_ring_buffer.hpp`.
4. **Build:** `Ctrl+Shift+B` (default task builds SPSC).
5. **Run tests (no debugger):** `Ctrl+Shift+P` → **Tasks: Run Test Task**.
6. **Debug:** Run and Debug → pick:
   - **Debug spsc_ring_buffer_tests (all)**
   - **Debug ConcurrentSingleProducerSingleConsumer**
   - **Debug spsc_ring_buffer_tests (one test)** — prompts for a `--gtest_filter`
7. Needs `g++` and `gdb` (`sudo apt install build-essential gdb cmake` on Ubuntu).

## API sketch

```cpp
portfolio::SpscRingBuffer<int> q(5);  // capacity() == 8 (rounded up to power of two)
q.try_push(42);                       // false if full — producer only
auto front = q.try_pop();             // std::optional<int> — consumer only
```

## Decisions

| Choice | Rationale |
|---|---|
| SPSC only | One writer per cursor; no CAS loops or ABA |
| No auto-overwrite | Dropping a slot while the consumer may read it is racy |
| No `try_pop_back` | Keeps producer/consumer roles fixed and the algorithm simple |
| Round capacity to power of two | Index with `i & (capacity - 1)` instead of modulo |
| Separate cache lines for cursors | Avoid false sharing between producer and consumer cores |
| Header-only | Easy to drop into other experiments |
| Deleted move/copy | Concurrent atomics do not compose with relocating the object |
