#!/usr/bin/env python3
"""Decompress SettingsApp_body_XX.inc.z64 into an output directory."""
from __future__ import annotations

import base64
import pathlib
import sys
import zlib


def main() -> int:
    if len(sys.argv) != 2:
        print("usage: decompress_settings_bodies.py <out_dir>", file=sys.stderr)
        return 2
    out = pathlib.Path(sys.argv[1])
    out.mkdir(parents=True, exist_ok=True)
    root = pathlib.Path(__file__).resolve().parent
    parts = sorted(root.glob("SettingsApp_body_*.inc.z64"))
    if not parts:
        print("no SettingsApp_body_*.inc.z64 found", file=sys.stderr)
        return 1
    for part in parts:
        name = part.name[:-4]  # strip .z64 -> *.inc
        b64 = "".join(part.read_text(encoding="ascii").split())
        data = zlib.decompress(base64.b64decode(b64))
        (out / name).write_bytes(data)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
