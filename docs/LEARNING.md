# Learning gaps tracker

Short morning drill (before portfolio work). Re-quiz the open gaps; update scores.
Do **not** turn this into a long theory session — 15–30 minutes max.

## Ritual

1. Pick 1–2 open gaps below (or whatever felt fuzzy yesterday).
2. 2–4 quiz questions each (teach-back).
3. Update **Last checked** + **Score (1–5)** + one-line note.
4. Then start the day’s project.

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
| Relaxed counters | RMW total OK; not for publishing data | 2026-08-10 | 3 | A1 solid; A3 still “correct value” wording — keep drilling |
| CV + mutex | wait unlocks; avoids lost wakeup | 2026-08-11 | 4 | Lost-wakeup timeline cold; lock+atomic wait was the missing piece |
| CV predicate | re-check condition; spurious/wrong wake | 2026-08-11 | 4 | Before-sleep check locked in; after-wake = spurious/stolen item |
| notify_one vs all | one worker vs shutdown/broadcast | 2026-08-06 | 4 | Clear |

## Morning queue (rotate)

1. Relaxed vs release-acquire one-liner (keep drilling wording)
2. Lost wakeup + predicate (keep warm at 4)
3. Quick stack/heap + atomics smoke check (keep warm)

## Done well recently (don’t ignore)

- SPSC cursor memory orders
- Async logger / timer pipeline teach-back
- Arena purpose + `nullptr` full path
