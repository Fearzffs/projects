#!/usr/bin/env python3
"""Keep only non-FetchContent entries in compile_commands.json for clangd."""
import json
import sys


def main() -> int:
    if len(sys.argv) != 3:
        print("usage: filter_compile_commands.py IN OUT", file=sys.stderr)
        return 2
    inp, out = sys.argv[1], sys.argv[2]
    with open(inp, encoding="utf-8") as f:
        data = json.load(f)
    keep = [
        e
        for e in data
        if "/_deps/" not in e.get("file", "").replace("\\", "/")
    ]
    with open(out, "w", encoding="utf-8") as f:
        json.dump(keep, f, indent=2)
        f.write("\n")
    print(f"compile_commands: kept {len(keep)} of {len(data)} entries")
    return 0


if __name__ == "__main__":
    sys.exit(main())
