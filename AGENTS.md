# AGENTS.md

Instructions for Cursor agents working in this repository.

## Ownership

- The human lead owns architecture, public APIs, and “done.”
- Agents own scaffolding, implementation drafts, tests, CI plumbing, and fix-up loops.
- Do **not** redesign public APIs or add new portfolio projects unless the user asked.

## Repo layout

- Top-level numbered folders: `01-ring-buffer`, `02-…`
- Each project is self-contained: `README.md`, `CMakeLists.txt`, `include/` or `src/`, `tests/`
- Prefer extending the current project over creating shared frameworks early

## Build & test

Use `g++` explicitly: on some Cloud Agent images the default `c++` is Clang without a usable `libstdc++` link line.

### 01 — ring buffer

```bash
cmake -S 01-ring-buffer -B 01-ring-buffer/build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=g++
cmake --build 01-ring-buffer/build -j"$(nproc)"
ctest --test-dir 01-ring-buffer/build --output-on-failure
```

### 02 — SPSC ring buffer

```bash
cmake -S 02-spsc-ring-buffer -B 02-spsc-ring-buffer/build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=g++
cmake --build 02-spsc-ring-buffer/build -j"$(nproc)"
ctest --test-dir 02-spsc-ring-buffer/build --output-on-failure
```

### 03 — thread pool

```bash
cmake -S 03-thread-pool -B 03-thread-pool/build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_COMPILER=g++
cmake --build 03-thread-pool/build -j"$(nproc)"
ctest --test-dir 03-thread-pool/build --output-on-failure
```

## Windows (WSL)

Home PC builds through **WSL Ubuntu**; the git clone stays on the Windows drive
(`D:\workspace\projects` → `/mnt/d/workspace/projects`). Work PC stays native Ubuntu.

- One-time (after WSL feature install + reboot):  
  `powershell -NoProfile -ExecutionPolicy Bypass -File .\scripts\wsl-finish-setup.ps1`
- Prefer Cursor **WSL: Open Folder in WSL…** on `/mnt/d/workspace/projects` for IntelliSense + gdb.
- If the workspace is opened from Windows, `.vscode/tasks.json` still routes build/test through `wsl.exe`.

## Definition of done

1. Configures cleanly with CMake
2. Builds with no new warnings in touched targets (treat warnings as errors when easy)
3. All discovered tests pass via `ctest --output-on-failure`
4. Project README still matches the API
5. No drive-by refactors outside the requested scope

## Cursor Cloud specific instructions

- After code changes, always rebuild and run the relevant `ctest` command above
- Prefer fixing compile/test failures before opening or updating a PR
- If blocked by toolchain/network (e.g. FetchContent), report the exact error and stop
- Keep commits focused; do not rewrite unrelated portfolio folders
