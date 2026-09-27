#!/usr/bin/env python3
"""Check every C function in src/game against the original, per toolchain setting.

Compiles each file once per ASPSX version (C branch of NEEDS_RODATA/NON_MATCHING
guards enabled) and diffs every function that has C. Runs in the container:

    tools/verify_all.py [--aspsx 2.56,2.79] [--gcc 2.8.1-psx]
"""
import argparse
import os
import re
import subprocess
import sys
import tempfile
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import probe  # noqa: E402

ROOT = Path(__file__).resolve().parent.parent
GAME_FUNCS = {"main"} | set(re.findall(r"^(\w+)\s*=\s*0x[0-9A-Fa-f]+;.*type:func",
                                      (ROOT / "config/symbol_addrs.txt").read_text(), re.M))


def c_functions(c_file: Path) -> list[str]:
    """Functions defined in C (compiled with -DNEEDS_RODATA -DNEEDS_SDATA -DNON_MATCHING)."""
    text = c_file.read_text()
    defined = set(re.findall(r"^(?!static)[\w \*]*?\b(\w+)\s*\([^;{]*\)\s*\{", text, re.M))
    names = {n for n in defined if n in GAME_FUNCS or re.fullmatch(r"func_[0-9A-F]{8}", n)}
    return sorted(names)


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--aspsx", default="2.56,2.79")
    ap.add_argument("--gcc", default="2.8.1-psx")
    args = ap.parse_args()
    versions = args.aspsx.split(",")
    rows = {}
    with tempfile.TemporaryDirectory() as tmp:
        for c in sorted((ROOT / "src/game").glob("*.c")):
            funcs = c_functions(c)
            for v in versions:
                obj = os.path.join(tmp, f"{c.stem}_{v}.o")
                env = dict(os.environ, GCC_VER=args.gcc, ASPSX_VER=v,
                           CPPFLAGS_EXTRA="-DNEEDS_RODATA -DNEEDS_SDATA -DNON_MATCHING")
                r = subprocess.run(["tools/cc.sh", str(c.relative_to(ROOT)), obj],
                                   env=env, capture_output=True, text=True, cwd=ROOT)
                if r.returncode:
                    print(f"{c.name}: compile failed with ASPSX {v}: {r.stderr.strip()[:200]}")
                    continue
                for f in funcs:
                    try:
                        got, relocs = probe.compiled_words(obj, f)
                        rows.setdefault((c.name, f), {})[v] = probe.score(probe.target_words(f), got, relocs)
                    except (StopIteration, SystemExit):
                        rows.setdefault((c.name, f), {})[v] = None
    print(f"{'file':<14} {'function':<16} " + " ".join(f"{v:>6}" for v in versions))
    totals = {v: 0 for v in versions}
    for (fname, f), res in sorted(rows.items()):
        for v in versions:
            totals[v] += res.get(v) == 0
        if len({res.get(v) for v in versions}) > 1 or any(res.get(v) for v in versions):
            print(f"{fname:<14} {f:<16} " + " ".join(f"{str(res.get(v)):>6}" for v in versions))
    print(f"\nfunctions with C: {len(rows)}; matching per ASPSX: " +
          ", ".join(f"{v}={totals[v]}" for v in versions))


if __name__ == "__main__":
    main()
