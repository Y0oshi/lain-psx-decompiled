#!/usr/bin/env python3
"""List the functions whose bytes differ between build/SLPS_016.03 and the retail EXE.

    tools/whatdiff.py
"""
import bisect
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BUILT = ROOT / "build/SLPS_016.03"
ORIG = ROOT / "extract/disc1/SLPS_016.03"
MAP = ROOT / "build/main.map"
HEADER, VRAM = 0x800, 0x80010000


def symbols() -> list[tuple[int, str]]:
    """Function/data symbols from the linker map, sorted by address."""
    syms = []
    for line in MAP.read_text().splitlines():
        m = re.match(r"\s+0x([0-9a-f]{8})([0-9a-f]{8})?\s+(\w+)\s*$", line)
        if m:
            addr = int(m.group(2) or m.group(1), 16)
            if 0x80010000 <= addr < 0x80200000 and not m.group(3).endswith(("_START", "_END", "_SIZE", "_VRAM")):
                syms.append((addr, m.group(3)))
    return sorted(set(syms))


def main() -> None:
    a, b = BUILT.read_bytes(), ORIG.read_bytes()
    if len(a) != len(b):
        print(f"size differs: built {len(a):#x} vs original {len(b):#x} (a function changed size)")
    syms = symbols()
    addrs = [s[0] for s in syms]
    hits: dict[str, int] = {}
    first: dict[str, int] = {}
    for off in range(HEADER, min(len(a), len(b)), 4):
        if a[off:off + 4] != b[off:off + 4]:
            vaddr = VRAM + off - HEADER
            i = bisect.bisect_right(addrs, vaddr) - 1
            name = syms[i][1] if i >= 0 else "?"
            hits[name] = hits.get(name, 0) + 1
            first.setdefault(name, vaddr)
    if not hits:
        print("identical" if len(a) == len(b) else "common prefix identical")
    for name, n in sorted(hits.items(), key=lambda kv: first[kv[0]])[:40]:
        print(f"{first[name]:08X} {name:<32} {n} words differ")


if __name__ == "__main__":
    main()
