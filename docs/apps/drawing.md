# Drawing App

The Drawing app is Monolith's native sketching tool. It provides a pixel canvas, simple drawing tools, undo/redo, and save/load support through the [internal filesystem](../filesystem.md).

Drawing files use the `.modr` extension (Monolith Drawing Raster).

## Launching

Open **Drawing** from the Start menu. Multiple Drawing windows can be open at once:

- `Drawing`
- `Drawing 2`
- `Drawing 3`

After saving or opening a file, the window title changes to the file name, for example `Drawing - sketch.modr`.

## Toolbar

The toolbar has two rows.

Top row:

- **New**: clears the canvas, resets the file path, and restores the window's instance title (`Drawing`, `Drawing 2`, etc.).
- **Save**: saves the current sketch. If the sketch has no file path yet, Drawing prompts for one.
- **Open**: prompts for a `.modr` file path.
- **Undo**: undoes the last stroke or clear.
- **Redo**: redoes the last undone change.

Second row:

- **Pen**: paints with the selected color.
- **Eraser**: paints with the canvas background color.
- **Clear**: clears the whole canvas.
- **S / M / L**: selects small, medium, or large brush size.
- Color swatches: selects the active pen color and switches back to Pen.

## Mouse Controls

- Drag on the canvas to draw.
- Drag with Pen selected to paint with the active color.
- Drag with Eraser selected to restore the canvas background color.
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

Drawing opens `.modr` files only. This keeps drawing files distinct from future module-style files that may use similar names.

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

Internally, the live canvas stores pixels as `R,G,B,A`. The saved file stores only RGB because the canvas is fully opaque.

## Current Limitations

- No bucket fill tool yet.
- No custom color picker yet.
- No clipboard import/export yet.
- Drawing files do not open directly from the Filesystem Browser yet.
- Undo history is in memory only and resets when a drawing file is opened, the canvas is resized, or the app exits.

## Developer Notes

Main implementation files:

- `src/app/DrawingApp.hpp`
- `src/app/DrawingApp.cpp`
- `src/window/WindowManager.cpp` — `launchDrawing()`, mouse-up forwarding for drag interactions
- `src/app/App.hpp` — `IWindowController::restoreTrackedInstanceTitle()` for New/Ctrl+N title reset

Verification scripts: see [Development Scripts](../development/scripts.md).