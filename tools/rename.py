#!/usr/bin/env python3
"""Rename game symbols everywhere they are spelled out.

    tools/rename.py names.tsv          rows: old<TAB>new[<TAB>comment]
    tools/rename.py func_80012345 foo_bar

`old` is a splat placeholder (func_XXXXXXXX, D_XXXXXXXX, jtbl_XXXXXXXX) or a name
already in config/symbol_addrs.txt. The script:

- records the name in config/symbol_addrs.txt (functions with `type:func`),
  so `make split` writes the asm (and asm/nonmatchings/.../<name>.s) under it;
- replaces the old name, as a whole word, in src/, include/, wip/, docs, the
  port's game fork (port/game, port/src, port/tests) and the port's docs.

Afterwards: `tools/docker.sh make split && tools/docker.sh make` (must print
OK), then `python3 port/tools/gen_arena.py && python3 port/tools/gen_protos.py`
for the port. Comments in the mapping are not written anywhere; add them by hand.
"""
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SYMS = ROOT / "config/symbol_addrs.txt"
TEXT_GLOBS = [
    "src/**/*.c", "src/**/*.h", "include/**/*.h", "wip/**/*.c",
    "docs/*.md", "README.md",
    "port/PORTING.md", "port/game/**/*.c", "port/game/**/*.h", "port/game/**/*.md",
    "port/src/**/*.c", "port/src/**/*.h", "port/tests/**/*.c", "port/tests/**/*.h",
]
PLACEHOLDER = re.compile(r"^(?:func|D|jtbl)_([0-9A-F]{8})$")
EXE_END = 0x800A6800  # load address + text/data size
IDENT = re.compile(r"^[A-Za-z_]\w*$")
FUNC_HEADER = "// Game functions (src/game), by address."
DATA_HEADER = "// Game globals, by address."


def load_pairs(argv: list[str]) -> list[tuple[str, str]]:
    if len(argv) == 2 and not Path(argv[0]).exists():
        return [(argv[0], argv[1])]
    pairs = []
    for line in Path(argv[0]).read_text().splitlines():
        if not line.strip() or line.startswith("#"):
            continue
        cols = line.split("\t")
        pairs.append((cols[0].strip(), cols[1].strip()))
    return pairs


def symbol_table() -> dict[str, tuple[int, bool]]:
    """name -> (address, is_function) for the names already in symbol_addrs.txt."""
    out = {}
    for m in re.finditer(r"^(\w+)\s*=\s*0x([0-9A-Fa-f]+);(.*)$", SYMS.read_text(), re.M):
        out[m.group(1)] = (int(m.group(2), 16), "type:func" in m.group(3))
    return out


def known_addresses() -> list[int]:
    """Addresses splat currently has a symbol at (from the last `make split`)."""
    addrs = set()
    for s in (ROOT / "asm").rglob("*.s"):
        for m in re.finditer(r"^\s*dlabel (?:D|jtbl)_([0-9A-F]{8})\b", s.read_text(), re.M):
            addrs.add(int(m.group(1), 16))
    undef = ROOT / "config/undefined_syms_auto.txt"
    if undef.exists():
        addrs |= {int(a, 16) for a in re.findall(r"^\w+\s*=\s*0x([0-9A-Fa-f]+);", undef.read_text(), re.M)}
    addrs |= {a for a, _ in symbol_table().values()}
    return sorted(addrs)


def suffix(addr: int, is_func: bool, known: list[int]) -> str:
    if is_func:
        return " // type:func"
    # A named global without a size swallows the placeholders that follow it
    # (splat then writes `name+2` where the C still says D_xxxxxxxx), so give it
    # the size up to the next symbol splat knows about.
    attrs = []
    nxt = next((a for a in known if a > addr), None)
    if nxt is not None:
        attrs.append(f"size:0x{nxt - addr:X}")
    # .bss (past the end of the EXE image) isn't in any splat segment; say which one.
    if addr >= EXE_END:
        attrs.append("segment:main")
    return " // " + " ".join(attrs) if attrs else ""


def add_symbols(new: dict[str, tuple[int, bool]]) -> None:
    """Keep the generated sections sorted by address; other lines stay as they are."""
    known = known_addresses()
    if not known:
        print("warning: no asm/ (run `make split` first); data symbols get no size")
    text = SYMS.read_text()
    for header, want_func in ((FUNC_HEADER, True), (DATA_HEADER, False)):
        entries = {n: (a, suffix(a, f, known)) for n, (a, f) in new.items() if f == want_func}
        if not entries:
            continue
        if header in text:
            head, rest = text.split(header, 1)
            block, _, tail = rest.lstrip("\n").partition("\n\n")
            for m in re.finditer(r"^(\w+)\s*=\s*0x([0-9A-Fa-f]+);(.*)$", block, re.M):
                entries.setdefault(m.group(1), (int(m.group(2), 16), m.group(3)))
        else:
            head, tail = text.rstrip("\n") + "\n\n", ""
        lines = [f"{n} = 0x{a:08X};{s}" for n, (a, s) in sorted(entries.items(), key=lambda e: (e[1][0], e[0]))]
        text = head + header + "\n" + "\n".join(lines) + "\n" + ("\n" + tail if tail else "")
    SYMS.write_text(text)


def main() -> None:
    if not sys.argv[1:]:
        sys.exit(__doc__)
    pairs = load_pairs(sys.argv[1:])
    known = symbol_table()
    olds = {o for o, _ in pairs}
    news = [n for _, n in pairs]
    if len(set(news)) != len(news):
        sys.exit("duplicate new names: " + ", ".join(sorted({n for n in news if news.count(n) > 1})))
    added: dict[str, tuple[int, bool]] = {}
    for old, new in pairs:
        if not IDENT.match(new):
            sys.exit(f"bad name {new!r}")
        if new in known and new not in olds:
            sys.exit(f"{new} is already a symbol")
        if old in known:
            addr, is_func = known[old]
        elif m := PLACEHOLDER.match(old):
            addr, is_func = int(m.group(1), 16), old.startswith("func_")
        else:
            sys.exit(f"don't know the address of {old}")
        added[new] = (addr, is_func)

    # Existing entries under their old name are renamed in place.
    text = SYMS.read_text()
    for old, new in pairs:
        if old in known:
            text = re.sub(rf"^{old}(\s*=)", rf"{new}\1", text, flags=re.M)
            added.pop(new)
    SYMS.write_text(text)
    add_symbols(added)

    pattern = re.compile(r"\b(" + "|".join(re.escape(o) for o in sorted(olds, key=len, reverse=True)) + r")\b")
    mapping = dict(pairs)
    changed = 0
    for glob in TEXT_GLOBS:
        for path in ROOT.glob(glob):
            if "generated" in path.parts:
                continue
            text = path.read_text()
            new_text = pattern.sub(lambda m: mapping[m.group(1)], text)
            if new_text != text:
                path.write_text(new_text)
                changed += 1
    print(f"{len(pairs)} renames, {changed} files changed")


if __name__ == "__main__":
    main()
