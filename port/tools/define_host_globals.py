#!/usr/bin/env python3
"""Turn each host-typed global's declaration into a definition in its owning file.

Reads the "Host-typed globals" table in game/PORT_TYPES.md; for each row the
owning file is the one in [brackets] in the host-type column. Its
`extern ... D_xxxxxxxx...;` declaration loses `extern` (and gets the element
count when declared with []). Idempotent.
"""
import re
from pathlib import Path

PORT = Path(__file__).resolve().parent.parent
table = (PORT / "game/PORT_TYPES.md").read_text().split("## Host-typed globals", 1)[1].split("\n## ", 1)[0]
done, skipped = 0, []
for row in re.findall(r"^\|\s*([A-Za-z_]\w*)\s*\|[^|]*\|([^|]*)\|", table, re.M):
    sym, host = row
    m = re.search(r"\[(\w+\.c)\]", host)
    if not m:
        skipped.append(sym)  # e.g. an element of another table
        continue
    count = re.search(r"\[(\d+)\]", host)
    path = PORT / "game" / m.group(1)
    text = path.read_text()
    if re.search(rf"^(?!extern)[^\n;]*\b{sym}\b[^\n(]*;", text, re.M) and not re.search(rf"^extern[^\n;]*\b{sym}\b", text, re.M):
        done += 1  # already a definition
        continue
    pat = re.compile(rf"^extern\s+([^;\n]*?\b{sym}\b)(\[\])?([^;\n]*);", re.M)
    mm = pat.search(text)
    if not mm:
        skipped.append(sym)
        continue
    dims = f"[{count.group(1)}]" if mm.group(2) and count else (mm.group(2) or "")
    text = text[:mm.start()] + f"{mm.group(1)}{dims}{mm.group(3)}; /* port: defined here (host-typed) */" + text[mm.end():]
    path.write_text(text)
    done += 1
print(f"{done} defined, skipped: {' '.join(skipped) or 'none'}")
