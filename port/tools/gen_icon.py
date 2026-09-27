#!/usr/bin/env python3
"""Writes port/src/lain.png (1024x1024) and port/src/lain.ico from
port/client/lain_icon.h. Needs Pillow."""
import re
from pathlib import Path

from PIL import Image

PORT = Path(__file__).resolve().parent.parent
src = (PORT / "client/lain_icon.h").read_text()
pal = [int(v, 16) for v in re.findall(r"0x([0-9A-F]{8})", src.split("lain_icon_palette")[1].split("};")[0])]
rows = re.findall(r"\{([\d ,]+)\}", src.split("lain_icon_pixels")[1])
px = [[int(v) for v in r.split(",") if v.strip()] for r in rows]

icon = Image.new("RGBA", (16, 16))
for y, row in enumerate(px):
    for x, c in enumerate(row):
        p = pal[c]
        icon.putpixel((x, y), (p & 255, p >> 8 & 255, p >> 16 & 255, p >> 24 & 255))

big = icon.resize((1024, 1024), Image.NEAREST)
big.save(PORT / "src/lain.png")
sizes = [16, 32, 48, 64, 128]
icon.resize((256, 256), Image.NEAREST).save(
    PORT / "src/lain.ico",
    sizes=[(n, n) for n in sizes + [256]],
    append_images=[icon.resize((n, n), Image.NEAREST) for n in sizes],
)
