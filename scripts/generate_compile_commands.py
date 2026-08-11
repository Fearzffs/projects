#!/usr/bin/env python3
"""Generate a workspace-root compile_commands.json for clangd / cpptools.

Covers every NN-*/tests/*.cpp with that project's include/ plus declared
cross-folder deps and a discovered GoogleTest include (after first configure).

Usage:
  python3 scripts/generate_compile_commands.py [REPO_ROOT]
"""

import json
import sys
from pathlib import Path

# Cross-folder -I deps (folder name -> other folder names).
# Update when a new project links another portfolio folder.
IDE_DEPS = {
    "04-event-bus": ["03-thread-pool"],
    "05-async-logger": ["02-spsc-ring-buffer"],
    "06-timer-scheduler": ["03-thread-pool"],
    "09-task-graph": ["03-thread-pool"],
    "12-showcase-demo": [
        "01-ring-buffer",
        "02-spsc-ring-buffer",
        "03-thread-pool",
        "04-event-bus",
        "05-async-logger",
        "06-timer-scheduler",
        "07-arena-allocator",
        "08-blocking-mpmc-queue",
        "09-task-graph",
        "10-signal-slot",
        "11-fsm",
    ],
}


def find_gtest_include(root):
    matches = sorted(root.glob("*/build/_deps/googletest-src/googletest/include"))
    return matches[0] if matches else None


def portfolio_projects(root):
    projects = [
        p
        for p in root.iterdir()
        if p.is_dir() and len(p.name) >= 2 and p.name[:2].isdigit() and (p / "include").is_dir()
    ]
    return sorted(projects, key=lambda p: p.name)


def entry_for_test(root, project, test_cpp, gtest_include, cxx):
    includes = [project / "include"]
    for dep in IDE_DEPS.get(project.name, []):
        includes.append(root / dep / "include")

    cmd_parts = [cxx]
    for inc in includes:
        cmd_parts.append("-I{}".format(inc.resolve()))
    if gtest_include is not None:
        cmd_parts.append("-isystem{}".format(gtest_include.resolve()))
    cmd_parts.extend(["-std=c++2a", "-c", str(test_cpp.resolve())])

    directory = project / "build" / "tests"
    if not directory.is_dir():
        directory = project / "build"
    if not directory.is_dir():
        directory = project

    return {
        "directory": str(directory.resolve()),
        "command": " ".join(cmd_parts),
        "file": str(test_cpp.resolve()),
    }


def write_clangd(root, projects):
    """Keep .clangd CompileFlags -I list in sync with discovered projects."""
    lines = [
        "# Generated/updated by scripts/generate_compile_commands.py — do not hand-edit -I list.",
        "# CompilationDatabase covers tests/; CompileFlags.Add helps when editing headers.",
        "CompileFlags:",
        "  Add:",
        "    - -std=c++2a",
    ]
    for project in sorted(projects, key=lambda p: p.name, reverse=True):
        lines.append("    - -I{}/include".format(project.name))
    lines.append("  CompilationDatabase: .")
    lines.append("")
    (root / ".clangd").write_text("\n".join(lines), encoding="utf-8")


def generate(root):
    gtest_include = find_gtest_include(root)
    cxx = "/usr/bin/g++"
    entries = []
    projects = portfolio_projects(root)

    for project in projects:
        tests_dir = project / "tests"
        if not tests_dir.is_dir():
            continue
        for test_cpp in sorted(tests_dir.glob("test_*.cpp")):
            entries.append(
                entry_for_test(root, project, test_cpp, gtest_include, cxx)
            )

    out = root / "compile_commands.json"
    out.write_text(json.dumps(entries, indent=2) + "\n", encoding="utf-8")
    write_clangd(root, projects)
    gtest_note = (
        str(gtest_include)
        if gtest_include
        else "MISSING (configure+build any project once)"
    )
    print(
        "Wrote {} ({} entries); updated .clangd; gtest include: {}".format(
            out, len(entries), gtest_note
        )
    )
    return out


def main():
    root = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else Path(__file__).resolve().parents[1]
    if not (root / "AGENTS.md").is_file():
        sys.stderr.write("error: {} does not look like the portfolio root\n".format(root))
        return 1
    generate(root)
    return 0


if __name__ == "__main__":
    sys.exit(main())
