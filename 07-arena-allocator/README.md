# 07 — Arena (Bump) Allocator

Fixed-capacity bump allocator: fast sequential allocate, bulk `reset`, no per-block free (C++20).

## Why this project

Different axis from `01`–`06`: **lifetime and memory layout**, not threads.

| Choice | This project |
|---|---|
| Allocate | Bump a cursor (`try_allocate`) |
| Free | Whole-arena `reset` only |
| Full | `nullptr` (non-throwing) |

Shows:

- alignment padding in a linear buffer
- why games/compilers love arenas for frame/scratch lifetimes
- destructor responsibility stays with the caller on `reset`
- CMake + GoogleTest + CI matching the template

## Build & test

```bash
cmake -S 07-arena-allocator -B 07-arena-allocator/build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=g++
cmake --build 07-arena-allocator/build -j"$(nproc)"
cd 07-arena-allocator/build && ctest --output-on-failure
```

## API sketch

```cpp
portfolio::Arena arena(4 * 1024);

void* raw = arena.try_allocate(128);
int* xs = arena.try_allocate<int>(32);
auto* name = arena.try_create<std::string>("scratch");

// ... use objects ...
name->~basic_string();  // if non-trivial, destroy before reset
arena.reset();          // reuse the same buffer; no per-block free
```

## Decisions

| Choice | Rationale |
|---|---|
| No individual free | Classic bump trade: speed + simplicity vs flexible reclaim |
| `try_allocate` → nullptr | Matches portfolio non-throwing try_* style |
| `reset` skips destructors | Arena doesn't track live objects; caller owns lifetimes |
| Not thread-safe | Keeps the lesson on memory, not locking |
| Power-of-two alignment | Standard, cheap mask-based align |
