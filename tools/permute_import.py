#!/usr/bin/env python3
"""Set up a decomp-permuter work dir for one function. Runs in the container;
normally called by tools/permute.sh.

    tools/permute_import.py func_80012345 src/game/80012300.c build/permuter/func_80012345
    tools/permute_import.py --rank func_80012345 build/permuter/func_80012345

--rank rescores the permuter's output-*/source.c with check.sh's metric
(the permuter's own score weights regalloc/reordering/insertions differently,
so a lower permuter score can be more check.sh diffs).

Writes base.c (the file preprocessed with -DPERMUTER -DNON_MATCHING ..., pruned
to the function and what it uses), target.o (the function's asm assembled with
include/macro.inc), compile.sh (tools/cc.sh) and settings.toml.

pycparser can't parse __attribute__, but NO_GP-style section attributes change
the addressing mode, so every line containing one (all are extern declarations)
is kept twice: a stripped copy for the permuter to parse and the original as a
`#pragma _permuter b64literal`, emitted verbatim when compiling.
"""
import base64
import os
import re
import subprocess
import sys
from pathlib import Path

PERMUTER = os.environ.get("PERMUTER_DIR", "/opt/permuter")
sys.path.insert(0, PERMUTER)

from src import ast_util  # noqa: E402

CPP = ["/opt/gcc/2.8.1-psx/cpp", "-undef", "-P", "-lang-c", "-Dmips", "-D__mips__",
       "-D__GNUC__=2", "-D_LANGUAGE_C", "-DPSX", "-DPERMUTER", "-DNEEDS_RODATA",
       "-DNEEDS_SDATA", "-DNON_MATCHING", "-Iinclude", "-Isrc"]
RE_ATTR = re.compile(r"__attribute__\s*\(\((?:[^()]|\([^()]*\))*\)\)")

COMPILE_SH = """#!/bin/bash
# Called by the permuter as: compile.sh in.c -o out.o
set -eo pipefail
IN="$(realpath "$1")"; OUT="$(realpath -m "$3")"
cd /lain
CPPFLAGS_EXTRA="-DNEEDS_RODATA -DNEEDS_SDATA -DNON_MATCHING" exec tools/cc.sh "$IN" "$OUT"
"""


def find_asm(func: str) -> Path:
    for p in Path("asm/nonmatchings").rglob(f"{func}.s"):
        return p
    sys.exit(f"asm for {func} not found")


def split_attrs(source: str) -> str:
    out = []
    for line in source.splitlines():
        if "__attribute__" in line:
            plain = RE_ATTR.sub("", line)
            out.append(plain)
            enc = base64.b64encode(line.encode()).decode()
            out.append(f"#pragma _permuter b64literal {enc}")
        else:
            out.append(line)
    return "\n".join(out) + "\n"


def prune(ast, fn) -> None:
    """prune_ast keeps every pragma (and whatever it mentions), so take the
    attribute copies out first and put each back after its stripped
    declaration if that survived."""
    from perm_pycparser import c_ast as ca
    attached = {}
    rest = []
    for n in ast.ext:
        if isinstance(n, ca.Pragma) and "b64literal" in n.string and rest:
            attached.setdefault(id(rest[-1]), []).append(n)
        else:
            rest.append(n)
    ast.ext[:] = rest
    ast_util.prune_ast(fn, ast)
    out = []
    for n in ast.ext:
        out.append(n)
        out.extend(attached.get(id(n), []))
    ast.ext[:] = out


def rank(func: str, out_dir: str) -> None:
    import tempfile
    sys.path.insert(0, "tools")
    from probe import target_words, compiled_words, score
    target = target_words(func)
    rows = []
    with tempfile.TemporaryDirectory() as tmp:
        obj = os.path.join(tmp, "c.o")
        for src in sorted(Path(out_dir).glob("output-*/source.c")):
            r = subprocess.run([str(Path(out_dir) / "compile.sh"), str(src), "-o", obj],
                               capture_output=True)
            if r.returncode:
                continue
            got, relocs = compiled_words(obj, func)
            rows.append((score(target, got, relocs), str(src.parent)))
    for n, d in sorted(rows):
        print(f"{n:4d} diffs  {d}")


def main() -> None:
    if sys.argv[1] == "--rank":
        return rank(sys.argv[2], sys.argv[3])
    func, c_file, out_dir = sys.argv[1:4]
    do_prune = "--no-prune" not in sys.argv
    out = Path(out_dir)
    out.mkdir(parents=True, exist_ok=True)

    source = subprocess.check_output(CPP + [c_file], encoding="utf-8")
    source = split_attrs(source)
    if f" {func}(" not in source:
        sys.exit(f"{func} has no C body in {c_file} (is it guarded by something other than NON_MATCHING?)")
    ast = ast_util.parse_c(source, from_import=True)
    orig_fn, _ = ast_util.extract_fn(ast, func)
    if do_prune:
        prune(ast, orig_fn)
    (out / "base.c").write_text(ast_util.to_c_raw(ast))

    asm = find_asm(func)
    (out / "target.s").write_text('.include "macro.inc"\n.set noat\n.set noreorder\n'
                                  + asm.read_text())
    subprocess.check_call(["mips-linux-gnu-as", "-EL", "-march=r3000", "-mtune=r3000",
                           "-mabi=32", "-no-pad-sections", "-G0", "-Iinclude",
                           str(out / "target.s"), "-o", str(out / "target.o")])

    (out / "compile.sh").write_text(COMPILE_SH)
    os.chmod(out / "compile.sh", 0o755)
    (out / "settings.toml").write_text(f'func_name = "{func}"\ncompiler_type = "gcc"\n')

    # Sanity check: the imported base must compile.
    base_c = out / "base_check.c"
    base_c.write_text(ast_util.process_pragmas(ast_util.to_c_raw(ast)))
    subprocess.check_call([str(out / "compile.sh"), str(base_c), "-o", str(out / "base.o")])
    base_c.unlink()
    print(f"Imported {func} into {out}")


if __name__ == "__main__":
    main()
