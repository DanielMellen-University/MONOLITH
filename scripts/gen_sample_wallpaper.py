#!/usr/bin/env python3
"""Generate assets/wallpapers/sample.bmp and sample.png (no external deps)."""

from __future__ import annotations

import pathlib
import struct
import zlib

ROOT = pathlib.Path(__file__).resolve().parents[1]
OUT_DIR = ROOT / "assets" / "wallpapers"


def gradient_rgb(w: int, h: int) -> list[tuple[int, int, int]]:
    pixels: list[tuple[int, int, int]] = []
    for y in range(h):
        t = y / (h - 1) if h > 1 else 0.0
        r0 = int(18 + t * (16 - 18))
        g0 = int(24 + t * (80 - 24))
        b0 = int(42 + t * (90 - 42))
        for x in range(w):
            u = x / (w - 1) if w > 1 else 0.0
            rr = min(255, int(r0 + u * 20))
            gg = min(255, int(g0 + u * 10))
            bb = min(255, int(b0 + (1 - u) * 15))
            pixels.append((rr, gg, bb))
    return pixels


def write_bmp(path: pathlib.Path, w: int, h: int, pixels: list[tuple[int, int, int]]) -> None:
    row_pad = (4 - (w * 3) % 4) % 4
    raw = bytearray()
    for y in range(h):
        src_y = h - 1 - y  # BMP is bottom-up
        for x in range(w):
            r, g, b = pixels[src_y * w + x]
            raw += bytes([b, g, r])
        raw += b"\x00" * row_pad
    dib = struct.pack("<IIIHHIIIIII", 40, w, h, 1, 24, 0, len(raw), 2835, 2835, 0, 0)
    file_size = 14 + len(dib) + len(raw)
    header = struct.pack("<2sIHHI", b"BM", file_size, 0, 0, 14 + len(dib))
    path.write_bytes(header + dib + raw)


def write_png(path: pathlib.Path, w: int, h: int, pixels: list[tuple[int, int, int]]) -> None:
    def chunk(tag: bytes, data: bytes) -> bytes:
        return struct.pack(">I", len(data)) + tag + data + struct.pack(
            ">I", zlib.crc32(tag + data) & 0xFFFFFFFF
        )

    raw = bytearray()
    for y in range(h):
        raw.append(0)  # filter None
        for x in range(w):
            r, g, b = pixels[y * w + x]
            raw += bytes([r, g, b])
    ihdr = struct.pack(">IIBBBBB", w, h, 8, 2, 0, 0, 0)
    data = b"\x89PNG\r\n\x1a\n"
    data += chunk(b"IHDR", ihdr)
    data += chunk(b"IDAT", zlib.compress(bytes(raw), 9))
    data += chunk(b"IEND", b"")
    path.write_bytes(data)


def main() -> None:
    w, h = 64, 48
    pixels = gradient_rgb(w, h)
    OUT_DIR.mkdir(parents=True, exist_ok=True)
    bmp = OUT_DIR / "sample.bmp"
    png = OUT_DIR / "sample.png"
    write_bmp(bmp, w, h, pixels)
    write_png(png, w, h, pixels)
    print(f"wrote {bmp} ({bmp.stat().st_size} bytes)")
    print(f"wrote {png} ({png.stat().st_size} bytes)")


if __name__ == "__main__":
    main()
