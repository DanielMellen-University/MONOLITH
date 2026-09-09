# Drawing App

The Drawing app is Monolith's native sketching tool. It provides a pixel canvas, simple drawing tools, undo/redo, and save/load support through the [internal filesystem](../filesystem.md).

Drawing files use the `.modr` extension (Monolith Drawing Raster).

## Read This First

Drawing is a pixel editor, not a layer or vector editor. The canvas is edited in memory and is not written to the internal filesystem until **Save** succeeds.

- Save the sketch before closing, choosing **New**, or opening another file when the status bar shows `[modified]`.
- Drawing files must end in `.modr`. The suffix is matched case-insensitively when opening.
- `.mod` is intentionally treated as a normal text file. Saving a path such as `picture.mod` produces `picture.mod.modr` rather than changing the requested name.
- A saved file contains only the opaque RGB canvas and its dimensions. The active tool, colors, brush size, undo history, and window state are not part of the file.
- The status bar is the file prompt, progress display, and recovery guide. Press **Enter** to confirm a prompt and **Esc** to cancel it.

## At A Glance

| Item | Behavior |
|------|----------|
| Canvas | Opaque raster pixels sized to the Drawing client area |
| Default folder | `/home/monolith/drawings/` in the internal filesystem |
| File type | `.modr`, matched case-insensitively when opening |
| Editing model | Direct pixel edits with up to 32 in-memory undo states |
| Prompts | Inline in the status bar, with caret editing and Tab completion for paths |
| Persistence | Pixels and canvas dimensions are saved; tools, colors, and history are not |

Drawing has no separate file-picker or modal prompt. Save, Open, and custom RGB input temporarily turn the status bar into an editor. The active prompt shows a caret and scrolls horizontally when its text is longer than the window.

## Quick Start

1. Open **Drawing** from Start.
2. Choose a color and brush size, then drag on the canvas with **Pen** selected.
3. Use **Fill**, **Line**, **Rect**, or **Pick** when the task calls for a different tool.
4. Press **Ctrl+S**, choose a path if this is a new sketch, and press Enter.

The canvas is a raster surface. It fills the space between the toolbar and the status bar, and its pixel dimensions follow the Drawing window's client area. Resizing the window preserves the existing pixels from the top-left corner and clears undo/redo history for the new canvas size.

## First Session Walkthrough

### Create And Save A Sketch

1. Open **Drawing** from Start.
2. Choose a swatch, or choose **RGB** and enter three channel values.
3. Select **Pen**, choose **S**, **M**, or **L**, and drag on the canvas.
4. Use **Line**, **Rect**, or **Fill** for shape and region work. Use **Pick** to sample a canvas pixel and continue with that color.
5. Press **Ctrl+S**. For a new sketch, Drawing opens an inline path prompt with a suggested name.
6. Press **Enter** to save, or press **Tab** while editing a path to complete a directory or `.modr` filename.

### Reopen A Sketch

1. Press **Ctrl+O** or click **Open**.
2. The prompt starts in `/home/monolith/drawings/`.
3. Type the beginning of a directory or filename, then press **Tab**. Repeated Tab presses extend the shared prefix or show a short match list in the status bar.
4. Press **Enter** to open the completed path.

Opening a file replaces the current canvas and clears its undo/redo history. The current tool, brush size, and active color stay selected because they belong to the editor session, not the `.modr` file.

## Basic Workflow

1. Start with **New** if you want a blank sketch.
2. Pick a swatch or set a custom color with **RGB**.
3. Choose **Pen**, **Eraser**, **Fill**, **Line**, or **Rect**.
4. Choose **S**, **M**, or **L** for brush-based tools, then draw on the canvas.
5. Use **Ctrl+Z** and **Ctrl+Y** while refining the sketch.
6. Press **Ctrl+S**, accept the suggested `.modr` path, and press Enter.

The canvas is the source of truth: tools modify raster pixels directly, and every saved file contains the complete opaque canvas. Drawing does not create vector objects or layers, so a completed line or rectangle cannot be selected and edited separately.

## Launching

Open **Drawing** from the Start menu. Multiple Drawing windows can be open at once:

- `Drawing`
- `Drawing 2`
- `Drawing 3`

After saving or opening a file, the window title changes to the file name, for example `Drawing - sketch.modr`.

## Shell And Session Integration

Drawing participates in the same file workflow as the Terminal and Filesystem Browser:

- Terminal `open <path>` sends a case-insensitive `.modr` path to Drawing.
- Double-clicking a `.modr` file in the Filesystem Browser opens it in Drawing.
- The Filesystem Browser can also use **Open with Drawing**; Drawing still rejects paths that are not `.modr` files.
- Opening a `.modr` file that is already open focuses its existing Drawing window instead of creating a duplicate. A minimized matching window is restored first.
- When Monolith restores a saved desktop session, an open Drawing window keeps its geometry and bound `.modr` path.

All other file types continue to open in Text Editor through the shell's default routing. In particular, `.mod` is a text file, not a Drawing file.

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

### Tool Behavior

| Tool | Interaction | History |
|------|-------------|---------|
| Pen | Drag to paint with a filled circular brush | One undo state per drag |
| Eraser | Drag to paint the canvas background color | One undo state per drag |
| Fill | Click a connected region | One undo state per fill |
| Pick | Click one pixel to copy its RGB value, then return to Pen | No canvas change |
| Line | Drag from one endpoint to the other | One undo state per drag |
| Rect | Drag the two opposite corners | One undo state per drag |
| Clear | Clear the entire canvas | One undo state per clear |

Pen and Eraser interpolate between mouse events, so fast drags remain continuous. Line and Rect commit when the mouse button is released; releasing outside the canvas uses the last canvas point reached.

## Canvas Behavior

The canvas is sized from the Drawing client area. The toolbar occupies the top 96 pixels and the status bar occupies the bottom 22 pixels; the remaining area is the raster surface. Canvas coordinates are logical pixels, not host-window pixels.

When the window is resized:

- Existing pixels stay at their original top-left coordinates.
- A larger canvas is filled with the standard light background.
- A smaller canvas crops pixels at the right and bottom edges.
- Undo and redo history is cleared because the canvas dimensions changed.

The standard canvas background is RGB `245,245,248`. Eraser uses that same color, so it restores the background rather than revealing transparency.

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
| Left / Right | Move the save/open/RGB prompt caret by one UTF-8 character |
| Home / End | Move to the beginning or end of the active prompt |
| Backspace / Delete | Remove the previous or next complete UTF-8 character |

While a prompt is active, typed printable UTF-8 characters are inserted at the caret. Tab completes a matching directory or `.modr` file; with several matches it completes the shared prefix or shows a short match preview in the status bar. Long prompts stay at native text size and scroll horizontally to keep the caret visible.

### Prompt Behavior

- **Save** starts with the current file path when one exists. A new sketch starts with the next free name under `/home/monolith/drawings/`.
- **Open** starts at `/home/monolith/drawings/` and filters file completion to `.modr` entries.
- **RGB** starts with the current active color and accepts exactly three integer channels. Tab completion does not apply to RGB input.
- These prompts are inline status-bar inputs. Left/Right/Home/End move the caret, typed text is inserted at that position, and Backspace/Delete remove complete UTF-8 characters.
- Tab replaces only the final path component before the caret. If the caret is inside a directory component, completion waits until the caret is in the final component so text after it is not rewritten.
- Enter accepts the active prompt and Escape cancels it. Save adds `.modr` when the entered path does not already end in `.modr`; entering `picture.mod` therefore saves as `picture.mod.modr`.
- If a dirty sketch blocks Open, the first confirmation keeps the path prompt active. Confirming the same open action again discards the unsaved canvas and loads the file.

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

Drawing always writes a `.modr` document. If you save without typing `.modr`, Drawing appends the suffix automatically. It does not replace another suffix: entering `picture.mod` creates `picture.mod.modr`.

## Opening

Drawing opens `.modr` files only, with case-insensitive suffix matching. The shell routes `.modr` paths from Terminal `open` and the Filesystem Browser to Drawing; `.mod` remains a text file and does not open in Drawing. This keeps drawing files distinct from future module-style files that may use similar names.

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

Opening a valid file replaces the current canvas dimensions and pixels. The file's pixels are loaded as opaque RGB data, and the undo/redo stacks are cleared. The active tool, brush size, and color selection remain editor state and are not read from the file.

## Undo And Redo

Drawing stores a capped history of canvas snapshots.

- A snapshot is recorded before each stroke.
- A snapshot is recorded before each Fill operation.
- A snapshot is recorded before Clear.
- Undo and redo operate on full canvas states.
- Starting a new stroke or clearing after an undo resets redo history.
- Picking a color does not create a history state.
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

The format has no metadata for tools, brush size, custom color, undo history, or layers. Those are editor state and are not restored when the file is reopened.

The format is intentionally small and strict. A file with a wrong magic header, dimensions outside the supported range, truncated payload, or trailing bytes is rejected instead of partially opening.

## Unsaved Changes

A dirty sketch (status bar `[modified]`) guards destructive actions:

| Action | First time (dirty) | Confirm |
|--------|--------------------|---------|
| Close window (X / shell close) | Status warning | Close again to discard, or Ctrl+S to save |
| New / Ctrl+N | Status warning | New again to discard |
| Open path | Status warning | Confirm open again to discard |

Clear (toolbar) remains undoable and does not use this guard.

## Status Bar Messages

The status bar is both the command hint area and the app's lightweight feedback channel. Common messages mean:

| Message pattern | Meaning / next action |
|-----------------|-----------------------|
| `Pen ready...` or `Tool: ...` | The selected tool is ready for the next canvas action. |
| `Save as (...)` | A Save path prompt is active. Edit the path, then press Enter or Escape. |
| `Open path (...)` | An Open path prompt is active. Tab-complete a directory or `.modr` file, then press Enter. |
| `Custom RGB ...` | The RGB prompt is active. Enter three channels from 0 through 255. |
| `Path completed...` | Tab found a completion. Review the path before confirming it. |
| `No path matches.` | Tab found no matching directory or `.modr` file at the caret. Keep editing the path. |
| `Saved: ...` / `Opened: ...` | The operation completed and includes the normalized internal path. |
| `Save failed: ...` / `Open failed: ...` | The operation was rejected. The current canvas remains open so it can be corrected or saved elsewhere. |
| `RGB failed: ...` | The color was not changed. Enter exactly three integer channels in the accepted range. |
| `New sketch.` or `Cancelled.` | The requested reset or prompt cancellation completed. |

When the canvas has unsaved edits, `[modified]` is appended to the status bar. Save before closing, creating a new sketch, or opening another file. A failed save or open does not discard the current canvas.

## Troubleshooting

| Symptom | Cause and fix |
|---------|---------------|
| `Open failed: Drawing files must use .modr.` | Open accepts only `.modr` files, with case-insensitive suffix matching. A `.mod` file is text and belongs in Text Editor. |
| Saving `picture.mod` creates `picture.mod.modr` | Save appends `.modr` when the entered path does not already end in that suffix. Enter the intended `.modr` name explicitly. |
| `No path matches.` after pressing Tab | Completion compares the typed filename prefix exactly. Move the caret to the final path component, correct the prefix, and press Tab again. |
| `Open failed: could not read file.` | The virtual path is missing or could not be read. The current canvas remains open; correct the path or save the current sketch elsewhere. |
| `Open failed: not a valid .modr drawing file.` | The file header, dimensions, or pixel payload is invalid. Drawing does not partially load corrupt data. |
| The status bar shows `[modified]` | The canvas has edits that are not saved. Press **Ctrl+S** before closing, choosing **New**, or opening another sketch. |
| Undo is no longer available after resizing | Resizing changes the canvas dimensions, so Drawing clears history rather than applying snapshots to a different-sized canvas. |

## Current Limitations

- Custom RGB can be entered through the status-bar `r,g,b` prompt or sampled with Pick; there is no palette editor yet.
- The editor is raster-only: there are no layers, selections, transforms, zoom controls, or vector objects.
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

Verification scripts: see [Development Scripts](../development/scripts.md). The focused Drawing checks are:

```bash
./scripts/verify_drawing_integration.sh
g++ -std=c++23 scripts/test_drawing_roadmap.cpp src/app/DrawingRaster.cpp -o build/test_drawing_roadmap && ./build/test_drawing_roadmap
```
