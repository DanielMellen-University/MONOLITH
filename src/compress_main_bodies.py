#!/usr/bin/env python3
"""Compress main.cpp body includes into MCP-friendly .inc.z64 files."""
from __future__ import annotations

import base64
import pathlib
import sys
import textwrap
import zlib


def main() -> int:
    if len(sys.argv) != 3:
        print("usage: compress_main_bodies.py <input_dir> <output_dir>", file=sys.stderr)
        return 2

    source_dir = pathlib.Path(sys.argv[1])
    output_dir = pathlib.Path(sys.argv[2])
    parts = sorted(source_dir.glob("main_body_*.inc"))
    if not parts:
        print("no main_body_*.inc found", file=sys.stderr)
        return 1

    output_dir.mkdir(parents=True, exist_ok=True)
    for part in parts:
        encoded = base64.b64encode(zlib.compress(part.read_bytes(), 9)).decode("ascii")
        target = output_dir / (part.name + ".z64")
        target.write_text("\n".join(textwrap.wrap(encoded, 80)) + "\n", encoding="ascii")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
