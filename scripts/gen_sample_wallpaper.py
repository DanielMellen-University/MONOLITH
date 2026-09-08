#!/usr/bin/env python3
"""Generate assets/wallpapers/sample.bmp (24-bit BMP, no external deps)."""

import struct
import pathlib

ROOT = pathlib.Path(__file__).resolve().parents[1]
OUT = ROOT / "assets" / "wallpapers" / "sample.bmp"


def main() -> None:
    w, h = 64, 48
    pixels = bytearray()
    row_pad = (4 - (w * 3) % 4) % 4
    for y in range(h):
        src_y = h - 1 - y  # BMP is bottom-up
        t = src_y / (h - 1)
        r = int(18 + t * (16 - 18))
        g = int(24 + t * (80 - 24))
        b = int(42 + t * (90 - 42))
        for x in range(w):
            u = x / (w - 1)
            rr = min(255, int(r + u * 20))
            gg = min(255, int(g + u * 10))
            bb = min(255, int(b + (1 - u) * 15))
            pixels += bytes([bb, gg, rr])
        pixels += b"\x00" * row_pad

    dib = struct.pack("<IIIHHIIIIII", 40, w, h, 1, 24, 0, len(pixels), 2835, 2835, 0, 0)
    file_size = 14 + len(dib) + len(pixels)
    header = struct.pack("<2sIHHI", b"BM", file_size, 0, 0, 14 + len(dib))
    OUT.parent.mkdir(parents=True, exist_ok=True)
    OUT.write_bytes(header + dib + pixels)
    print(f"wrote {OUT} ({OUT.stat().st_size} bytes)")


if __name__ == "__main__":
    main()
