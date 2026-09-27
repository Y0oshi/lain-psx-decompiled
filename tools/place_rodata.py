#!/usr/bin/env python3
"""Insert INCLUDE_RODATA lines for orphan rodata into the game C files.

splat puts rodata that belongs to a single function into that function's .s
(so INCLUDE_ASM / compiled C provides it) and gives every other rodata symbol
its own asm/nonmatchings/game/<file>/D_*.s. Those orphans must be emitted by the
C file at the right point in address order: just before the first function
whose own rodata comes after them.

    tools/place_rodata.py            # all files; idempotent
"""
import re
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from move_funcs import block_start  # noqa: E402

ROOT = Path(__file__).resolve().parent.parent
DIRS = [ROOT / "asm/nonmatchings/game", ROOT / "asm/matchings/game"]


def first_rodata_addr(s_file: Path) -> int | None:
    text = s_file.read_text()
    if ".section .rodata" not in text:
        return None
    m = re.search(r"^dlabel (?:D|jtbl)_([0-9A-F]{8})", text, re.M)
    return int(m.group(1), 16) if m else None


def main() -> None:
    for c in sorted((ROOT / "src/game").glob("*.c")):
        orphans, funcs = [], []
        for d in DIRS:
            for s in sorted((d / c.stem).glob("*.s")):
                if s.stem.startswith("D_"):
                    orphans.append((int(s.stem[2:], 16), s.stem))
                elif (addr := first_rodata_addr(s)) is not None:
                    funcs.append((addr, s.stem))
        if not orphans:
            continue
        funcs.sort()
        lines = c.read_text().split("\n")
        text = "\n".join(lines)
        inserts: dict[str | None, list[str]] = {}
        for addr, sym in sorted(orphans):
            if f"INCLUDE_RODATA(\"asm/nonmatchings/game/{c.stem}\", {sym})" in text:
                continue
            after = next((f for a, f in funcs if a > addr), None)
            inserts.setdefault(after, []).append(sym)
        for func, syms in inserts.items():
            block = [f'INCLUDE_RODATA("asm/nonmatchings/game/{c.stem}", {s});' for s in syms] + [""]
            if func is None:
                lines += [""] + block
            else:
                i = block_start(lines, func)
                lines[i:i] = block
            print(f"{c.name}: {', '.join(syms)} before {func or 'end of file'}")
        c.write_text("\n".join(lines))


if __name__ == "__main__":
    main()
