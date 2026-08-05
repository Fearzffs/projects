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
portfolio::AsyncLogger logger(1024);  // default: console_sink()

logger.try_log(portfolio::LogLevel::info, "ready");

// file output (one handle for the logger lifetime):
portfolio::AsyncLogger to_disk(1024, portfolio::AsyncLogger::file_sink("app.log"));

// tests / custom output:
portfolio::AsyncLogger custom(64, [](portfolio::LogLevel level, std::string_view msg) {
    // ...
});

custom.shutdown();  // drain + join; destructor also calls shutdown
```

## Decisions

| Choice | Rationale |
|---|---|
| SPSC + producer mutex | Queue stays a true SPSC; mutex serializes producers only |
| `try_log` never blocks on I/O | Matches portfolio non-blocking style; drops when full |
| Dedicated writer thread | Keeps sink off the caller; clear consumer role |
| Injectable `Sink` | Deterministic tests without sleep-polling the console |
| `file_sink` owns one `ofstream` | Avoid open/close per line; path is explicit |
| No retrofit into `01`–`04` | One idea per folder; composition waits for a showcase app |
