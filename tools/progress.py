#!/usr/bin/env python3
"""Decompilation progress for the game code (PsyQ libraries excluded).

A function is:
  matched      - C is compiled into the build (no INCLUDE_ASM left for it)
  needs_rodata - C matches and owns its .rodata (#ifdef NEEDS_RODATA, defined in the build)
  needs_data   - C matches but emits .sdata/.data, which isn't split per file yet (#ifdef NEEDS_SDATA)
  nonmatching  - functionally equivalent C, not byte-identical (#ifdef NON_MATCHING)
  asm          - still INCLUDE_ASM only

    tools/progress.py [--files]
"""
import argparse
import re
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
ASM_DIRS = [ROOT / "asm/nonmatchings/game", ROOT / "asm/matchings/game"]
SRC = ROOT / "src/game"
GUARDS = {"NEEDS_RODATA": "needs_rodata", "NEEDS_SDATA": "needs_data", "NON_MATCHING": "nonmatching"}


def function_sizes() -> dict[str, int]:
    sizes = {}
    for s in (p for d in ASM_DIRS for p in d.glob("*/*.s")):
        m = re.search(r"^nonmatching (\w+), (0x[0-9A-Fa-f]+)", s.read_text(), re.M)
        if m:
            sizes[m.group(1)] = int(m.group(2), 16)
    return sizes


def statuses(c_file: Path) -> dict[str, str]:
    """Map function name -> status for every INCLUDE_ASM in the file."""
    out = {}
    stack: list[tuple[str | None, bool]] = []  # (guard name, in #else branch)
    for line in c_file.read_text().splitlines():
        s = line.strip()
        if m := re.match(r"#\s*if(?:n?def)?\s+(\w+)", s):
            stack.append((m.group(1), False))
        elif s.startswith("#else") and stack:
            stack[-1] = (stack[-1][0], True)
        elif s.startswith("#endif") and stack:
            stack.pop()
        elif m := re.search(r'INCLUDE_ASM\(\s*"[^"]*",\s*(\w+)\s*\)', s):
            status = "asm"
            for guard, in_else in stack:
                if in_else and guard in GUARDS:
                    status = GUARDS[guard]
            out[m.group(1)] = status
    return out


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--files", action="store_true", help="per-file breakdown")
    args = ap.parse_args()

    sizes = function_sizes()
    status = {name: "matched" for name in sizes}
    per_file = {}
    for c in sorted(SRC.glob("*.c")):
        st = statuses(c)
        status.update(st)
        names = [n.stem for d in ASM_DIRS for n in (d / c.stem).glob("*.s")]
        per_file[c.name] = Counter(status[n] for n in names if n in sizes)

    total_bytes = sum(sizes.values())
    by_status = Counter()
    bytes_by = Counter()
    for name, st in status.items():
        by_status[st] += 1
        bytes_by[st] += sizes[name]

    print(f"game functions: {len(sizes)}  ({total_bytes:,} bytes)")
    for st in ("matched", "needs_rodata", "needs_data", "nonmatching", "asm"):
        pct = 100 * bytes_by[st] / total_bytes
        print(f"  {st:<13} {by_status[st]:4d} funcs  {bytes_by[st]:7,} bytes  {pct:5.1f}%")
    done = bytes_by["matched"] + bytes_by["needs_rodata"] + bytes_by["needs_data"]
    print(f"matching C (incl. pending rodata/data): {100 * done / total_bytes:.1f}%")
    if args.files:
        for f, c in per_file.items():
            print(f"  {f:<14} " + "  ".join(f"{k}={c[k]}" for k in
                  ("matched", "needs_rodata", "needs_data", "nonmatching", "asm")))


if __name__ == "__main__":
    main()
