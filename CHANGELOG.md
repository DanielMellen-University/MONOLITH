# Changelog

## 2026-06: Documentation Reorganization

Restructured project documentation so each app has its own guide and the root README stays high-level.

- New documentation hub at `docs/README.md` with links to all guides.
- Per-app guides under `docs/apps/`: Terminal, Text Editor, Filesystem Browser, Drawing, Settings.
- New `docs/filesystem.md` for the shared internal filesystem layer.
- New `docs/development/scripts.md` for verification scripts (moved out of root README).
- Root README slimmed down: removed Drawing Controls and Developer Utilities sections.
- `docs/architecture.md` trimmed app-specific detail; links to app docs instead.
- `docs/drawing.md` moved to `docs/apps/drawing.md`.
- Updated `AGENTS.md` and `docs/vision.md` to reflect the new structure.

## 2026-06: Drawing Toolbar Controls

Drawing now exposes common file and history actions directly in the toolbar.

- Added toolbar buttons for New, Save, Open, Undo, and Redo.
- The toolbar is now two compact rows so file/history controls and drawing tools remain visible.
- Toolbar actions reuse the same behavior as the existing keyboard shortcuts.
- The canvas starts lower to make room for the expanded toolbar.

Builds cleanly.

## 2026-06: Drawing Path Completion

Drawing save/open prompts are easier to use.

- `Tab` completes paths while entering a Drawing save or open path.
- Open prompts complete directories and `.modr` files only.
- Open prompts now start in `/home/monolith/drawings/` instead of the current file path, so Tab helps pick another sketch.
- Save prompts complete existing directories and names so saving beside prior sketches is less tedious.
- Multiple matches complete to a shared prefix when possible and show a compact match preview in the status bar.
- Drawing open now requires the `.modr` extension, keeping the drawing format distinct from future module-style files.

Builds cleanly.

## 2026-06: Drawing Undo and Redo

Drawing is safer to use during sketching.

- Added capped canvas history for drawing changes.
- `Ctrl+Z` undoes strokes and canvas clears.
- `Ctrl+Y` and `Ctrl+Shift+Z` redo undone changes.
- Starting a new stroke or clearing the canvas resets redo history.
- Opening a file or resizing the canvas clears history so old snapshots are not applied to the wrong canvas size.
- Status text now hints the undo shortcut.

Builds cleanly.

## 2026-06: Drawing Color Fix

Toolbar swatches and painted strokes now use the same RGB values on Linux.

- **Canvas texture pixel format**: use `SDL_PIXELFORMAT_ABGR8888` on little-endian hosts so the internal `R,G,B,A` pixel buffer matches what SDL expects (fixes red/blue swap vs. toolbar swatches).
- Save/load `.modr` format unchanged — only the on-screen texture upload path was corrected.

See `src/app/DrawingApp.cpp` (`syncTexture`).

## 2026-06: Drawing Program

First native creative app — a simple pixel canvas for sketching inside Monolith.

### Key Features
- **Drawing app** (`DrawingApp`): pen and eraser tools, three brush sizes, eight preset colors, clear canvas.
- Drag on the canvas to paint; eraser restores the canvas background color.
- **Save / load** via the internal filesystem using the `.modr` binary format (RGB raster, top-down).
  - Ctrl+S saves (prompts for path on first save; defaults to `/home/monolith/drawings/sketch.modr` or next free name).
  - Ctrl+O opens a `.modr` file by virtual path.
  - Ctrl+N clears the canvas for a fresh sketch.
- Window title updates to `Drawing - filename.modr` after save/open.
- Uses the standard multi-instance titling system (`Drawing`, `Drawing 2`, etc.).
- **Start menu** entry added between Settings and Shut Down.
- **Window Manager**: forwards `SDL_MOUSEBUTTONUP` to the focused app so drag-to-draw releases cleanly.
- First-run seeding creates `/home/monolith/drawings/` alongside existing home directories.

### Developer Utilities
- `scripts/verify_drawing_integration.sh` — static wiring checks (no display required)
- `scripts/test_modr_format.cpp` — headless `.modr` encode/decode roundtrip test
- `scripts/headless_drawing_smoke.sh` — optional Xvfb launch smoke test

See:
- `src/app/DrawingApp.{hpp,cpp}`
- `src/window/WindowManager.{hpp,cpp}` (`launchDrawing`, Start menu, mouse-up forwarding)
- `src/main.cpp` (drawings directory seed)
- Updated README and architecture docs

## 2026-06: Window Geometry Cleanup

Focused Window Manager cleanup to keep taskbar, usable desktop, and title-bar button geometry consistent.

- Added shared geometry helpers for the taskbar rectangle, usable desktop rectangle, usable desktop bottom edge, logical-to-screen rect conversion, and title-bar button hitboxes.
- Window creation now clamps new windows into the usable desktop area before app resize notification.
- Maximized windows now use the same usable desktop rectangle as drag and resize clamping.
- Title-bar button rendering and click handling now use the same rectangle calculations, reducing drift between what is drawn and what is clickable.
- Start menu and taskbar rendering now use the shared taskbar geometry helper.

See `src/window/WindowManager.{hpp,cpp}` and updated architecture docs.

## Previous History

Historical detailed entries from earlier development (Terminal/Editor polish, dynamic instance titling, Start Menu + Settings, Filesystem Browser, etc.) live in git history.