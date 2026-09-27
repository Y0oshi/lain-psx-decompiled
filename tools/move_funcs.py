#!/usr/bin/env python3
"""Move the tail of a C file (from one function onward) into a new file.

Used when a source file is split at an original translation-unit boundary:

    tools/move_funcs.py src/game/OLD.c src/game/NEW.c func_XXXXXXXX

NEW.c gets `#include "common.h"`, every top-level declaration from OLD.c that
precedes the cut (externs, typedefs, macros, inline helpers; function bodies
and their guards are dropped), then everything from func_XXXXXXXX onward with
INCLUDE_ASM paths rewritten. OLD.c is truncated at the cut.
"""
import re
import sys
from pathlib import Path

GUARD_RE = re.compile(r"^\s*#\s*if.*\b(NEEDS_RODATA|NEEDS_SDATA|NON_MATCHING)\b")
FUNC_DEF_RE = re.compile(r"^(?!static\s+(__)?inline|extern\s+inline|typedef|struct|union|enum|#)"
                         r"[A-Za-z_][\w\s\*]*\b(\w+)\s*\([^;]*$")
INLINE_RE = re.compile(r"^(static\s+(__)?inline(__)?|extern\s+inline)\b")


def block_start(lines: list[str], func: str) -> int:
    """First line of func's block: its guard, leading comment, definition or INCLUDE_ASM."""
    idx = None
    for i, l in enumerate(lines):
        if re.search(rf"INCLUDE_ASM\([^)]*\b{func}\s*\)", l) or \
           (re.match(rf"^[A-Za-z_][\w\s\*]*\b{func}\s*\(", l) and not l.rstrip().endswith(";")):
            idx = i
            break
    if idx is None:
        sys.exit(f"{func} not found")
    # Include an enclosing guard (#if ... ) whose block contains the function.
    depth = 0
    for i in range(idx - 1, -1, -1):
        s = lines[i].strip()
        if s.startswith("#endif"):
            depth += 1
        elif s.startswith("#if"):
            if depth == 0:
                if GUARD_RE.match(lines[i]):
                    idx = i
                break
            depth -= 1
        elif depth == 0 and s and not s.startswith(("#else", "/*", "*", "//")) and i < idx - 50:
            break
    # Include a comment block directly above.
    while idx > 0 and lines[idx - 1].lstrip().startswith(("/*", "*", "//")):
        idx -= 1
    return idx


def declarations(lines: list[str]) -> list[str]:
    """Top-level declarations only: drop function bodies, INCLUDE_ASM, function guards."""
    out, i, n = [], 0, len(lines)
    guard_stack: list[bool] = []
    while i < n:
        l = lines[i]
        s = l.strip()
        if s.startswith("#if"):
            is_guard = bool(GUARD_RE.match(l))
            guard_stack.append(is_guard)
            if not is_guard:
                out.append(l)
            i += 1
            continue
        if s.startswith(("#else", "#elif")):
            if guard_stack and not guard_stack[-1]:
                out.append(l)
            i += 1
            continue
        if s.startswith("#endif"):
            if guard_stack and not guard_stack.pop():
                out.append(l)
            i += 1
            continue
        if "INCLUDE_ASM(" in l or "INCLUDE_RODATA(" in l or s.startswith('#include "common.h"'):
            i += 1
            continue
        if FUNC_DEF_RE.match(l) and not INLINE_RE.match(l):
            # Skip the whole function body; also drop its comment block already emitted.
            while out and out[-1].lstrip().startswith(("/*", "*", "//")):
                out.pop()
            depth, seen = 0, False
            while i < n:
                depth += lines[i].count("{") - lines[i].count("}")
                seen = seen or "{" in lines[i]
                i += 1
                if seen and depth == 0:
                    break
            continue
        out.append(l)
        i += 1
    # Collapse runs of blank lines.
    res: list[str] = []
    for l in out:
        if not (l.strip() == "" and res and res[-1].strip() == ""):
            res.append(l)
    return res


def main() -> None:
    old, new, func = Path(sys.argv[1]), Path(sys.argv[2]), sys.argv[3]
    lines = old.read_text().split("\n")
    cut = block_start(lines, func)
    head, tail = lines[:cut], lines[cut:]
    old_dir, new_dir = f"asm/nonmatchings/game/{old.stem}", f"asm/nonmatchings/game/{new.stem}"
    tail = [l.replace(old_dir, new_dir) for l in tail]
    body = ['#include "common.h"', "",
            f"/* Declarations carried over from {old.name} (same original headers). */"]
    body += declarations(head[1:] if head and head[0].startswith('#include "common.h"') else head)
    body += [""] + tail
    new.write_text("\n".join(body).rstrip() + "\n")
    old.write_text("\n".join(head).rstrip() + "\n")
    print(f"moved {len(tail)} lines from {old.name} to {new.name} (cut at line {cut + 1})")


if __name__ == "__main__":
    main()
