#!/usr/bin/env python3
"""Fetch upstream stb_image.h into the build tree (SHA-256 pinned by CMake)."""
from __future__ import annotations

import hashlib
import pathlib
import sys
import urllib.request

STB_IMAGE_URL = "https://raw.githubusercontent.com/nothings/stb/master/stb_image.h"


def main() -> int:
    if len(sys.argv) != 3:
        print("usage: amalgamate.py <out.h> <sha256>", file=sys.stderr)
        return 2
    out = pathlib.Path(sys.argv[1])
    expected = sys.argv[2].lower()
    out.parent.mkdir(parents=True, exist_ok=True)
    with urllib.request.urlopen(STB_IMAGE_URL, timeout=60) as resp:
        data = resp.read()
    digest = hashlib.sha256(data).hexdigest()
    if digest != expected:
        print(
            f"stb_image.h sha256 mismatch: got {digest}, expected {expected}",
            file=sys.stderr,
        )
        return 1
    out.write_bytes(data)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
