#!/usr/bin/env python3
"""Writes PNG test images of every color type, bit depth and interlacing, with the
expected 8-bit RGBA next to each (name.rgba: width, height as 32-bit LE, then pixels).
The expected pixels come from Pillow's decoder.

    python3 gen_pngs.py <out dir>
"""
import os
import random
import struct
import sys
import zlib

from PIL import Image


def chunk(kind, body):
    return struct.pack(">I", len(body)) + kind + body + struct.pack(">I", zlib.crc32(kind + body) & 0xFFFFFFFF)


def pack_row(samples, depth):
    """Packs sample values (already per channel) into bytes."""
    if depth == 16:
        return b"".join(struct.pack(">H", v) for v in samples)
    if depth == 8:
        return bytes(samples)
    out, acc, bits = bytearray(), 0, 0
    for v in samples:
        acc = (acc << depth) | v
        bits += depth
        if bits == 8:
            out.append(acc)
            acc, bits = 0, 0
    if bits:
        out.append(acc << (8 - bits))
    return bytes(out)


def write_png(path, w, h, ctype, depth, pixels, interlace=False, plte=None, trns=None, filt=0):
    chans = {0: 1, 2: 3, 3: 1, 4: 2, 6: 4}[ctype]
    passes = [(0, 0, 8, 8), (4, 0, 8, 8), (0, 4, 4, 8), (2, 0, 4, 4), (0, 2, 2, 4), (1, 0, 2, 2), (0, 1, 1, 2)]
    raw = bytearray()
    for (x0, y0, dx, dy) in (passes if interlace else [(0, 0, 1, 1)]):
        xs = list(range(x0, w, dx))
        ys = list(range(y0, h, dy))
        if not xs or not ys:
            continue
        for y in ys:
            row = []
            for x in xs:
                row.extend(pixels[y * w + x][:chans])
            raw.append(filt)
            data = pack_row(row, depth)
            if filt == 1:  # Sub
                bpp = max(1, chans * depth // 8)
                data = bytes((data[i] - (data[i - bpp] if i >= bpp else 0)) & 0xFF for i in range(len(data)))
            raw += data
    ihdr = struct.pack(">IIBBBBB", w, h, depth, ctype, 0, 0, 1 if interlace else 0)
    body = chunk(b"IHDR", ihdr)
    if plte:
        body += chunk(b"PLTE", bytes(c for rgb in plte for c in rgb))
    if trns is not None:
        body += chunk(b"tRNS", trns)
    z = zlib.compress(bytes(raw), 9)
    # split IDAT to test concatenation
    body += chunk(b"IDAT", z[: len(z) // 2]) + chunk(b"IDAT", z[len(z) // 2:])
    with open(path, "wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n" + body + chunk(b"IEND", b""))


def expected(path):
    im = Image.open(path)
    if im.mode.startswith("I;16"):
        # Pillow clamps 16-bit gray to 255 when converting; the 8-bit value is the high byte
        px = bytes(c for v in im.getdata() for c in (v >> 8, v >> 8, v >> 8, 255))
        data = struct.pack("<II", im.width, im.height) + px
    else:
        im = im.convert("RGBA")
        data = struct.pack("<II", im.width, im.height) + im.tobytes()
    with open(path[:-4] + ".rgba", "wb") as f:
        f.write(data)


def main():
    out = sys.argv[1]
    os.makedirs(out, exist_ok=True)
    rnd = random.Random(1)
    w, h = 13, 9  # odd sizes exercise the interlace passes and sub-byte rows
    cases = []
    for depth in (1, 2, 4, 8, 16):
        m = (1 << depth) - 1
        cases.append((f"gray{depth}", 0, depth, [(rnd.randint(0, m),) for _ in range(w * h)], None, None))
    cases.append(("gray8_trns", 0, 8, [(rnd.choice((0, 7, 200)),) for _ in range(w * h)], None, struct.pack(">H", 7)))
    for depth in (8, 16):
        m = (1 << depth) - 1
        cases.append((f"rgb{depth}", 2, depth, [tuple(rnd.randint(0, m) for _ in range(3)) for _ in range(w * h)], None, None))
        cases.append((f"graya{depth}", 4, depth, [tuple(rnd.randint(0, m) for _ in range(2)) for _ in range(w * h)], None, None))
        cases.append((f"rgba{depth}", 6, depth, [tuple(rnd.randint(0, m) for _ in range(4)) for _ in range(w * h)], None, None))
    cases.append(("rgb8_trns", 2, 8, [rnd.choice(((1, 2, 3), (9, 9, 9))) for _ in range(w * h)], None, bytes([0, 1, 0, 2, 0, 3])))
    for depth in (1, 2, 4, 8):
        n = 1 << depth
        plte = [tuple(rnd.randint(0, 255) for _ in range(3)) for _ in range(n)]
        trns = bytes(rnd.randint(0, 255) for _ in range(min(n, 5)))
        cases.append((f"pal{depth}", 3, depth, [(rnd.randint(0, n - 1),) for _ in range(w * h)], plte, trns))
    for name, ctype, depth, px, plte, trns in cases:
        for interlace in (False, True):
            p = os.path.join(out, f"{name}{'_i' if interlace else ''}.png")
            write_png(p, w, h, ctype, depth, px, interlace, plte, trns, filt=1 if depth >= 8 else 0)
            expected(p)
    print(f"{len(cases) * 2} PNGs in {out}")


if __name__ == "__main__":
    main()
