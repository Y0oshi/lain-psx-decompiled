#!/usr/bin/env python3
"""Compile a candidate C function under many toolchain settings and diff it
against the original. Used to identify the compiler; runs in the container.

    tools/probe.py func_80028DF4 path/to/candidate.c
"""
import difflib
import itertools
import os
import re
import subprocess
import sys
import tempfile
from pathlib import Path

# Override with comma-separated env vars, e.g. PROBE_GCCS=2.8.1-psx PROBE_OPTS=-O2
GCCS = os.environ.get("PROBE_GCCS", "2.6.3-psx,2.7.2-psx,2.7.2-cdk,2.8.0-psx,2.8.1-psx,2.91.66-psx").split(",")
OPTS = os.environ.get("PROBE_OPTS", "-O1,-O2,-O3").split(",")
ASPSX = os.environ.get("PROBE_ASPSX", "2.34,2.56,2.67,2.79,2.86").split(",")


def target_words(func: str) -> list[int]:
    for path in Path("asm").rglob("*.s"):
        text = path.read_text()
        m = re.search(rf"^glabel {func}\n(.*?)^endlabel {func}", text, re.S | re.M)
        if m:
            return [int.from_bytes(bytes.fromhex(w), "little")
                    for w in re.findall(r"/\* [0-9A-F]+ [0-9A-F]{8} ([0-9A-F]{8}) \*/", m.group(1))]
    sys.exit(f"{func} not found in asm/")


def compiled_words(obj: str, func: str) -> tuple[list[int], set[int]]:
    syms = subprocess.run(["mips-linux-gnu-nm", "-S", obj], capture_output=True, text=True).stdout
    addr, size = next((int(l.split()[0], 16), int(l.split()[1], 16))
                      for l in syms.splitlines() if l.endswith(" " + func) and len(l.split()) == 4)
    raw = subprocess.run(["mips-linux-gnu-objcopy", "-O", "binary", "-j", ".text", obj, "/dev/stdout"],
                         capture_output=True).stdout
    words = [int.from_bytes(raw[i:i + 4], "little") for i in range(addr, addr + size, 4)]
    relocs = subprocess.run(["mips-linux-gnu-objdump", "-r", "-j", ".text", obj],
                            capture_output=True, text=True).stdout
    reloc_offs = {int(l.split()[0], 16) - addr for l in relocs.splitlines()
                  if re.match(r"^[0-9a-f]{8} ", l)}
    return words, reloc_offs


def masked(w: int, relocated: bool) -> int:
    op = w >> 26
    if op in (2, 3):
        return w & 0xFC000000
    return w & 0xFFFF0000 if relocated else w


def score(target: list[int], got: list[int], relocs: set[int]) -> int:
    """Instructions that don't line up after sequence alignment (0 = match)."""
    # Relocated fields can't be compared, so mask them on both sides.
    a = [masked(w, False) if (w >> 26) not in (2, 3) else w & 0xFC000000 for w in target]
    b = [masked(w, i * 4 in relocs) for i, w in enumerate(got)]
    for i, w in enumerate(a):
        if i < len(b) and b[i] != w and (w >> 16) == (b[i] >> 16) and i * 4 in relocs:
            a[i] = b[i]
    sm = difflib.SequenceMatcher(None, a, b, autojunk=False)
    same = sum(blk.size for blk in sm.get_matching_blocks())
    return max(len(a), len(b)) - same


def main() -> None:
    func, src = sys.argv[1], sys.argv[2]
    target = target_words(func)
    results = []
    with tempfile.TemporaryDirectory() as tmp:
        obj = os.path.join(tmp, "t.o")
        for gcc, opt, aspsx in itertools.product(GCCS, OPTS, ASPSX):
            env = dict(os.environ, GCC_VER=gcc, OPTFLAGS=opt, ASPSX_VER=aspsx)
            r = subprocess.run(["tools/cc.sh", src, obj], env=env, capture_output=True, text=True)
            if r.returncode:
                results.append((999, gcc, opt, aspsx, r.stderr.strip().splitlines()[-1:]))
                continue
            got, relocs = compiled_words(obj, func)
            results.append((score(target, got, relocs), gcc, opt, aspsx, ""))
    results.sort()
    for s, gcc, opt, aspsx, err in results:
        print(f"{s:4d} diffs  gcc {gcc:<12} {opt}  aspsx {aspsx}  {err or ''}")


if __name__ == "__main__":
    main()
