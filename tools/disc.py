#!/usr/bin/env python3
"""Disc image intake for the Lain decomp.

Reads a user-supplied BIN/CUE (Redump format preferred) or CHD, records
hashes, and extracts the ISO9660 filesystem of the data track into
extract/discN/. Nothing produced here may be committed to the repository.

Usage:
    tools/disc.py hash    <image.cue|image.chd>
    tools/disc.py extract <image.cue|image.chd> --disc 1
"""
from __future__ import annotations

import argparse
import hashlib
import re
import shutil
import struct
import subprocess
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path

RAW_SECTOR = 2352
USER_SECTOR = 2048
SYNC = bytes([0x00] + [0xFF] * 10 + [0x00])
SUBMODE_FORM2 = 0x20
SUBMODE_REALTIME = 0x40


@dataclass
class Track:
    number: int
    mode: str        # e.g. "MODE2/2352", "AUDIO"
    path: Path


def parse_cue(cue: Path) -> list[Track]:
    tracks: list[Track] = []
    current_file: Path | None = None
    for line in cue.read_text(errors="replace").splitlines():
        line = line.strip()
        if m := re.match(r'FILE\s+"(.+)"\s+\w+', line):
            current_file = cue.parent / m.group(1)
        elif m := re.match(r"TRACK\s+(\d+)\s+(\S+)", line):
            if current_file is None:
                sys.exit(f"{cue}: TRACK before FILE")
            tracks.append(Track(int(m.group(1)), m.group(2), current_file))
    if not tracks:
        sys.exit(f"{cue}: no tracks found")
    return tracks


def resolve_image(image: Path, workdir: Path) -> Path:
    """Return a .cue path, converting a CHD with chdman when needed."""
    if image.suffix.lower() == ".cue":
        return image
    if image.suffix.lower() == ".chd":
        if not shutil.which("chdman"):
            sys.exit("chdman not found: brew install rom-tools (or mame)")
        cue = workdir / (image.stem + ".cue")
        subprocess.run(["chdman", "extractcd", "-i", str(image), "-o", str(cue),
                        "-f"], check=True)
        return cue
    sys.exit(f"unsupported image type: {image.suffix} (use .cue or .chd)")


class SectorReader:
    """Reads 2048-byte user data from a raw (2352) or cooked (2048) track."""

    def __init__(self, path: Path):
        self.f = path.open("rb")
        size = path.stat().st_size
        head = self.f.read(12)
        self.raw = head == SYNC and size % RAW_SECTOR == 0
        self.sector_size = RAW_SECTOR if self.raw else USER_SECTOR

    def read(self, lba: int, count: int = 1) -> bytes:
        out = bytearray()
        for i in range(count):
            self.f.seek((lba + i) * self.sector_size)
            sector = self.f.read(self.sector_size)
            if not self.raw:
                out += sector
                continue
            mode = sector[15]
            # Mode 1: data at 16. Mode 2 Form 1: 8-byte subheader, data at 24.
            out += sector[16:16 + USER_SECTOR] if mode == 1 else sector[24:24 + USER_SECTOR]
        return bytes(out)

    def read_raw(self, lba: int) -> bytes:
        self.f.seek(lba * self.sector_size)
        return self.f.read(self.sector_size)

    def is_stream(self, lba: int, count: int) -> bool:
        """True if any sector is Form 2 or real-time (XA audio / STR video).

        Lain's ISO directory doesn't flag its streams with XA attributes, so
        the sector subheaders are the only reliable source.
        """
        if not self.raw:
            return False
        for i in range(count):
            self.f.seek((lba + i) * RAW_SECTOR + 18)
            if self.f.read(1)[0] & (SUBMODE_FORM2 | SUBMODE_REALTIME):
                return True
        return False


@dataclass
class Entry:
    name: str
    lba: int
    size: int
    is_dir: bool
    # XA attributes: form 2 (STR/XA streams) must be copied as raw sectors.
    form2: bool


def iter_dir(reader: SectorReader, lba: int, size: int):
    data = reader.read(lba, (size + USER_SECTOR - 1) // USER_SECTOR)
    pos = 0
    while pos < size:
        length = data[pos]
        if length == 0:  # records never span sectors; skip to next one
            pos = (pos // USER_SECTOR + 1) * USER_SECTOR
            continue
        rec = data[pos:pos + length]
        ext_lba = struct.unpack_from("<I", rec, 2)[0]
        ext_size = struct.unpack_from("<I", rec, 10)[0]
        flags = rec[25]
        name_len = rec[32]
        name = rec[33:33 + name_len]
        pos += length
        if name in (b"\x00", b"\x01"):
            continue
        # System Use area begins after the (padded) name; CD-XA signature "XA".
        su = 33 + name_len + (1 - name_len % 2)
        form2 = False
        if rec[su + 6:su + 8] == b"XA":
            attr = struct.unpack_from(">H", rec, su + 4)[0]
            form2 = bool(attr & 0x1000) or bool(attr & 0x4000)
        yield Entry(name.decode("ascii", "replace").split(";")[0], ext_lba,
                    ext_size, bool(flags & 2), form2)


def walk(reader: SectorReader, lba: int, size: int, prefix: str = ""):
    for e in iter_dir(reader, lba, size):
        path = f"{prefix}{e.name}"
        if e.is_dir:
            yield from walk(reader, e.lba, e.size, path + "/")
        else:
            yield path, e


def root_record(reader: SectorReader) -> tuple[int, int]:
    pvd = reader.read(16)
    if pvd[1:6] != b"CD001":
        sys.exit("no ISO9660 primary volume descriptor at sector 16")
    root = pvd[156:156 + 34]
    return struct.unpack_from("<I", root, 2)[0], struct.unpack_from("<I", root, 10)[0]


def sha1_file(path: Path) -> str:
    h = hashlib.sha1()
    with path.open("rb") as f:
        while chunk := f.read(1 << 20):
            h.update(chunk)
    return h.hexdigest()


def cmd_hash(cue: Path) -> None:
    for t in parse_cue(cue):
        print(f"track {t.number:02d} {t.mode:<11} sha1 {sha1_file(t.path)}  {t.path.name}")
    print("\nCompare these against the Redump entry for your disc.")


def cmd_extract(cue: Path, out: Path) -> None:
    data_track = next((t for t in parse_cue(cue) if t.mode != "AUDIO"), None)
    if data_track is None:
        sys.exit("no data track")
    reader = SectorReader(data_track.path)
    out.mkdir(parents=True, exist_ok=True)
    manifest = []
    for path, e in walk(reader, *root_record(reader)):
        dest = out / path
        dest.parent.mkdir(parents=True, exist_ok=True)
        n = (e.size + USER_SECTOR - 1) // USER_SECTOR
        stream = reader.raw and (e.form2 or reader.is_stream(e.lba, n))
        if stream:
            # Keep full 2352-byte sectors so XA/MDEC decoders see subheaders.
            dest.write_bytes(b"".join(reader.read_raw(e.lba + i) for i in range(n)))
        else:
            dest.write_bytes(reader.read(e.lba, n)[:e.size])
        manifest.append(f"{e.lba:8d} {e.size:10d} {'RAW' if stream else 'F1 '} {path}")
    (out / "MANIFEST.txt").write_text("\n".join(manifest) + "\n")

    cnf = out / "SYSTEM.CNF"
    if cnf.exists():
        m = re.search(r"BOOT\s*=\s*cdrom:\\?([^;\s]+)", cnf.read_text(errors="replace"))
        if m:
            print(f"boot executable: {m.group(1)}")
    print(f"extracted {len(manifest)} files to {out}")


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("command", choices=["hash", "extract"])
    ap.add_argument("image", type=Path)
    ap.add_argument("--disc", type=int, default=1)
    ap.add_argument("--out", type=Path, default=Path("extract"))
    args = ap.parse_args()

    with tempfile.TemporaryDirectory() as tmp:
        cue = resolve_image(args.image, Path(tmp))
        if args.command == "hash":
            cmd_hash(cue)
        else:
            cmd_extract(cue, args.out / f"disc{args.disc}")


if __name__ == "__main__":
    main()
