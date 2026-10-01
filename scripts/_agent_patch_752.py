#!/usr/bin/env python3
import base64, zlib
from pathlib import Path

payload = Path("scripts/_agent_patch_752_payload.b64").read_text().strip()
Path("scripts/test_drawing_state.cpp").write_bytes(zlib.decompress(base64.b64decode(payload)))

def must_replace(path, old, new, optional=False):
    p = Path(path)
    t = p.read_text()
    if old not in t:
        if optional:
            print(f"skip missing optional needle in {path}")
            return False
        raise SystemExit(f"missing needle in {path}: {old[:80]!r}")
    p.write_text(t.replace(old, new, 1))
    print(f"patched {path}")
    return True

agents = Path(".agents/AGENTS.md").read_text()
if "| 7.52 |" not in agents:
    must_replace(
        ".agents/AGENTS.md",
        "| 2026-09-29 | 7.51 | Text Editor streams file opens and rejects documents above 16 MiB or 65,536 lines; hosted headless workflow #71 passed |\n",
        "| 2026-10-01 | 7.52 | Drawing streams `.modr` opens, caps live canvases at 4096×4096, and rejects oversized open/save; local headless drawing tests passed |\n| 2026-09-29 | 7.51 | Text Editor streams file opens and rejects documents above 16 MiB or 65,536 lines; hosted headless workflow #71 passed |\n",
    )
    must_replace(
        ".agents/AGENTS.md",
        "| 7.51 | Bound Text Editor file I/O | done | Stream file opens in 16 KiB chunks, normalize line endings across chunk boundaries, and enforce 16 MiB / 65,536-line limits on open and save without replacing the current document on rejected opens |\n",
        "| 7.51 | Bound Text Editor file I/O | done | Stream file opens in 16 KiB chunks, normalize line endings across chunk boundaries, and enforce 16 MiB / 65,536-line limits on open and save without replacing the current document on rejected opens |\n| 7.52 | Bound Drawing file I/O | done | Stream `.modr` opens in 16 KiB chunks, cap live canvases and files at 4096×4096, and leave the current canvas unchanged when an open is rejected |\n",
    )
elif "| 7.52 | Bound Drawing file I/O | in progress |" in agents:
    must_replace(
        ".agents/AGENTS.md",
        "| 7.52 | Bound Drawing file I/O | in progress |",
        "| 7.52 | Bound Drawing file I/O | done |",
    )

if must_replace(
    ".agents/SESSION_LOG.md",
    "# Session log\n\n",
    "# Session log\n\n| 2026-10-01 | fix | Streamed Drawing `.modr` opens through bounded chunks, capped live canvases at 4096×4096 to match the format limit, rejected oversized open/save while preserving the active canvas, and covered clamp plus oversized rejection. |\n\n",
    optional=True,
) is False and "| 2026-10-01 | fix | Streamed Drawing" not in Path(".agents/SESSION_LOG.md").read_text():
    raise SystemExit("SESSION_LOG missing 7.52 row")

if must_replace(
    "CHANGELOG.md",
    "# Changelog\n\n",
    "# Changelog\n\n## 2026-10: Bound Drawing file loading\n\n- Cap live Drawing canvases at 4096×4096 so window growth cannot exceed the `.modr` format limit.\n- Stream `.modr` opens in 16 KiB chunks, reject files above the maximum payload size, and leave the current canvas unchanged on failure.\n- Refuse saves whose dimensions exceed the shared canvas limit.\n\n",
    optional=True,
) is False and "## 2026-10: Bound Drawing file loading" not in Path("CHANGELOG.md").read_text():
    raise SystemExit("CHANGELOG missing 7.52 section")

must_replace(
    "docs/filesystem.md",
    "the Text Editor uses it to stream line parsing and rejects documents above 16 MiB or 65,536 lines. `readFileTailChunks()`",
    "the Text Editor uses it to stream line parsing and rejects documents above 16 MiB or 65,536 lines, and Drawing uses it to stream `.modr` payloads while rejecting files above the 4096×4096 size cap. `readFileTailChunks()`",
    optional=True,
)
must_replace(
    "docs/development/scripts.md",
    "Headless Drawing state test for clean loads, resize and tile-dirty tracking,",
    "Headless Drawing state test for bounded streamed `.modr` loading, oversized open/save rejection, canvas dimension clamping, clean loads, resize and tile-dirty tracking,",
    optional=True,
)
must_replace(
    "docs/apps/drawing.md",
    "and its pixel dimensions follow the Drawing window's client area. Resizing the window",
    "and its pixel dimensions follow the Drawing window's client area up to 4096×4096. Resizing the window",
    optional=True,
)
must_replace(
    "docs/apps/drawing.md",
    "Opening a missing file, a non-`.modr` path, or corrupt data leaves the current sketch open and reports the failure in the status bar.\n\n",
    "Opening a missing file, a non-`.modr` path, or corrupt data leaves the current sketch open and reports the failure in the status bar.\n\nFile opens stream in bounded 16 KiB chunks. Live canvases and `.modr` files share a 4096×4096 dimension cap; oversized files are rejected on open and save, and a rejected open leaves the current canvas untouched.\n\n",
    optional=True,
)
must_replace(
    "docs/apps/drawing.md",
    "## Current Limitations\n\n",
    "## Current Limitations\n\n- Live canvases and open/save `.modr` files are limited to 4096×4096 pixels so oversized window growth or payloads cannot allocate unbounded drawing buffers.\n",
    optional=True,
)
must_replace(
    "docs/architecture.md",
    "Drawing captures reversible 32×32 tile preimages",
    "Drawing streams `.modr` opens in 16 KiB chunks, caps live canvases and files at 4096×4096, and leaves the active canvas unchanged when a read is rejected; it captures reversible 32×32 tile preimages",
    optional=True,
)
print("ALL PATCHES OK")
