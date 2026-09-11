#!/usr/bin/env python3
"""Rebuild stb_image.h from zlib-compressed base64 fragments."""
from __future__ import annotations

import base64
import pathlib
import sys
import zlib


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: amalgamate.py <out.h>", file=sys.stderr)
        return 2
    out = pathlib.Path(sys.argv[1])
    root = pathlib.Path(__file__).resolve().parent
    parts = sorted(root.glob("stb_image_b64_*.txt"))
    if not parts:
        print("no stb_image_b64_*.txt fragments found", file=sys.stderr)
        return 1
    b64 = "".join(part.read_text(encoding="ascii") for part in parts)
    b64 = "".join(b64.split())
    data = zlib.decompress(base64.b64decode(b64))
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_bytes(data)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
