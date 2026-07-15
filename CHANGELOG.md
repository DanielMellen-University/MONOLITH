# Changelog

## 2026-07: Snake & Minesweeper polish

Gameplay and presentation upgrades for both Start menu **Games**.

### Snake
- Persistent high score (`~/.monolith/snake_highscore.txt`) with **NEW BEST!** on beat.
- Direction input queue (up to two turns) so quick corners are not dropped.
- HUD: score / best / length measured left-to-right; controls hint right-aligned when space allows (no label collisions).
- Click to pause/resume or restart after game over; centered pause/game-over/win text stacks.
- Checkerboard board, food highlight, body gradient, directional head eyes, eat flash.

### Minesweeper
- Chord open: middle-click or left-click on a revealed number when flags match.
- Right-click cycles empty → flag → question mark.
- Best times per difficulty (`~/.monolith/minesweeper_best.txt`) with **NEW BEST!** on win.
- Face button for new game; hit mine highlighted; wrong flags marked **X** on loss.
- First-click neighborhood clear on Intermediate as well as Beginner; centered win/lose overlays; click overlay to restart.

Builds cleanly.

## 2026-07: Start Menu Categories

The Start menu groups apps only where it clearly helps.

- **Games** category header with **Snake** and **Minesweeper** nested underneath.
- Other apps (Terminal, Text Editor, Filesystem, Drawing, Settings) stay top-level.
- Separators frame the Games section and Shut Down.
- Menu height is computed from the entry list so new categories do not clip.

Builds cleanly.

## 2026-07: Snake and Minesweeper

Two classic games as native Monolith apps.

- **Snake**: 20×20 grid, WASD/arrows, pause on blur, mild speed-up with score, multi-instance titles.
- **Minesweeper**: Beginner / Intermediate / Expert, first-click safety, flood reveal, flags, timer.
- **Window Manager**: `App::update()` is now called each frame for non-minimized windows (game ticks/timers).
- Start menu entries: Snake and Minesweeper (before Shut Down).
- Docs: `docs/apps/snake.md`, `docs/apps/minesweeper.md`; architecture notes app update dispatch.

Builds cleanly.

## 2026-07: Text Editor Syntax Highlighting

The Text Editor now colorizes source text while you type.

- Per-line highlighting for comments, strings, numbers, and keywords.
- **Light mode** for plain-text files (`.txt`, etc.) avoids keyword false positives in prose; apostrophes in words stay plain text.
- **Code mode** for common source extensions (`.cpp`, `.py`, `.js`, `.rs`, `.md`, etc.) adds keyword coloring.
- Status bar reserves its own height so shortcut hints never overlap file content (fixes `welcome.txt` garble on small windows).

Builds cleanly.

## 2026-07: Filesystem Copy/Move, Terminal Open, Drawing Fill

Three usability upgrades across the shell and apps.

- **Filesystem Browser**: right-click Copy, Cut, and Paste (plus Ctrl+C/X/V) for files and folders, including recursive directory trees.
- **Terminal**: `edit <file>` opens text files in the Text Editor; `open <path>` routes `.modr` files to Drawing and other files to the Editor.
- **Drawing**: new Fill toolbar tool flood-fills connected regions with the active color (undo/redo supported).

Builds cleanly.

## 2026-06: Filesystem Drawing Open & Editor Path Prompts

Tighter shell integration and editor polish.

- **Filesystem Browser**: double-click / Open on `.modr` files launches Drawing via `IWindowController::openInDrawing()` (singleton per file, like editors).
- **Drawing**: `launchDrawing(path)` loads sketches from the shell; save/open/update binding tracked in Window Manager.
- **Text Editor**: Ctrl+O open prompt, Ctrl+S / Ctrl+Shift+S save and save-as prompts with Tab completion; Ctrl+Y / Ctrl+Shift+Z redo added.

Builds cleanly.

## 2026-06: Doc Audit & Drawing Instance Titles

Aligned documentation with the codebase and fixed a Drawing title regression.

- **Docs**: Terminal prompt examples use `~>`; README notes first-launch demo windows; filesystem table adds `documents/` and `welcome.txt`; architecture documents 1280×720 rendering, taskbar/minimize behavior, and `restoreTrackedInstanceTitle()`; vision roadmap marks shipped vs planned items.
- **Code**: `IWindowController::restoreTrackedInstanceTitle()` lets apps return to WM-managed instance names; Drawing New/Ctrl+N uses it so `Drawing 2` stays `Drawing 2` instead of collapsing to `Drawing`.
- **WindowManager.hpp**: Default logical desktop size and comments updated to match runtime (1280×720).

Builds cleanly.

## 2026-06: Settings Desktop Background

Settings now controls a real desktop preference.

- Added an **Appearance** section with six desktop background color presets.
- Clicking a swatch updates the live desktop immediately.
- The choice persists to `~/.monolith/desktop_settings.txt` across restarts.
- Window Manager owns the setting; Settings applies changes through `IWindowController`.

Builds cleanly.

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