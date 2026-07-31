# How to run Cursor agents on this repo

This is the human-facing cheat sheet. Machine instructions live in `AGENTS.md`.

## Two kinds of agents

| Mode | Use when |
|---|---|
| **IDE Agent** (local) | Designing APIs, Plan Mode, reviewing diffs live |
| **Cloud Agent** | “Implement / test / open PR” while you step away |

## First-time setup (you)

1. Open [Cloud Agents environments](https://cursor.com/dashboard/cloud-agents#environments) and ensure this repo can configure + build + test (snapshot once green).
2. Keep `AGENTS.md` and `.cursor/rules/` accurate — that is how agents know the quality bar.
3. Optional: create an Automation at [cursor.com/automations](https://cursor.com/automations) that runs on PR push: build + `ctest`, comment results.

## Prompt pattern that works

1. Goal (behavior / API)
2. Constraints (folders allowed; do not redesign public API)
3. Acceptance (`cmake` + `ctest` must be green)
4. Attach context with `@01-ring-buffer` / `@AGENTS.md`

Example:

> Extend `01-ring-buffer` with X. Do not change the existing public API unless required. Configure Debug, build, run `ctest --test-dir 01-ring-buffer/build --output-on-failure`. Fix failures. Open/update the PR only when green.

## Your job vs the agent’s job

- **You:** choose the next project, approve API shape, review PRs
- **Agent:** implement, add tests, run the build, fix compile/test breaks, open PR
