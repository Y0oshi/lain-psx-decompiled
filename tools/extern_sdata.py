#!/usr/bin/env python3
"""Filter between cc1 and maspsx: small externs via $gp.

Turn `.extern sym, size` (size <= -G) into `.comm sym,size` so maspsx addresses
them via $gp. The original ASPSX used cc1's .extern size hints; maspsx only
recognises symbols defined in the current file. A common symbol allocates
nothing and the linker resolves it to the real definition.

(Jump tables keep cc1's `.align 3`: alignment is relative to each object's
.rodata start, and the linker places sections 4-aligned (SUBALIGN(4)), exactly
like psylink. A table that looks misaligned means a translation-unit boundary.)

    usage: ... | extern_sdata.py <G value> | maspsx ...
"""
import re
import sys

limit = int(sys.argv[1])
extern_re = re.compile(r"^\s*\.extern\s+([\w.$]+),\s*(\d+)\s*$")
for line in sys.stdin:
    m = extern_re.match(line)
    if m and 0 < int(m.group(2)) <= limit:
        line = f"\t.comm\t{m.group(1)},{m.group(2)}\n"
    sys.stdout.write(line)
