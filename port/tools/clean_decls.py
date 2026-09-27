#!/usr/bin/env python3
"""Remove local declarations in game/*.c that the host build gets elsewhere:
- prototypes of game functions already in game_protos.h (its signatures come
  from the definitions, so they win over ad-hoc local ones);
- hand-written libc prototypes (memcpy, strcat, ...), which clash with the
  host's <string.h>/<stdio.h>.

    python3 tools/clean_decls.py
"""
import re
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
protos = set(re.findall(r"^[^/\n]*?\b(\w+)\s*\([^;]*\);", (ROOT / "game/include/game_protos.h").read_text(), re.M))
LIBC = {"memcpy", "memset", "strcat", "strcpy", "strncpy", "strlen", "strcmp", "strncmp", "sprintf",
        "printf", "rand", "srand", "free", "malloc"}
decl = re.compile(r"^\s*(?:extern\s+)?[A-Za-z_][\w \*]*?\b(\w+)\s*\([^;{]*\)\s*;\s*(/\*.*\*/)?\s*$")
removed = 0
for c in sorted((ROOT / "game").glob("*.c")):
    lines = c.read_text().split("\n")
    keep = []
    for l in lines:
        m = decl.match(l)
        if m and (m.group(1) in protos or m.group(1) in LIBC):
            removed += 1
            continue
        keep.append(l)
    c.write_text("\n".join(keep))
print(f"removed {removed} local declarations")
