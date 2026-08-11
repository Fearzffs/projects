# 12 — Showcase demo (telemetry)

End-to-end app that glues the portfolio into a small **host telemetry** pipeline (C++20).

## What it does

FSM:

`Boot → Running → Shutdown`

**Running** stays active: samples the host and prints **one line** per tick (`cpu%` / `load` / `tempC`). Every N samples it flushes/aggregates (task graph) without leaving Running. Quits after `--seconds` (default 2; `0` = forever).

Metrics (best-effort):

- CPU % (`/proc/stat`)
- load average (`/proc/loadavg`)
- temperature if present (`/sys/class/thermal/...`)

## Which project plays which role

| Folder | Role |
|---|---|
| `11-fsm` | Session phases |
| `06-timer-scheduler` | Sample clock (`try_run_every`) |
| `08-blocking-mpmc-queue` | Sample ingest queue |
| `09-task-graph` | Flush: drain → aggregate → finalize |
| `03-thread-pool` | Graph / bus / timer callbacks |
| `04-event-bus` | `StatsReady` |
| `10-signal-slot` | Queued `stats_updated` → main `process()` |
| `05-async-logger` (+`02`) | FSM / bus log lines |
| `01-ring-buffer` | Overwriting sample history |
| `07-arena-allocator` | Per-window aggregate scratch |

## Build & run

```bash
cmake -S 12-showcase-demo -B 12-showcase-demo/build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=g++
cmake --build 12-showcase-demo/build -j"$(nproc)"
./12-showcase-demo/build/showcase_demo --seconds 2 --samples 5 --period-ms 200
cd 12-showcase-demo/build && ctest --output-on-failure
```

`--seconds` = how long to run (0 = forever). `--samples` = report every N samples.


## Shutdown order

Timers → sample queue → logger → thread pool (app-owned lifetimes).
