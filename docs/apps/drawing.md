# Drawing

A pixel editor. You draw on an opaque RGB canvas with a few simple tools and save to `.modr` files in the [internal filesystem](../filesystem.md). There are no layers, selections or vector shapes.

## Using it

Open **Drawing** from the Start menu or desktop, double-click a `.modr` file in Filesystem, or run `open <file>.modr` in Terminal.

- Windows are titled `Drawing`, `Drawing 2` and so on, or `Drawing - <name>` once a file is open.
- Each `.modr` file opens in at most one window. Opening it again focuses that window.
- A new window starts with the Pen, the medium brush, black, and a blank canvas.
- The canvas fills the window between the toolbar and the status bar. The status bar shows hints and messages, and `[modified]` when there are unsaved changes.

### Toolbar

| Row | Buttons |
|-----|---------|
| 1 | New, Save, Open, Undo, Redo |
| 2 | Pen, Eraser, Fill, Pick, Line, Rect, Clear, S, M, L |
| 3 | RGB, then eight color swatches |

| Tool | How to use it |
|------|---------------|
| Pen | Drag to paint with a round brush |
| Eraser | Drag to paint the background color (245,245,248) |
| Fill | Click to fill the connected area (four-way) with the current color |
| Pick | Click a pixel to make its color current, then return to Pen |
| Line | Drag from one end to the other; always 1 pixel wide |
| Rect | Drag between opposite corners; 1 pixel outline |
| Clear | Clear the whole canvas (can be undone) |
| S / M / L | Brush radius 2, 5 or 10 pixels (Pen and Eraser) |

Fast drags are joined into continuous strokes. If you drag outside the canvas, the point is clamped to the nearest edge. A stroke that changes no pixels does not count as an edit.

### Colors

| Swatch | RGB |
|--------|-----|
| Black | 20,20,24 |
| White | 245,245,248 |
| Red | 220,70,70 |
| Green | 70,180,90 |
| Blue | 70,120,220 |
| Yellow | 230,200,60 |
| Orange | 230,140,50 |
| Purple | 150,80,200 |

Clicking a swatch selects that color and switches to Pen. **RGB** opens a prompt in the status bar: type three whole numbers from 0 to 255, as `255,0,128`, `255 0 128` or `255, 0, 128`, then press Enter. Hex, fractions and alpha are refused.

## Keyboard shortcuts

| Key | Action |
|-----|--------|
| Ctrl+S | Save (asks for a path if the sketch has no file yet) |
| Ctrl+O | Open a `.modr` file |
| Ctrl+N | New blank sketch |
| Ctrl+Z | Undo |
| Ctrl+Y, Ctrl+Shift+Z | Redo |
| Ctrl+D | Confirm: discard changes, or overwrite a file changed on disk |
| Ctrl+Shift+S | During an unsaved-changes decision for Close or New: save to a new path |

In a Save, Open or RGB prompt:

| Key | Action |
|-----|--------|
| Enter | Accept |
| Esc | Cancel |
| Tab | Complete the path (Save and Open only) |
| Left / Right, Home / End | Move the caret |
| Backspace / Delete | Delete a character |

Canvas shortcuts are paused while a prompt is open.

## Saving and opening

- A new sketch suggests `/home/monolith/drawings/sketch.modr`, then `sketch_2.modr`, `sketch_3.modr` and so on, using the first free name.
- Save adds `.modr` if the name does not end with it. `picture.mod` becomes `picture.mod.modr`; the existing suffix is not replaced.
- Save creates missing folders in the path.
- Ctrl+S on a sketch that already has a file writes straight to it. There is no general Save As; to make a variant, copy the file in Filesystem or with `cp`, open the copy and edit that.
- Open starts in `/home/monolith/drawings/` and completes only folders and `.modr` files. The suffix is matched ignoring case, so `SKETCH.MODR` works. `.mod` files are text and open in Text Editor.
- `~` means `/home/monolith` in both prompts.
- Opening a file replaces the canvas and clears undo history. Tool, brush and color stay as they were.
- If Save or Open fails, the canvas is unchanged and the prompt stays open so you can fix the path.
- Renaming a file to `.modr` does not convert it. The contents must already be a valid `.modr` image.

## Unsaved changes

Close, New and Open ask before throwing away changes:

| Key | Close or New | Open |
|-----|--------------|------|
| Ctrl+D | Discard and continue | Discard and open the file |
| Ctrl+S | Save, then continue | Save and cancel the Open |
| Ctrl+Shift+S | Save to a new path, then continue | Not available |
| Esc | Keep the sketch | Keep the sketch |

Repeating Close, New or Enter never discards on its own. A failed save does not continue. Shut Down is blocked while any sketch has unsaved changes. There is no autosave or recovery file.

## Files changed outside Drawing

- If another app changes the open file, the canvas stays as it is and the status bar shows `[external change]`.
- Before every save, Drawing checks whether the file on disk still matches what it last loaded or saved. If not, Ctrl+S pauses: Ctrl+D overwrites, Esc keeps the disk version.
- To load the other version, open the same path again (Ctrl+D first if you have unsaved changes).
- If the file is renamed or moved, the window follows it. If it is deleted, the window keeps the canvas and becomes untitled.

## Undo and resize

- Each changed stroke, fill or clear is one undo step. Pick does not create one.
- Undo keeps 32 steps and up to 64 MiB of saved pixels, oldest dropped first. Only the 32x32 tiles an edit touches are stored, so large canvases can still undo small edits. An edit too large for the budget clears the history.
- Resizing the window resizes the canvas: pixels stay anchored at the top left, new space is background color, and the right and bottom edges are cropped when shrinking. Resizing clears undo history and marks a saved sketch as modified.
- Changing the text size in Settings rescales the toolbar and status bar but does not change the canvas pixels.

## Status messages

| Message | Meaning |
|---------|---------|
| `Open failed: Drawing files must use .modr.` | The path does not end in `.modr` |
| `Open failed: could not read file.` | Missing or unreadable file |
| `Open failed: .modr file exceeds the size limit.` | Larger than any valid `.modr` file |
| `Open failed: not a valid .modr drawing file.` | Bad header, size or pixel data |
| `No path matches.` | Tab found nothing for the text before the caret |
| `RGB failed: ...` | The color was not three numbers from 0 to 255 |

## The .modr format

| Field | Content |
|-------|---------|
| Magic | `MODR` (4 bytes) |
| Width | 32-bit little-endian, 1 to 4096 |
| Height | 32-bit little-endian, 1 to 4096 |
| Pixels | RGB bytes, rows top to bottom, left to right |

The file must be exactly 12 + width x height x 3 bytes. Wrong magic, bad dimensions, a short file or extra bytes are rejected, and the current canvas is kept. Only pixels and size are stored: no tool, color, history or transparency. A canvas larger than 4096 on a side cannot be saved; Save reports this before writing anything.

## Limits

- Raster only: no layers, selections, transforms, zoom or text.
- No clipboard import or export, and no palette editor (use RGB or Pick).
- Undo history lives in memory and is lost on open, resize and exit.

## Developer notes

- `src/app/DrawingApp.{hpp,cpp}`: canvas, tools, prompts, unsaved-change state.
- `src/app/DrawingRaster.{hpp,cpp}`: line, rectangle and fill rasterizing, RGB parsing, `.modr` encoding and decoding. No SDL dependency, so it is tested headless.
- `src/window/detail/wm_body_07.inc`: `launchDrawing()`.
- The window manager owns file singletons, instance titles, session restore and close guards (`allowClose`, `IWindowController::restoreTrackedInstanceTitle`).
- Paths are normalized before they are stored, so titles, prompts and session records agree.
- The new-name generator must keep counting until it finds a free name and never wrap to a used one.
- Files are read and written in 16 KiB chunks with no full-file buffer. Canvas upload and caching details: [rendering.md](../internals/rendering.md#drawing).

Focused checks, run from the repo root after a build (the full suite is in [testing.md](../development/testing.md)):

```bash
./scripts/verify_drawing_integration.sh
g++ -std=c++23 scripts/test_drawing_roadmap.cpp src/app/DrawingRaster.cpp -o build/test_drawing_roadmap && ./build/test_drawing_roadmap
g++ -std=c++23 scripts/test_modr_format.cpp src/app/DrawingRaster.cpp -o build/test_modr_format && ./build/test_modr_format
g++ -std=c++23 scripts/test_drawing_state.cpp src/app/DrawingApp.cpp src/app/DrawingRaster.cpp src/fs/Filesystem.cpp src/detail/AtomicFile.cpp src/detail/AtomicTempOutput.cpp $(pkg-config --cflags --libs sdl2 SDL2_ttf) -o build/test_drawing_state && SDL_VIDEODRIVER=dummy ./build/test_drawing_state
```

The smoke test launches the real app for 3 seconds. It needs a built `build/monolith` and an X server on display `:198` (override with `MONOLITH_TEST_DISPLAY`):

```bash
Xvfb :198 -screen 0 1280x720x24 &
./scripts/headless_drawing_smoke.sh
```
