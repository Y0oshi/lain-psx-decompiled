#!/usr/bin/env python3
"""Drop a guard from a function once its C matches.

    tools/unguard.py <file.c> <func> NON_MATCHING   # C now matches
    tools/unguard.py <file.c> <func> NEEDS_RODATA   # rodata is now owned by the C file
    tools/unguard.py <file.c> <func> NON_MATCHING NEEDS_RODATA  # matches, but needs rodata

Finds the #if/#ifdef block whose #else branch holds INCLUDE_ASM(..., func) and
removes the named condition. If no condition is left, the C branch is kept
unconditionally and the INCLUDE_ASM branch is deleted.
"""
import re
import sys
from pathlib import Path


def main() -> None:
    path, func, guard = Path(sys.argv[1]), sys.argv[2], sys.argv[3]
    replacement = sys.argv[4] if len(sys.argv) > 4 else None
    lines = path.read_text().split("\n")
    asm_i = next(i for i, l in enumerate(lines)
                 if re.search(rf'INCLUDE_ASM\([^)]*\b{func}\s*\)', l))
    # Walk back to the enclosing #else and its #if, tracking nesting.
    depth, else_i, if_i = 0, None, None
    for i in range(asm_i - 1, -1, -1):
        s = lines[i].strip()
        if s.startswith("#endif"):
            depth += 1
        elif s.startswith("#if"):
            if depth == 0:
                if_i = i
                break
            depth -= 1
        elif s.startswith("#else") and depth == 0 and else_i is None:
            else_i = i
    endif_i = next(i for i in range(asm_i + 1, len(lines)) if lines[i].strip().startswith("#endif"))
    if if_i is None or else_i is None:
        sys.exit(f"{func}: no #if/#else guard around its INCLUDE_ASM")

    conds = re.findall(r"\b(NEEDS_RODATA|NEEDS_SDATA|NON_MATCHING)\b", lines[if_i])
    if guard not in conds:
        sys.exit(f"{func}: guard is `{lines[if_i].strip()}`, not {guard}")
    remaining = [c for c in conds if c != guard]
    if replacement and replacement not in remaining:
        remaining.append(replacement)
    if remaining:
        lines[if_i] = f"#ifdef {remaining[0]}"
    else:
        lines = lines[:if_i] + lines[if_i + 1:else_i] + lines[endif_i + 1:]
    path.write_text("\n".join(lines))
    print(f"{func}: removed {guard} guard" + (f", still under {remaining[0]}" if remaining else ""))


if __name__ == "__main__":
    main()
