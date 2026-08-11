# Learning gaps tracker

Short morning drill (before deep / lab work). Re-quiz the open gaps; update scores.
Do **not** turn this into a long theory session — 15–30 minutes max.
Then one deep block (harden a primitive or a Phase-2 theme).

CP (1–4 problems/day) stays separate.

## Ritual

1. Pick gaps: prefer **1 concurrency + 1 C++-core** when energy allows (else one only).
2. 2–4 quiz questions each (teach-back). Concurrency: bug hunts in `02`/`05`/`08` once wording is warm. C++ core: banks C–F.
3. Update **Last checked** + **Score (1–5)** + one-line note (`0` = not assessed yet).
4. Then start the day’s deep block (lab or theme).

## Score meaning

| Score | Meaning |
|---|---|
| 1 | Guessing / wrong mental model |
| 2 | Idea nearby, wrong mechanism |
| 3 | Works with a nudge |
| 4 | Explains correctly cold |
| 5 | Can spot bugs / teach it cleanly |

## Open gaps

| Gap | Focus check | Last checked | Score | Note |
|---|---|---|---|---|
| Stack vs heap | locals vs `new`/vector buffer; arena pointers | 2026-08-06 | 4 | A5 false→fixed; solid on vector |
| Dangling / lifetime | return `&local`; use-after-scope | 2026-08-07 | 4 | Leak→dangling aha; arena vs &local clear |
| Atomics publish | release store after payload; acquire before read | 2026-08-06 | 4 | SPSC aha closed B1 |
| Atomic ≠ whole object | nearby non-atomics still race | 2026-08-06 | 4 | B2 clean |
| Relaxed counters | RMW total OK; not for publishing data | 2026-08-11 | 3 | Phase 0: drill bank below — push to 4+ |
| CV + mutex | wait unlocks; avoids lost wakeup | 2026-08-11 | 4 | Lost-wakeup timeline cold; lock+atomic wait was the missing piece |
| CV predicate | re-check condition; spurious/wrong wake | 2026-08-11 | 4 | Before-sleep check locked in; after-wake = spurious/stolen item |
| notify_one vs all | one worker vs shutdown/broadcast | 2026-08-06 | 4 | Clear |
| Relaxed vs release/acquire | one sentence + counterexample | 2026-08-11 | 3 | Phase 0 focus — wording still soft |
| `unique_ptr` / `shared_ptr` / `weak_ptr` | ownership, control block, cycles | — | 0 | Not assessed — bank C |
| Lvalue / rvalue / `std::move` | value category vs type; move ≠ magic | — | 0 | Not assessed — bank D |
| Inheritance / virtual | vtable, dtor, slicing, override | — | 0 | Not assessed — bank E |
| Design patterns (core set) | when/why; not memorizing UML | — | 0 | Not assessed — bank F (baseline) |

## Morning queue (rotate)

**Concurrency (Phase 0 until 4+):**
1. Relaxed vs release-acquire (bank A)
2. Relaxed counters vs publishing data (bank B)
3. Lost wakeup + predicate (keep warm at 4)

**C++ core (alternate mornings — diagnostic then drill):**
4. Smart pointers (bank C)
5. Value categories + `std::move` (bank D)
6. Inheritance / virtual (bank E)
7. Design patterns baseline (bank F) — short; one pattern deep per session once scored

Warm smoke (any leftover minutes): stack/heap + atomics publish.

**Session rule:** 1 concurrency gap + 1 C++-core gap when energy allows; otherwise one only. Still 15–30 min total.

## Phase 0 — Wording drills (close to 4+)

Agent: ask cold; human answers; then reveal the **Target** line. Update the table after each session.

### A — Relaxed vs release/acquire

1. One sentence: what does `release` store + `acquire` load give you that `relaxed` does not?
   - **Target:** A happens-before edge so prior writes by the releaser become visible to the acquirer; relaxed only constrains that atomic’s own RMW/load/store, not surrounding data.
2. Counterexample: writer fills a buffer then `store(ready, relaxed)`; reader `load(ready, relaxed)==true` and reads the buffer. What’s wrong?
   - **Target:** No happens-before; reader may see stale/torn buffer even when it sees `ready==true`.
3. Bug hunt: in `02` SPSC, which cursor uses release/acquire and why must the *payload* write happen before the release?
   - **Target:** Producer writes slot then release-stores write cursor; consumer acquire-loads write cursor then reads slot — order of payload before release is the publish.

### B — Relaxed counters vs publishing data

1. When is `fetch_add(..., relaxed)` enough?
   - **Target:** When you only need a total count and never use that atomic to publish other memory.
2. “I used relaxed and still got the correct final count after join” — does that prove publishing was safe?
   - **Target:** No. Joins provide synchronization; the count being right says nothing about data races on nearby objects.
3. Drop counter in a queue vs “queue not empty” flag: which may be relaxed, which needs publish semantics?
   - **Target:** Drop/lost *count* can be relaxed; a flag/cursor that means “slot is readable” needs release/acquire (or mutex) with the payload.

### Phase 0 done when

Both **Relaxed counters** and **Relaxed vs release/acquire** sit at **4+** for two consecutive drill days.

## C++ core banks (expand mornings)

First time through each bank is **diagnostic** (score honestly 1–5). Later sessions reuse harder variants / bug hunts.

### C — `unique_ptr` / `shared_ptr` / `weak_ptr`

1. Who deletes the object for `unique_ptr` vs `shared_ptr`? Can you copy each?
   - **Target:** `unique_ptr` — sole owner, deleter on destroy/reset; move-only. `shared_ptr` — shared ownership via control block; copyable; last owner deletes.
2. What does `weak_ptr` solve that `shared_ptr` alone cannot? How do you use it safely?
   - **Target:** Break/observe without extending lifetime (cycles, caches). `lock()` → `shared_ptr` or empty; never dereference `weak_ptr` directly.
3. `shared_ptr` to stack object / `this` without `enable_shared_from_this` — what’s wrong?
   - **Target:** Dual ownership or wrong control block → double-free / dangling. Stack must not be managed by smart ptr; `shared_from_this` needs prior `shared_ptr` ownership.

### D — Lvalue / rvalue / `std::move`

1. Is `std::move(x)` a move? What does it actually do?
   - **Target:** It is an rvalue cast (`static_cast<T&&>`); enables move ctor/assign if they exist; does not move by itself.
2. Name one lvalue and one rvalue in `int a = 1; foo(a); foo(1);`.
   - **Target:** `a` is an lvalue; `1` (and often the expression `std::move(a)`) is an rvalue (xvalue for move result).
3. After `auto b = std::move(a);` on a `string a`, what can you assume about `a`?
   - **Target:** Valid but unspecified state (often empty for string); do not assume contents; safe to assign/destroy.

### E — Inheritance / virtual

1. Why is a public polymorphic base destructor usually `virtual`?
   - **Target:** `delete` via base pointer must run derived destructor; otherwise UB / resource leaks.
2. What is object slicing? Give a one-line example pattern.
   - **Target:** Assign/pass derived by value as base → derived part chopped. Prefer refs/pointers/`unique_ptr<Base>`.
3. `override` vs omitting it; when do you need `virtual` on the derived function?
   - **Target:** `override` checks you actually override; derived need not repeat `virtual` if base was virtual, but `override` is the safety net.

### F — Design patterns (baseline map)

Score the **map**, not trivia. Ask: “What problem does it solve? Where have you seen it in *our* portfolio or code?”

| Pattern | One-line problem | Portfolio hint |
|---|---|---|
| RAII | Tie resource lifetime to scope | locks, `Connection`, handles |
| Observer | Notify dependents of change | `10` signal/slot; `04` bus |
| Strategy | Swap algorithm/behavior | injectable logger sink |
| State | Behavior depends on mode | `11` FSM |
| Command | Queue/undo work as objects | pool tasks / graph nodes (loose) |
| Factory | Centralize creation | (often overkill — say when not to) |
| Singleton | One global instance | (usually avoid — say why) |

Diagnostic prompts:
1. Pick RAII vs Observer: which fits “disconnect when `Connection` dies”?
2. Which pattern is our FSM closest to — and how is it *not* a full GoF State soup?
3. Name one pattern you would **not** force into `01` ring buffer, and why.

### C++ core “caught up” when

Banks C–E at **4+**; bank F at **3+** on the map (4+ = can match pattern↔problem without the table).

## Deep track (post-demo)

North star: C++ systems / concurrency depth. Spine folders stay; we **harden** and **theme**, we do not invent `13+` unless asked.

### Lab rotation (Phase 1)

1. Correctness under stress (longer tests; ASAN/TSAN)
2. Shutdown / lifetime order
3. API honesty (thread contracts in READMEs)
4. Showcase only if it teaches something new

Sanitizers (any project that includes `PortfolioIde.cmake`):

```bash
cmake -S 08-blocking-mpmc-queue -B 08-blocking-mpmc-queue/build-tsan \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=g++ -DPORTFOLIO_TSAN=ON
cmake --build 08-blocking-mpmc-queue/build-tsan -j"$(nproc)"
cd 08-blocking-mpmc-queue/build-tsan && ctest --output-on-failure

cmake -S 05-async-logger -B 05-async-logger/build-asan \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=g++ -DPORTFOLIO_ASAN=ON
# … build + ctest likewise
```

Do not enable ASAN and TSAN together.

### Weekly themes (Phase 2)

| Week theme | Teach-back focus | Spike lab |
|---|---|---|
| Memory model | happens-before on *your* SPSC/logger | Annotate / quiz `02`/`05` orders |
| Back-pressure | drop vs block vs wait-free | Stress `08` + compare `01` try_* |
| Scheduling | pool + timer fairness | Read `03`/`06`; quiz shutdown |
| False sharing | padding / hot cursors | Measure or reason on ring cursors |
| Lock-free vs mutex | when *not* lock-free | Design teach-back cold |
| Design cold | logger / timer / job system | Whiteboard then compare to code |

### “Best ever” signal (Phase 3)

Cold: ~5 min each on `01`–`12` (threads + failure modes); LEARNING gaps at 4–5 for two weeks; find and fix one real contract/bug with a test.

## First session back

1. Hard drill: Phase 0 banks A + B (above), **or** if those are warm, diagnostic pass on banks C–D (smart ptr + move).
2. Deep: run `08` stress tests (+ TSAN build if time).
3. Update scores in the table; note theme for next deep day. Next mornings: rotate C++ core (C→F) alongside concurrency.

### Lab status (2026-08-11)

- Stress tests landed: `BlockingMpmcQueue.StressManyProducersConsumers`, `ShutdownWhileMixedTryAndBlocking`; `AsyncLogger.StressConcurrentProducersThenDrain`, `ShutdownIdempotentAndRejectsAfter`.
- `08` Debug + TSAN ctest: all passed (including stress).
- `05` Debug ctest: all passed (including stress).
- API contracts documented in `05` / `08` / `11` READMEs.
- First live drill (banks A+B scores) still awaits the human after vacation — do not bump Phase 0 scores to 4 until teach-back.

## Done well recently (don’t ignore)

- SPSC cursor memory orders
- Async logger / timer pipeline teach-back
- Arena purpose + `nullptr` full path
- Showcase FSM ownership (`post` vs `handle`), Queued `process()`, flush single-flight
