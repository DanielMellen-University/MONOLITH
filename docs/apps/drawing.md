# Drawing App

The Drawing app is Monolith's native sketching tool. It provides a pixel canvas, simple drawing tools, undo/redo, and save/load support through the [internal filesystem](../filesystem.md).

Drawing files use the `.modr` extension (Monolith Drawing Raster).

## Quick Start

1. Open **Drawing** from Start.
2. Choose a color and brush size, then drag on the canvas with **Pen** selected.
3. Use **Fill**, **Line**, **Rect**, or **Pick** when the task calls for a different tool.
4. Press **Ctrl+S**, choose a path if this is a new sketch, and press Enter.

The canvas is a raster surface. It fills the space between the toolbar and the status bar, and its pixel dimensions follow the Drawing window's client area. Resizing the window preserves the existing pixels from the top-left corner and clears undo/redo history for the new canvas size.

## Launching

Open **Drawing** from the Start menu. Multiple Drawing windows can be open at once:

- `Drawing`
- `Drawing 2`
- `Drawing 3`

After saving or opening a file, the window title changes to the file name, for example `Drawing - sketch.modr`.

## Toolbar

The toolbar has three rows.

Top row:

- **New**: clears the canvas, resets the file path, and restores the window's instance title (`Drawing`, `Drawing 2`, etc.).
- **Save**: saves the current sketch. If the sketch has no file path yet, Drawing prompts for one.
- **Open**: prompts for a `.modr` file path.
- **Undo**: undoes the last stroke or clear.
- **Redo**: redoes the last undone change.

Second row:

- **Pen**: paints with the selected color.
- **Eraser**: paints with the canvas background color.
- **Fill**: flood-fills a connected region with the selected color.
- **Pick**: samples the clicked canvas pixel as custom RGB, then returns to Pen.
- **Line**: drag to paint a straight 1px stroke between two points.
- **Rect**: drag to paint a 1px rectangle boundary.
- **Clear**: clears the whole canvas.
- **S / M / L**: selects small, medium, or large brush size.

Third row:

- **RGB**: type a custom `r,g,b` color (0–255) in the status bar.
- Color swatches: selects the active pen color and switches back to Pen (clears custom RGB).

The **S**, **M**, and **L** buttons select brush radii of 2, 5, and 10 pixels. The brush stamps a filled circle at each point in a drag. Line and Rect are always 1 pixel wide and do not use the brush size.

## Colors

The eight swatches select the active pen color. Choosing a swatch also switches to Pen and disables the custom RGB color.

Select **RGB** to edit the current custom color in the status bar. Enter exactly three channel values, either as `r,g,b` or `r g b`, with every value between 0 and 255. Enter applies the color and switches back to Pen when needed. Escape cancels the prompt.

Pick samples the RGB value at the clicked canvas pixel, stores it as the custom color, and switches back to Pen. Sampling does not change the canvas or add an undo state.

## Mouse Controls

- Drag on the canvas to draw.
- Drag with Pen selected to paint with the active color.
- Drag with Eraser selected to restore the canvas background color.
- Drag with Line selected to stroke a straight line from press to release.
- Drag with Rect selected to stroke a rectangle from press to release.
- Click with Fill selected to flood-fill the connected region under the cursor.
- Click with Pick selected to sample the pixel under the cursor without changing the canvas or undo history.
- Click a toolbar button to change tools, open prompts, or run file/history actions.

## Keyboard Shortcuts

| Shortcut | Action |
|----------|--------|
| Ctrl+S | Save the current sketch |
| Ctrl+O | Open a `.modr` sketch by path |
| Ctrl+N | Start a new sketch |
| Ctrl+Z | Undo the last stroke or clear |
| Ctrl+Y / Ctrl+Shift+Z | Redo |
| Tab | Complete paths in save/open prompt |
| Enter | Confirm save/open prompt |
| Esc | Cancel save/open prompt |
| Backspace | Edit save/open prompt path |

While a path prompt is active, typed printable characters are added to the prompt. Tab completes a matching directory or `.modr` file; with several matches it completes the shared prefix or shows a short match preview in the status bar.

## Saving

Drawing saves sketches into the internal Monolith filesystem. New sketches default under:

```text
/home/monolith/drawings/
```

The first default name is:

```text
/home/monolith/drawings/sketch.modr
```

If that file already exists, Drawing picks the next free name, such as:

```text
/home/monolith/drawings/sketch_2.modr
```

If you save without typing `.modr`, Drawing adds it automatically.

## Opening

Drawing opens `.modr` files only. The shell routes `.modr` paths from Terminal `open` and the Filesystem Browser to Drawing; `.mod` remains a text file and does not open in Drawing. This keeps drawing files distinct from future module-style files that may use similar names.

The Open prompt starts in:

```text
/home/monolith/drawings/
```

Use `Tab` while the prompt is active to complete directory names or `.modr` files.

Examples:

```text
/home/monolith/drawings/s
```

Pressing `Tab` can complete that to:

```text
/home/monolith/drawings/sketch.modr
```

If multiple files match, Drawing completes the shared prefix when possible. If no shared prefix can be extended, the status bar shows a compact preview of matching names.

Opening a missing file, a non-`.modr` path, or corrupt data leaves the current sketch open and reports the failure in the status bar.

## Undo And Redo

Drawing stores a capped history of canvas snapshots.

- A snapshot is recorded before each stroke.
- A snapshot is recorded before Clear.
- Undo and redo operate on full canvas states.
- Starting a new stroke or clearing after an undo resets redo history.
- Opening a file clears history.
- Resizing the canvas clears history so old snapshots are not applied to the wrong canvas size.

The current cap is 32 history states.

## File Format

`.modr` is a simple binary raster format:

- Magic bytes: `MODR`
- Width: 32-bit little-endian integer
- Height: 32-bit little-endian integer
- Pixel payload: RGB bytes, top-to-bottom and left-to-right

Width and height must be between 1 and 4096 pixels. The decoder requires the file to contain exactly the header plus `width * height * 3` payload bytes.

Internally, the live canvas stores pixels as `R,G,B,A`. The saved file stores only RGB because the canvas is fully opaque.

## Unsaved Changes

A dirty sketch (status bar `[modified]`) guards destructive actions:

| Action | First time (dirty) | Confirm |
|--------|--------------------|---------|
| Close window (X / shell close) | Status warning | Close again to discard, or Ctrl+S to save |
| New / Ctrl+N | Status warning | New again to discard |
| Open path | Status warning | Confirm open again to discard |

Clear (toolbar) remains undoable and does not use this guard.

## Current Limitations

- Custom RGB can be entered through the status-bar `r,g,b` prompt or sampled with Pick; there is no palette editor yet.
- No clipboard import/export yet.
- Dirty guards use status-bar double-confirm, not a modal dialog.
- Undo history is in memory only and resets when a drawing file is opened, the canvas is resized, or the app exits.

## Developer Notes

Main implementation files:

- `src/app/DrawingApp.hpp`
- `src/app/DrawingApp.cpp`
- `src/app/DrawingRaster.hpp` / `DrawingRaster.cpp` - line/rect raster, pixel reads, custom RGB parse, `.modr` encode/decode (shared with headless tests)
- `src/window/detail/wm_body_07.inc` — `launchDrawing()` and Drawing window creation
- `src/window/detail/wm_body_01.inc` / `wm_body_08.inc` — mouse-up forwarding, open routing, and session restore
- `src/app/App.hpp` — `IWindowController::restoreTrackedInstanceTitle()`, `allowClose` for dirty guards

Canvas GPU path (`syncTexture`): recreate the streaming texture only when missing or size-changed; upload CPU pixels only while `m_textureDirty` is set by paint, undo, load, or resize.

Verification scripts: see [Development Scripts](../development/scripts.md).
