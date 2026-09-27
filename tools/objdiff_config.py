#!/usr/bin/env python3
"""Generate objdiff inputs for the progress report (decomp.dev).

Writes:
  objdiff.json                          one unit per linked code segment
  build/objdiff/include/macro.inc       macro.inc without the .NON_MATCHING labels
  build/objdiff/target/src/game/<f>.s   the original code of each C file: its
                                        functions (asm/{non,}matchings) plus its
                                        .rodata/.sdata slices, in address order

Target objects are assembled from these;
base objects are the C files compiled without INCLUDE_ASM. See `make report`.

    tools/objdiff_config.py
"""
import json
import re
from pathlib import Path

import yaml

ROOT = Path(__file__).resolve().parent.parent
CONFIG = ROOT / "config/slps_016.03.yaml"
OUT = ROOT / "build/objdiff"
ASM_DIRS = [ROOT / "asm/nonmatchings", ROOT / "asm/matchings"]
ADDR = re.compile(r"^\s*/\* [0-9A-F]+ ([0-9A-F]{8}) ", re.M)
SECTION = re.compile(r"^\s*\.section\s+([.\w]+)")


def write_if_changed(path: Path, text: str) -> None:
    if path.exists() and path.read_text() == text:
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text)


def code_segments() -> list[tuple[str, str]]:
    """(type, name) for every c/asm subsegment, in link order."""
    cfg = yaml.safe_load(CONFIG.read_text())
    out = []
    for seg in cfg["segments"]:
        if not isinstance(seg, dict):
            continue
        for sub in seg.get("subsegments", []):
            if isinstance(sub, list) and len(sub) >= 3 and sub[1] in ("c", "asm"):
                out.append((sub[1], sub[2]))
    return out


def text_only(src: str) -> str:
    """Drop migrated .rodata blocks; the unit's .rodata slice already has them."""
    keep, section = [], ".text"
    for line in src.splitlines():
        if m := SECTION.match(line):
            section = m.group(1)
            continue
        if section == ".text" or line.strip().startswith(".set "):
            keep.append(line)
    return "\n".join(keep) + "\n"


def target_asm(name: str) -> str:
    """Concatenated original asm for C unit `name` (e.g. game/80013138)."""
    funcs = {}
    for d in ASM_DIRS:
        for s in (d / name).glob("*.s"):
            src = s.read_text()
            if "glabel" not in src:
                continue  # INCLUDE_RODATA piece, covered by the .rodata slice
            text = text_only(src)
            funcs[int(ADDR.search(text).group(1), 16)] = text
    parts = ['.include "macro.inc"\n']
    for kind in ("rodata", "sdata"):
        data = ROOT / f"asm/data/{name}.{kind}.s"
        if data.exists():
            parts.append(data.read_text())
    parts.append(".section .text\n")
    parts.extend(funcs[a] for a in sorted(funcs))
    return "\n".join(parts)


def main() -> None:
    macro = (ROOT / "include/macro.inc").read_text()
    macro, n = re.subn(r"(\.macro nonmatching[^\n]*\n).*?(\.endm)", r"\1\2", macro, flags=re.S)
    assert n == 1, "nonmatching macro not found in include/macro.inc"
    write_if_changed(OUT / "include/macro.inc", macro)

    units = []
    for kind, name in code_segments():
        if kind == "c":
            write_if_changed(OUT / f"target/src/{name}.s", target_asm(name))
            units.append({
                "name": name,
                "target_path": f"build/objdiff/target/src/{name}.o",
                "base_path": f"build/objdiff/base/src/{name}.o",
                "metadata": {
                    "progress_categories": ["game"],
                    "source_path": f"src/{name}.c",
                    "complete": "#ifdef NON_MATCHING" not in (ROOT / f"src/{name}.c").read_text(),
                },
            })
        elif not name.startswith("psyq/"):
            # PsyQ SDK objects are Sony's library, linked as is: not part of the
            # decompilation, so they stay out of the progress figures.
            units.append({
                "name": name,
                "target_path": f"build/objdiff/target/asm/{name}.o",
                "base_path": None,
                "metadata": {"progress_categories": ["game"]},
            })

    config = {
        "$schema": "https://raw.githubusercontent.com/encounter/objdiff/main/config.schema.json",
        "build_target": False,
        "build_base": False,
        "units": units,
        "progress_categories": [
            {"id": "game", "name": "Game"},
        ],
    }
    write_if_changed(ROOT / "objdiff.json", json.dumps(config, indent=2) + "\n")


if __name__ == "__main__":
    main()
