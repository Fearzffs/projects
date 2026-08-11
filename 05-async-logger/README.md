# 05 — Async Logger

Non-blocking logger that enqueues records into [`02-spsc-ring-buffer`](../02-spsc-ring-buffer) and writes them on a dedicated consumer thread (C++20).

## Why this project

Classic SPSC use case: hot-path `try_log` must not wait on disk/console I/O.

| Choice | This project |
|---|---|
| Queue | `02` lock-free SPSC |
| API | Non-blocking `try_log` |
| Writer | One dedicated consumer thread |
| Shutdown | Drain remaining records, then join |

Shows:

- composing a prior portfolio primitive (`02`) into a product-shaped component
- producer mutex so many app threads may call `try_log` while the queue still sees one producer
- injectable sink for tests (default writes to stdout/stderr)
- `console_sink()` / `file_sink(path)` helpers; file keeps one `ofstream` open
- CMake + GoogleTest + CI matching the template

## Build & test

```bash
cmake -S 05-async-logger -B 05-async-logger/build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=g++
cmake --build 05-async-logger/build -j"$(nproc)"
cd 05-async-logger/build && ctest --output-on-failure
```

## API sketch

```cpp
klib::AsyncLogger logger(1024);  // default: console_sink()

logger.try_log(klib::LogLevel::info, "ready");

// file output (one handle for the logger lifetime):
klib::AsyncLogger to_disk(1024, klib::AsyncLogger::file_sink("app.log"));

// tests / custom output:
klib::AsyncLogger custom(64, [](klib::LogLevel level, std::string_view msg) {
    // ...
});

custom.shutdown();  // drain + join; destructor also calls shutdown
```

## Thread / lifetime contracts

- **`try_log`:** many app threads OK (serialized by an internal producer mutex); never blocks on I/O; returns false if full or after shutdown.
- **Writer:** one dedicated consumer thread owns the sink calls.
- **`shutdown()`:** stop accepts, drain remaining records through the sink, join writer. Safe to call more than once; destructor also shuts down.
- **Do not** shut down the thread pool *under* a logger that still needs to finish draining if the sink or callers depend on that pool — logger has its own writer thread, but anything the sink touches must outlive drain.
- Drop policy when full: record is not enqueued (caller sees `false`); not a blocking back-pressure story (contrast `08`).

## Decisions

| Choice | Rationale |
|---|---|
| SPSC + producer mutex | Queue stays a true SPSC; mutex serializes producers only |
| `try_log` never blocks on I/O | Matches portfolio non-blocking style; drops when full |
| Dedicated writer thread | Keeps sink off the caller; clear consumer role |
| Injectable `Sink` | Deterministic tests without sleep-polling the console |
| `file_sink` owns one `ofstream` | Avoid open/close per line; path is explicit |
| No retrofit into `01`–`04` | One idea per folder; composition waits for a showcase app |
