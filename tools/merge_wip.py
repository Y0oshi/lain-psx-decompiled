#!/usr/bin/env python3
"""Merge a finished wip/<function>.c draft into its src/game file.

    tools/merge_wip.py wip/menu_run.c src/game/80023468.c [--guard NEEDS_SDATA]

Drops the draft's #include and any typedef the target file already defines,
then replaces the function's INCLUDE_ASM line with the rest of the draft
(declarations, helpers, function). With --guard, the draft body is wrapped in
#ifdef GUARD ... #else INCLUDE_ASM ... #endif. Extern/prototype duplicates are
left in place (identical redeclarations are legal); conflicts show at compile.
"""
import argparse
import re
from pathlib import Path


def typedef_names(text: str) -> set[str]:
    return set(re.findall(r"^\}\s*(\w+)\s*;", text, re.M)) | \
        set(re.findall(r"^typedef\s+[^{;]*?\b(\w+)\s*;", text, re.M))


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("draft", type=Path)
    ap.add_argument("target", type=Path)
    ap.add_argument("--guard")
    args = ap.parse_args()
    func = args.draft.stem
    target = args.target.read_text()
    have = typedef_names(target)
    lines = args.draft.read_text().split("\n")
    out, i = [], 0
    while i < len(lines):
        l = lines[i]
        if l.startswith("#include"):
            i += 1
            continue
        if l.startswith("typedef"):
            j, depth = i, 0
            while True:
                depth += lines[j].count("{") - lines[j].count("}")
                if depth == 0 and lines[j].rstrip().endswith(";") or (depth == 0 and re.search(r"\}\s*\w+\s*;", lines[j])):
                    break
                j += 1
            m = re.search(r"(\w+)\s*;[^;]*$", lines[j])
            if m and m.group(1) in have:
                i = j + 1
                continue
            out += lines[i:j + 1]
            i = j + 1
            continue
        out.append(l)
        i += 1
    body = "\n".join(out).strip("\n")
    asm_re = re.compile(rf'^INCLUDE_ASM\("([^"]+)", {func}\);$', re.M)
    m = asm_re.search(target)
    if not m:
        raise SystemExit(f"no INCLUDE_ASM for {func} in {args.target}")
    if args.guard:
        body = f"#ifdef {args.guard}\n{body}\n#else\n{m.group(0)}\n#endif"
    args.target.write_text(target[:m.start()] + body + target[m.end():])
    print(f"merged {args.draft} into {args.target}" + (f" under {args.guard}" if args.guard else ""))


if __name__ == "__main__":
    main()
