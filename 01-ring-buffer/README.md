# 01 — Ring Buffer

Mutex-synchronized fixed-capacity FIFO ring buffer in modern C++20.

## Why this project

A small, complete library that shows:

- clear public API (`try_push` / `try_pop` / `try_pop_back`, no blocking surprises)
- optional auto-overwrite when full (default on)
- concurrency correctness under multi-producer / multi-consumer load
- CMake + GoogleTest + CI as the template for later portfolio folders

## Build & test

```bash
cmake -S 01-ring-buffer -B 01-ring-buffer/build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=g++
cmake --build 01-ring-buffer/build -j"$(nproc)"
ctest --test-dir 01-ring-buffer/build --output-on-failure
```

## Debug tests in Cursor / VS Code

Microsoft's `ms-vscode.cpptools` does **not** appear in Cursor (licensing). Use Cursor's extension instead.

1. Extensions (`Ctrl+Shift+X`) → search exactly: `@id:anysphere.cpptools`
2. Install **Anysphere C/C++** (`anysphere.cpptools`). Optional fallback debugger: `@id:vadimcn.vscode-lldb` (CodeLLDB).
3. Open the **repo root** as the workspace folder (the folder that contains `.vscode/` and `01-ring-buffer/`).
4. Set a breakpoint in `tests/test_ring_buffer.cpp` or `include/ring_buffer/ring_buffer.hpp`.
5. Run and Debug → pick:
   - **Debug ring_buffer_tests (all)**
   - **Debug ConcurrentProducersConsumers**
   - **Debug ring_buffer_tests (one test)** — prompts for a `--gtest_filter`
6. Needs `g++` and `gdb` (`sudo apt install build-essential gdb cmake` on Ubuntu).

**Build only (no debugger):** `Ctrl+Shift+B` runs the default task `build`  
(configure + compile). If that shortcut opens something else in Cursor:

1. `Ctrl+K` then `Ctrl+S` (Keyboard Shortcuts)
2. Search: `workbench.action.tasks.build`
3. Bind it to `Ctrl+Shift+B`

## API sketch

```cpp
portfolio::RingBuffer<int> q(8);              // auto_overwrite == true (default)
q.try_push(42);                               // if full, drops oldest then inserts
auto front = q.try_pop();                     // oldest — std::optional<int>
auto back = q.try_pop_back();                 // newest — std::optional<int>

portfolio::RingBuffer<int> strict(8, false);  // reject push when full
```

## Decisions

| Choice | Rationale |
|---|---|
| Mutex, not lock-free | Correctness and readability first; lock-free can be a follow-up project |
| Size counter (not “empty slot”) | Full capacity usable; full/empty are unambiguous |
| `try_*` only | Callers control back-pressure; no hidden blocking |
| `auto_overwrite` default on | Typical “latest N samples” use; pass `false` for strict full-buffer rejection |
| Header-only | Easy to drop into other experiments |
