# Drawing App

The Drawing app is Monolith's native sketching tool. It provides a pixel canvas, simple drawing tools, undo/redo, and save/load support through the [internal filesystem](../filesystem.md).

Drawing files use the `.modr` extension (Monolith Drawing Raster).

If a bound sketch's parent directory is deleted, Drawing keeps the current canvas but releases the file binding. An active Open or Save prompt returns to the nearest surviving parent directory.

## Guide Map

- [Quick Start](#quick-start)
- [Workflow Recipes](#workflow-recipes)
- [Keyboard-First Reference](#keyboard-first-reference)
- [File Lifecycle](#file-lifecycle)
- [Toolbar](#toolbar)
- [Display Scaling](#display-scaling)
- [Colors](#colors)
- [Mouse Controls](#mouse-controls)
- [Prompt Behavior](#prompt-behavior)
- [Saving](#saving)
- [External File Changes](#external-file-changes)
- [Common File Workflows](#common-file-workflows)
- [Opening](#opening)
- [Undo And Redo](#undo-and-redo)
- [File Format](#file-format)
- [Unsaved Changes](#unsaved-changes)
- [Status Bar Messages](#status-bar-messages)
- [Troubleshooting](#troubleshooting)

## Read This First

Drawing is a pixel editor, not a layer or vector editor. The canvas is edited in memory and is not written to the internal filesystem until **Save** succeeds.

- When `[modified]` is shown, choose **Ctrl+D** to discard for Close, New, or Open; **Ctrl+S** to save before Close or New; or **Esc** to keep the sketch.
- Drawing files must end in `.modr`. The suffix is matched case-insensitively when opening.
- `.mod` is intentionally treated as a normal text file. Saving a path such as `picture.mod` produces `picture.mod.modr` rather than changing the requested name.
- A saved file contains only the opaque RGB canvas and its dimensions. The active tool, colors, brush size, undo history, and window state are not part of the file.
- The status bar is the file prompt, progress display, and recovery guide. Press **Enter** to confirm a prompt and **Esc** to cancel it.

## Daily Operator Card

| Goal | Fastest path |
|------|--------------|
| Start a blank sketch | Open **Drawing**, or press **Ctrl+N** in an existing Drawing window. |
| Draw | Choose a swatch, select **Pen**, **Eraser**, **Fill**, **Line**, or **Rect**, then use the canvas. |
| Sample a color | Select **Pick**, click a canvas pixel, and continue with Pen. |
| Save a new sketch | Press **Ctrl+S**, keep or edit the suggested `.modr` path, then press **Enter**. |
| Save an open sketch | Press **Ctrl+S**. If the bound file changed externally, press **Ctrl+D** at the warning to overwrite it, or **Esc** to keep the disk version. |
| Open another sketch | Press **Ctrl+O**, type or Tab-complete a `.modr` path, then press **Enter**. |
| Recover from a mistaken stroke | Press **Ctrl+Z**. Use **Ctrl+Y** or **Ctrl+Shift+Z** to redo it. |
| Rename or move a saved sketch | Use Filesystem Browser Rename or Terminal `mv`, keeping the full `.modr` suffix. |

The status bar always shows the next useful action. During Save, Open, or RGB input, the status bar becomes the active prompt and canvas shortcuts are paused until **Enter** or **Esc**.

## File Naming Rules

`.modr` is the Drawing file type. Keep the suffix visible when naming or renaming a sketch:

| Operation | Result |
|-----------|--------|
| Save as `sketch` | Saves as `sketch.modr`. |
| Save as `sketch.modr` | Saves with that exact `.modr` suffix. |
| Save as `sketch.mod` | Saves as `sketch.mod.modr`; the existing suffix is not replaced. |
| Open `sketch.modr` | Opens in Drawing when the file is valid. |
| Open `sketch.mod` | Rejected by Drawing; `.mod` remains a Text Editor file. |
| Rename in Filesystem Browser | Uses the name entered; add `.modr` yourself when renaming a Drawing file. |

The suffix is matched case-insensitively when opening, so `SKETCH.MODR` is still a Drawing file. Save and Open operate on the internal Monolith filesystem, not the host filesystem.

Save and rename follow different rules. Save adds `.modr` when the typed path has no `.modr` suffix, but Filesystem Browser rename and Terminal `mv` use the destination name exactly as entered. To convert an existing file into a Drawing file, rename it to the complete name, for example `draft.mod` to `draft.modr`, then open the renamed file. Renaming it to `draft` does not add the suffix automatically, and `draft.mod` remains a Text Editor file.

For a normal rename:

1. In Filesystem Browser, select the sketch and choose **Rename**, or use Terminal `mv`.
2. Keep the `.modr` suffix in the new name.
3. Open the renamed file from Filesystem Browser, or run `open <path>` in Terminal.

Example Terminal workflow:

```text
mv /home/monolith/drawings/draft.mod /home/monolith/drawings/draft.modr
open /home/monolith/drawings/draft.modr
```

If the sketch is already open, Monolith updates its title and Save target after a successful rename or move. The open window keeps the canvas in memory while its canonical virtual path changes.

Renaming changes the virtual filename only; it does not convert file contents. A file renamed from `.mod` to `.modr` must already contain a valid `MODR` header, dimensions, and RGB payload or Drawing will reject it as corrupt. To create a new Drawing file from a blank sketch or another image, use Drawing Save or copy an existing valid `.modr` file.

## Virtual Path Handling

Drawing stores internal filesystem paths in canonical form. Before a path is opened, saved, bound to a window, or shown in status text, Monolith removes repeated separators and resolves `.` and `..` segments.

| Entered path | Canonical path |
|--------------|----------------|
| `/home/monolith/drawings/./sketch.modr` | `/home/monolith/drawings/sketch.modr` |
| `/home/monolith/drawings/../drawings/sketch.modr` | `/home/monolith/drawings/sketch.modr` |
| `//home//monolith//drawings//sketch.modr` | `/home/monolith/drawings/sketch.modr` |
| `~/drawings/sketch.modr` | `/home/monolith/drawings/sketch.modr` |

In Drawing's Open and Save prompts, a leading `~` or `~/` expands to `/home/monolith`; Tab completion searches the resolved directory but keeps the shorthand in the prompt until accepted. `~name` is not expanded. Normalization only changes the path spelling. It does not make a missing file valid. Save separately creates missing parent directories before writing when the internal filesystem allows it; Open never creates a missing file. The same canonical path is used for Drawing's title, Save target, session record, one-window-per-file routing, and active path prompt. Directory moves and file renames therefore update the normalized path consistently.

## At A Glance

| Item | Behavior |
|------|----------|
| Canvas | Opaque raster pixels sized to the Drawing client area |
| Default folder | `/home/monolith/drawings/` in the internal filesystem |
| File type | `.modr`, matched case-insensitively when opening |
| Editing model | Direct pixel edits with up to 32 combined in-memory undo/redo states, capped at 64 MiB |
| Prompts | Inline in the status bar, with caret editing and Tab completion for paths |
| Persistence | Pixels and canvas dimensions are saved; tools, colors, and history are not |
| Startup defaults | Pen tool, medium brush, black swatch, and a new blank sketch |

Drawing has no separate file-picker or modal prompt. Save, Open, and custom RGB input temporarily turn the status bar into an editor. The active prompt shows a caret and scrolls horizontally when its text is longer than the window.

## Quick Start

1. Open **Drawing** from Start.
2. Choose a color and brush size, then drag on the canvas with **Pen** selected.
3. Use **Fill**, **Line**, **Rect**, or **Pick** when the task calls for a different tool.
4. Press **Ctrl+S**, choose a path if this is a new sketch, and press Enter.

The canvas is a raster surface. It fills the space between the toolbar and the status bar, and its pixel dimensions follow the Drawing window's client area. Resizing the window preserves the existing pixels from the top-left corner and clears undo/redo history for the new canvas size.

## Workflow Recipes

### Start A New Drawing

1. Open **Drawing** from Start, or press **Ctrl+N** in an existing Drawing window.
2. Select a swatch, set a custom color with **RGB**, and choose a tool and brush size.
3. Draw on the canvas. The status bar adds `[modified]` after the first canvas change.
4. Press **Ctrl+S**, keep the suggested `.modr` path or edit it, then press **Enter**.

Fill expands horizontal spans into a worklist, reducing per-pixel queue traffic on large flat regions while preserving four-connected selection.

### Edit An Existing Drawing

1. Double-click a `.modr` file in Filesystem Browser, or press **Ctrl+O** and enter its internal path.
2. Use **Pick** to sample an existing pixel when you need to continue with its color.
3. Use **Ctrl+Z** and **Ctrl+Y** while refining the canvas.
4. Press **Ctrl+S** to write changes back to the file's current path.

### Make A Variant

1. Copy the original `.modr` file in Filesystem Browser or with Terminal `cp`.
2. Open the copied path explicitly.
3. Edit and save the copy. The original remains unchanged and the new Drawing window is bound to the copy.

### Rename Or Move A Drawing

Use Filesystem Browser Rename or Terminal `mv`, keeping the complete `.modr` suffix. The open Drawing window follows the new internal path. Renaming changes the file name only; it does not convert a non-MODR file into a valid drawing.

## Keyboard-First Reference

| Intent | Shortcut | Result |
|--------|----------|--------|
| Save | **Ctrl+S** | Saves the current file, or opens the Save path prompt for a new sketch. |
| Open | **Ctrl+O** | Opens the `.modr` path prompt. |
| New | **Ctrl+N** | Starts a blank sketch; a dirty sketch requires an explicit choice. |
| Discard or confirm overwrite | **Ctrl+D** | Confirms an external-file overwrite, or explicitly discards the sketch in a dirty Close, New, or Open decision. |
| Save and continue | **Ctrl+S** | In a dirty Close or New decision, saves first and continues only after success. In dirty Open, saves the current sketch and cancels Open. |
| Save As and continue | **Ctrl+Shift+S** | During a dirty Close or New decision, choose another save path; the pending action completes only after success. |
| Undo | **Ctrl+Z** | Restores the previous canvas state. |
| Redo | **Ctrl+Y** or **Ctrl+Shift+Z** | Restores the next canvas state. |
| Complete a path | **Tab** | Completes a directory or `.modr` filename while Save or Open is active. |
| Confirm or cancel | **Enter** / **Esc** | Accepts or abandons the active Save, Open, or RGB status-bar prompt. |

The status bar is the active prompt whenever Drawing asks for a path or RGB value. Keep the pointer in the canvas for painting; keyboard shortcuts are handled by the Drawing window while no prompt is active. Starting a prompt ends an in-progress mouse stroke so its ignored button release cannot leave drawing armed. Losing focus ends the stroke for the same reason, without undoing pixels already painted.

## First Session Walkthrough

### Create And Save A Sketch

1. Open **Drawing** from Start.
2. Choose a swatch, or choose **RGB** and enter three channel values.
3. Select **Pen**, choose **S**, **M**, or **L**, and drag on the canvas.
4. Use **Line**, **Rect**, or **Fill** for shape and region work. Use **Pick** to sample a canvas pixel and continue with that color.
5. Press **Ctrl+S**. For a new sketch, Drawing opens an inline path prompt with a suggested name.
6. Press **Enter** to save, or press **Tab** while editing a path to complete a directory or `.modr` filename.

When saving, type the complete `.modr` name if you want a specific filename. Drawing never changes `.mod` into `.modr` in place; it appends `.modr` to any name that does not already end in that suffix.

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

## File Lifecycle

Drawing keeps the live canvas separate from the file path and from editor-session settings. This is the quickest way to predict what an action will do:

| Situation | Result |
|-----------|--------|
| Start Drawing or choose **New** | A blank, clean canvas is created. The file path is cleared and the normal Drawing instance title returns. |
| Paint, fill, clear, or resize a loaded sketch | The canvas becomes `[modified]`. The change exists only in memory until Save succeeds; undoing back to the last saved pixels clears the marker. **New** starts a clean blank canvas with its own undo/redo baseline. |
| Press **Save** on a new sketch | The status bar opens a path prompt with the next free `/home/monolith/drawings/sketch*.modr` name. |
| Press **Save** on a loaded sketch | The current `.modr` path is written immediately; Drawing has no separate Save As command. |
| Save fails | The current canvas and file binding stay open; a pending Close or New does not complete. A Save path prompt stays available for correction and retry. |
| Open fails | The current canvas remains open and unchanged. Correct the path or save the current sketch elsewhere. |
| Open succeeds | The canvas dimensions and pixels are replaced, the file becomes clean, and undo/redo history is cleared. Tools, brush size, and color stay as session settings. |
| Resize a loaded sketch | Pixels keep their top-left alignment, the canvas may crop or grow, history is cleared, and the file becomes modified until saved again. |
| Another app overwrites the bound `.modr` | The open canvas stays in memory and is not silently replaced. Open the currently bound path to load the external version, or Save to deliberately write the current canvas back. A dirty canvas requires **Ctrl+D** to discard before reloading. |
| Close, choose **New**, or open while modified | The status bar offers explicit choices. **Ctrl+D** discards; **Ctrl+S** saves before Close/New or saves and cancels Open; **Esc** keeps the sketch. |

There is no automatic recovery file. If the process exits before Save succeeds, unsaved pixels and in-memory undo history are lost. A successful first save claims the new `.modr` singleton before notifying other apps that the file was created.

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
- Opening an already-open `.modr` from an existing Drawing window focuses the existing owner and leaves the current canvas unchanged. Save prompts also reject a destination owned by another Drawing window instead of creating two canvases for one file.
- A missing or invalid initial `.modr` falls back to a clean blank canvas with the normal bare `Drawing` title, does not reserve a file binding, and remains eligible for session restore. Correcting the file and opening it again retries the load normally.
- Saving a bare sketch turns it into a file-backed Drawing and releases its old bare `Drawing` instance number; other bare sketches compact their titles immediately.
- When Monolith restores a saved desktop session, an open Drawing window keeps its geometry and bound `.modr` path.
- When a bound `.modr` is renamed in Filesystem Browser, moved with Filesystem Browser cut/paste, or moved with Terminal `mv`, the open Drawing window follows the normalized virtual path and updates its title, Save target, session record, singleton focus binding, and active Save/Open prompt. Moving a directory also remaps open drawings below that directory.
- If the bound `.modr` or one of its parent directories is deleted, Drawing keeps the current canvas in memory, releases the deleted file singleton, and becomes a tracked untitled `Drawing` window. Any active Save/Open prompt returns to the nearest valid parent. Use Save to choose a new `.modr` path; unsaved pixels are not discarded automatically.

All other file types continue to open in Text Editor through the shell's default routing. In particular, `.mod` is a text file, not a Drawing file.

## Closing And Session Restore

Drawing uses the same desktop lifecycle as Text Editor, including the dirty-document guard:

- Closing a clean Drawing window closes it immediately.
- Closing a modified Drawing window shows a status-bar choice. **Ctrl+D** discards and closes, **Ctrl+S** saves then closes, and **Esc** cancels. Repeating Close never discards.
- Start menu **Shut Down** and the host window close request use the same guard. Unsaved sketches are not silently discarded by either exit path.
- A failed save, failed open, or canceled prompt leaves the current canvas available. Shutdown remains blocked until every dirty Drawing is explicitly resolved.
- Starting **New** clears the file binding and restores the shell-managed instance title, such as `Drawing 2`.

Session restore records a Drawing window's geometry, minimized or maximized state, and bound virtual path. A successfully restored `.modr` file reopens in Drawing with its saved pixels and dimensions. A missing, invalid, or rejected initial path does not reserve that path, so the file can be corrected and opened again normally.

Monolith keeps one active Drawing window per normalized `.modr` path. Opening a path that is already open focuses that window; it does not create a second editor for the same file. Multiple unsaved sketches can still be open together and keep their normal instance titles.

## Toolbar

The toolbar has three rows.

Top row:

- **New**: clears the canvas, resets the file path, and restores the window's instance title (`Drawing`, `Drawing 2`, etc.).
- **Save**: writes the current bound file immediately, or prompts for a path when the sketch has no file path yet.
- **Open**: prompts for a `.modr` file path.
- **Undo**: undoes the last changed stroke, fill, or clear.
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
| Pen | Drag to paint with a filled circular brush | One undo state per changed drag |
| Eraser | Drag to paint the canvas background color | One undo state per changed drag |
| Fill | Click a connected region | One undo state per changed fill |
| Pick | Click one pixel to copy its RGB value, then return to Pen | No canvas change |
| Line | Drag from one endpoint to the other | One undo state per changed drag |
| Rect | Drag the two opposite corners | One undo state per changed drag |
| Clear | Clear the entire canvas | One undo state per changed clear; blank canvases are unchanged |

Pen and Eraser interpolate between mouse events, so fast drags remain continuous. Captured drags clamp pointer positions to the nearest canvas edge when the pointer leaves the canvas. Line and Rect commit when the mouse button is released, using that clamped endpoint. A stroke that leaves every pixel unchanged does not mark the sketch modified or consume an undo state.

## Canvas Behavior

The canvas is sized from the Drawing client area. At the base interface scale, the toolbar occupies the top 96 pixels and the status bar occupies the bottom 22 pixels; both bands grow from the active font metrics when text scaling is increased. The remaining area is the raster surface. Canvas coordinates are logical pixels, not host-window pixels.

When the window is resized:

- Existing pixels stay at their original top-left coordinates.
- A larger canvas is filled with the standard light background.
- A smaller canvas crops pixels at the right and bottom edges.
- Undo and redo history is cleared because the canvas dimensions changed.
- Resizing a file-backed sketch marks it `[modified]`; save again to persist the new dimensions.

The standard canvas background is RGB `245,245,248`. Eraser uses that same color, so it restores the background rather than revealing transparency.

## Display Scaling

Drawing follows the shared interface text scale from Settings. Changing that scale updates the toolbar and status-bar text without changing the raster itself:

- Existing canvas pixels, dimensions, file binding, dirty state, and undo history stay unchanged.
- Toolbar labels and status messages use the new interface font metrics on the next render, and their hit-test bands move with the resized chrome.
- Cached toolbar button and color-swatch hit rectangles are cleared immediately on resize or interface-scale changes, then rebuilt on demand for input or during the next render so queued events use the current control positions.
- Pointer coordinates are mapped through the displayed canvas rectangle, so strokes, shapes, fills, and color picking stay aligned with the preserved raster when the available canvas height changes.
- If Save, Open, or RGB is active, the prompt is remeasured and its cached horizontal offset is reset so the caret remains visible at the new text width.
- A scale change does not save, reload, resize, or otherwise modify the sketch.

After changing the scale, continue editing the current prompt normally. The prompt may scroll horizontally again as the caret moves through a long path.

## Colors

The eight swatches select the active pen color. Choosing a swatch also switches to Pen and disables the custom RGB color.

The built-in swatches use these exact RGB values:

| Swatch | RGB |
|--------|-----|
| Black | `20,20,24` |
| White | `245,245,248` |
| Red | `220,70,70` |
| Green | `70,180,90` |
| Blue | `70,120,220` |
| Yellow | `230,200,60` |
| Orange | `230,140,50` |
| Purple | `150,80,200` |

Select **RGB** to edit the current custom color in the status bar. Enter exactly three channel values, either as `r,g,b` or `r g b`, with every value between 0 and 255. Enter applies the color and switches back to Pen when needed. Escape cancels the prompt.

Accepted examples include:

```text
255,0,128
255 0 128
255, 0, 128
```

Hex values such as `#ff0080`, decimal fractions, and a fourth alpha channel are rejected. RGB values describe the opaque canvas color; Drawing does not save transparency.

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
| Ctrl+Z | Undo the last changed stroke, fill, or clear |
| Ctrl+Y / Ctrl+Shift+Z | Redo |
| Tab | Complete paths in save/open prompt |
| Enter | Confirm the active Save, Open, or RGB prompt |
| Esc | Cancel the active Save, Open, or RGB prompt |
| Left / Right | Move the save/open/RGB prompt caret by one UTF-8 character |
| Home / End | Move to the beginning or end of the active prompt |
| Backspace / Delete | Remove the previous or next complete UTF-8 character |

While a prompt is active, Drawing routes keyboard input to that prompt and ignores canvas actions and normal shortcuts until the prompt is finished. Printable UTF-8 characters are inserted at the caret. Tab completes a matching directory or file during Save. During Open, it completes directories and `.modr` files only. With several matches it completes the shared prefix or shows a short match preview in the status bar. Long prompts stay at native text size and scroll horizontally to keep the caret visible.

### Prompt Behavior

- **Save** starts with the current file path when one exists. A new sketch starts with the next free name under `/home/monolith/drawings/`.
- **Open** starts at `/home/monolith/drawings/` and filters file completion to `.modr` entries.
- **RGB** starts with the current active color and accepts exactly three integer channels. Tab completion does not apply to RGB input.
- These prompts are inline status-bar inputs. Left/Right/Home/End move the caret, typed text is inserted at that position, and Backspace/Delete remove complete UTF-8 characters.
- Tab replaces only the final path component before the caret. If the caret is inside a directory component, completion waits until the caret is in the final component so text after it is not rewritten.
- Completion candidates depend on the prompt: Save considers directories and existing entries, Open considers directories and `.modr` files, and RGB has no completion.
- Ambiguous completions stop at a complete UTF-8 codepoint, so filenames that share only leading bytes cannot insert an invalid partial character.
- If a bound file or directory in the active Save or Open path moves, the prompt follows the canonical path and keeps the caret at the same suffix position on a UTF-8 boundary.
- Enter accepts the active prompt and Escape cancels it. Save adds `.modr` when the entered path does not already end in `.modr`; entering `picture.mod` therefore saves as `picture.mod.modr`.
- If a dirty sketch blocks Open, the path prompt stays active. **Ctrl+D** discards and loads the current path; **Ctrl+S** saves the current sketch and cancels Open; **Esc** cancels. Changing the path requires a fresh decision.
- Recoverable Save, Open, and RGB validation or I/O failures keep the prompt active with its text, caret, and horizontal position, so the input can be corrected and retried. Escape cancels it; opening a file already owned by another Drawing focuses that window instead.

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

The name generator keeps scanning until it finds a free suffix, so a large collection of sketches does not fall back to an occupied earlier name.

Drawing always writes a `.modr` document. If you save without typing `.modr`, Drawing appends the suffix automatically. It does not replace another suffix: entering `picture.mod` creates `picture.mod.modr`.

Save creates missing parent directories for the entered virtual path before writing the document. For example, saving to `/home/monolith/drawings/concepts/rough.modr` creates `concepts/` when it does not already exist. This applies only to Save; an Open path must already name an existing valid `.modr` file.

The RGB payload is written in bounded 16 KiB chunks through the filesystem's atomic writer. Even at the 4096 x 4096 format limit, saving does not allocate a second full encoded-file buffer; a failed stream leaves the previous file unchanged.

After a successful save, Drawing copies the current pixels into its saved comparison image, reusing the existing allocation when it has enough capacity. Same-size saves avoid allocating a temporary full-canvas RGBA snapshot.

Drawing has no general **Save As** command; **Ctrl+Shift+S** opens a Save path prompt only while a dirty Close or New decision is active. Otherwise, to make a copy, copy the `.modr` file in Filesystem Browser or with Terminal, then open the copy and continue editing it. Saving an already-open sketch normally writes its current bound path.

## External File Changes

Drawing treats the canvas in the active window as the working copy. It does not reload automatically when Terminal, Filesystem Browser, Text Editor, or another filesystem operation writes the same virtual path. Instead, the status bar reports the external change while the canvas remains stable, preventing a background file operation from replacing unsaved pixels.

Use the action that matches your intent:

| Situation | Action |
|-----------|--------|
| You want to keep the canvas currently visible | Press **Ctrl+S**. If the file changed externally, confirm with **Ctrl+D**; **Esc** keeps the disk version. |
| You want to inspect the version written by another app | Press **Ctrl+O**, select the same `.modr`, then press **Ctrl+D** if the canvas is modified. |
| You want both versions | Copy the file to a new `.modr` path first, then open the copy or save the current canvas to another path. |

An external overwrite does not change the Drawing title, bound path, dirty marker, or undo history. An `[external change]` marker remains visible through other status updates until the file is reloaded, successfully saved, removed, or replaced with a new sketch. Before saving to the bound path, Drawing compares the current file stamp with its last loaded/saved version; a changed stamp triggers exact comparison of the serialized `.modr` bytes, detecting host edits and other Monolith processes without a change notification. Unchanged stamps avoid rereading the canvas file. A normal Ctrl+S pauses before replacing a different or missing disk version; press **Ctrl+D** to confirm the overwrite, or **Esc** to keep the external version. The comparison is streamed without a second encoded-file buffer and the confirmation applies only to that save attempt. Opening the currently bound path reloads the external version in the same window; if the canvas is dirty, the existing dirty-content decision uses **Ctrl+D** to discard local pixels and reload. A successful Save still sends the normal filesystem change notification so other open apps can refresh their views. Host writers are not locked and can still race after comparison.

Bound-path saves check the expected file stamp again immediately before replacement. A write arriving while the `.modr` stream is generated is preserved and requires another **Ctrl+D** confirmation. An absent expected target uses no-replace rename, so a concurrent create is preserved; existing targets retain a final stamp-check-to-rename race.

## Common File Workflows

Use this sequence when managing a saved sketch outside the canvas:

| Goal | Filesystem Browser | Terminal equivalent |
|------|--------------------|---------------------|
| Copy a sketch | Select the `.modr` file, choose **Copy**, open the destination folder, then choose **Paste**. | `cp /home/monolith/drawings/sketch.modr /home/monolith/drawings/sketch-copy.modr` |
| Rename a sketch | Select the file, choose **Rename**, and keep or type the full `.modr` suffix. | `mv /home/monolith/drawings/draft.mod /home/monolith/drawings/draft.modr` |
| Open a sketch | Double-click a valid `.modr` file. | `open /home/monolith/drawings/sketch-copy.modr` |
| Move a sketch | Cut the file, navigate to the destination, then paste it. | `mv /home/monolith/drawings/sketch.modr /home/monolith/archive/sketch.modr` |

The copy and move destinations must not already contain an entry with the same name. `cp` and `mv` use the destination exactly as entered, so neither command adds `.modr`. When a bound sketch is moved or renamed, its open Drawing window follows the new canonical path. When it is copied, the copy is a separate file and must be opened explicitly.

The short version is:

```text
# Duplicate a valid drawing, then open the duplicate.
cp /home/monolith/drawings/sketch.modr /home/monolith/drawings/sketch-copy.modr
open /home/monolith/drawings/sketch-copy.modr

# Rename a valid drawing without changing its contents.
mv /home/monolith/drawings/draft.mod /home/monolith/drawings/draft.modr
open /home/monolith/drawings/draft.modr
```

### Choose The Right Operation

Use the operation that matches the result you want:

| Goal | Use | Why |
|------|-----|-----|
| Continue editing the current sketch | **Save** | Writes the current canvas to its existing bound path. |
| Start another sketch from the same pixels | **Copy**, then **Open** the copy | Preserves the original and creates a separate file binding. |
| Change the sketch's filename | **Rename** or `mv` | Changes the virtual path; it does not rewrite or convert the file payload. |
| Put the sketch in another folder | **Cut/Paste** or `mv` | Moves the same file and updates an open Drawing window to the new path. |
| Turn a blank canvas into a Drawing file | **Save** with a `.modr` name | Creates a valid MODR header, dimensions, and RGB payload. |

Do not use rename to convert an unknown file into a Drawing. Changing `image.mod` to `image.modr` only changes its name. Use Drawing Save to create a valid document, or open the renamed file only when its contents are already a valid MODR raster.

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

Drawing reads `.modr` data in bounded filesystem chunks and decodes RGB directly into the new RGBA canvas. It does not first buffer the full encoded file or allocate a blank canvas that would immediately be replaced; the current sketch remains intact until the complete payload validates.

## Undo And Redo

Drawing stores a capped history of canvas edits.

- Changed strokes capture the original pixels of touched 32×32 tiles, only when the first pixel in each tile is about to change.
- Fill captures original pixels from only the 32×32 tiles it changes. Clear captures only tiles containing pixels to remove, so untouched background tiles do not consume undo history.
- Undo and redo swap saved tile preimages back onto the canvas.
- Starting a changed stroke or clearing after an undo resets redo history.
- Picking a color does not create a history state.
- Opening a file clears history.
- Resizing the canvas clears history so old edits are not applied to the wrong canvas size.

Undo and redo share a combined limit of 32 history states and 64 MiB of stored pixel data. The oldest undo states are discarded first when either limit is reached. Sparse strokes, localized fills, and clears of sparse artwork remain undoable on canvases larger than 64 MiB when their changed tiles fit the budget. If an operation exceeds the budget, Drawing immediately releases its pending tile captures and clears undo/redo rather than growing memory without bound. Exact preimage-byte totals are stored per history entry and per stack, so edits and eviction do not rescan retained tile vectors. Clear scans the canvas by 32×32 tiles, recording and resetting only tiles that contain non-background pixels. Tile preimages swap their pixel data in place for undo and redo; compact dirty and capture scratch maps are reused between edits and reset through touched-tile indices, avoiding a full scratch-map clear for each stroke, fill, or clear. The dirty map keeps the modified marker accurate after saves and undo/redo by rechecking only tiles changed by each sparse edit.

## File Format

`.modr` is a simple binary raster format:

- Magic bytes: `MODR`
- Width: 32-bit little-endian integer
- Height: 32-bit little-endian integer
- Pixel payload: RGB bytes, top-to-bottom and left-to-right

Width and height must be between 1 and 4096 pixels. The encoder and decoder enforce the same limits. The decoder requires the file to contain exactly the header plus `width * height * 3` payload bytes, and the encoder requires an exact `width * height * 4` RGBA buffer. A live canvas can exceed the file-format limit on an unusually large client area; Save reports that limit before creating a target directory or writing a file. Resize the canvas to supported dimensions before saving.

The streaming encoder uses a fixed 16 KiB scratch buffer, so the file payload is not first assembled as another canvas-sized string before the atomic write.

Internally, the live canvas stores pixels as `R,G,B,A`. The saved file stores only RGB because the canvas is fully opaque.

The format has no metadata for tools, brush size, custom color, undo history, or layers. Those are editor state and are not restored when the file is reopened.

The format is intentionally small and strict. A file with a wrong magic header, dimensions outside the supported range, truncated payload, or trailing bytes is rejected instead of partially opening.

## Unsaved Changes

A dirty sketch (status bar `[modified]`) blocks destructive actions until you choose:

| Action | Explicit choices |
|--------|------------------|
| Close window (X / shell close) | **Ctrl+D** discards and closes; **Ctrl+S** saves then closes; **Esc** cancels and keeps the sketch. |
| New / Ctrl+N | **Ctrl+D** discards and starts a blank sketch; **Ctrl+S** saves then starts New; **Esc** cancels and keeps the sketch. |
| Open path | Press **Enter** to validate the path and show the dirty decision. **Ctrl+D** discards and opens that path; **Ctrl+S** saves the current sketch and cancels Open; **Esc** cancels and keeps the sketch. |

Clear (toolbar) remains undoable and does not use this guard.

Repeating Close, New, or Enter does not discard the sketch. For Close or New on an untitled sketch, **Ctrl+S** opens Save As; use **Ctrl+Shift+S** to choose another path for a saved sketch. The pending action completes only after the save succeeds. A failed save keeps the sketch and does not close or start New. For Open, saving first deliberately cancels the current Open prompt; start Open again after saving if you still want to switch files. The same choices apply when reopening the currently bound path to reload an external change.

## Status Bar Messages

The status bar is both the command hint area and the app's lightweight feedback channel. Common messages mean:

| Message pattern | Meaning / next action |
|-----------------|-----------------------|
| `Pen ready...` or `Tool: ...` | The selected tool is ready for the next canvas action. |
| `Save as (...)` | A Save path prompt is active. Edit the path, then press Enter or Escape. |
| `Open path (...)` | An Open path prompt is active. Tab-complete a directory or `.modr` file, then press Enter. |
| `Custom RGB ...` | The RGB prompt is active. Enter three channels from 0 through 255. |
| `Unsaved changes ... Ctrl+D ...` | A destructive action is waiting for a choice. Use the displayed **Ctrl+D**, **Ctrl+S**, or **Esc** action; repeating the original action never confirms discard. |
| `Path completed...` | Tab found a completion. Review the path before confirming it. |
| `No path matches.` | Tab found no eligible directory or file at the caret. Keep editing the path. |
| `Saved: ...` / `Opened: ...` | The operation completed and includes the normalized internal path. |
| `Save failed: ...` / `Open failed: ...` | The operation was rejected and the current canvas remains. A path prompt stays active when its input can be corrected or saved elsewhere. |
| `RGB failed: ...` | The color was not changed. Enter exactly three integer channels in the accepted range. |
| `New sketch.` or `Cancelled.` | The requested reset or prompt cancellation completed. |

When the canvas has unsaved edits, `[modified]` is appended to the status bar. Dirty Close and New offer explicit discard, save-and-continue, and cancel choices. Dirty Open stays in its path prompt until you explicitly discard, save first (which cancels Open), or cancel. Changing the Open path requires a fresh decision. Failed saves and opens keep the current canvas; a failed save never completes a pending Close or New.

## Troubleshooting

| Symptom | Cause and fix |
|---------|---------------|
| `Open failed: Drawing files must use .modr.` | Open accepts only `.modr` files, with case-insensitive suffix matching. A `.mod` file is text and belongs in Text Editor. |
| Saving `picture.mod` creates `picture.mod.modr` | Save appends `.modr` when the entered path does not already end in that suffix. Enter the intended `.modr` name explicitly. |
| `No path matches.` after pressing Tab | Completion compares the typed filename prefix exactly. Move the caret to the final path component, correct the prefix, and press Tab again. |
| `Open failed: could not read file.` | The virtual path is missing or could not be read. The current canvas remains open; correct the path or save the current sketch elsewhere. |
| `Open failed: .modr file exceeds the size limit.` | The file is larger than the maximum valid `.modr` payload. Drawing rejects it before buffering the contents; the current canvas and Open prompt remain available. |
| `Open failed: not a valid .modr drawing file.` | The file header, dimensions, or pixel payload is invalid. Drawing does not partially load corrupt data. |
| The status bar shows `[modified]` | The canvas has edits that are not saved. Close, New, and Open will offer explicit choices; **Ctrl+S** saves without discarding. |
| Undo is no longer available after resizing | Resizing changes the canvas dimensions, so Drawing clears history rather than applying edits to a different-sized canvas. |

## Current Limitations

- Custom RGB can be entered through the status-bar `r,g,b` prompt or sampled with Pick; there is no palette editor yet.
- The editor is raster-only: there are no layers, selections, transforms, zoom controls, or vector objects.
- No clipboard import/export yet.
- Dirty decisions use status-bar keyboard choices rather than a modal dialog.
- Undo history is in memory only, shares a 64 MiB budget across undo and redo, and resets when a drawing file is opened, the canvas is resized, or the app exits.

## Developer Notes

Main implementation files:

- `src/app/DrawingApp.hpp`
- `src/app/DrawingApp.cpp`
- `src/app/DrawingRaster.hpp` / `DrawingRaster.cpp` - line/rect raster, scanline fill, pixel reads, custom RGB parse, `.modr` encode/decode (shared with headless tests)
- `src/window/detail/wm_body_07.inc` — `launchDrawing()` and Drawing window creation
- `src/window/detail/wm_body_01.inc` / `wm_body_08.inc` — mouse-up forwarding, open routing, and session restore
- `src/app/App.hpp` — `IWindowController::restoreTrackedInstanceTitle()`, `allowClose` for dirty guards

Canvas GPU path (`syncTexture`): recreate the streaming texture only when missing or size-changed. Paint and fill accumulate their changed pixel/span bounds, while undo and redo accumulate restored tile bounds; only that rectangle is uploaded. Resize, load, and Clear invalidate the full texture.

Drawing toolbar labels and status or prompt text use a renderer-owned `TextTextureCache`, bounded to 256 entries and an estimated 16 MiB. Text, color, and font form each key, so unchanged labels survive status and prompt updates rather than rebuilding a renderer texture every frame. The active prompt also reuses its measured caret-prefix width until the prefix or shared font changes. The text cache clears when the renderer changes, and shared interface-scale changes also invalidate the caret measurement. It remains separate from the canvas texture's bounded dirty-region upload lifecycle above.

Drawing status-bar text uses a narrower internal clip. It intersects that clip with the caller's renderer clip and restores the caller clip after the status bar is drawn, so embedded rendering cannot leak into neighboring shell regions.

The Drawing implementation has three boundaries worth preserving when changing it:

1. `DrawingRaster` owns format, fill, and pixel rules that can be tested without SDL.
2. `DrawingApp` owns the live canvas, prompts, dirty state, and editor-session settings.
3. `WindowManager` owns file singleton routing, instance titles, session restore, and desktop close guards.

All Drawing file paths should be normalized before they are stored in app state or passed to WindowManager. This keeps titles, prompts, session records, and one-window-per-file routing aligned when a path contains redundant separators or `.` and `..` segments.

Two path invariants are especially important:

- New Save prompts scan from `sketch.modr` through increasing numbered names until they find a free path. The generator must not wrap back to an earlier name after a fixed number of candidates.
- When a bound file or directory moves, the active prompt path is remapped with the file path. Its caret is then clamped to a complete UTF-8 codepoint boundary so editing cannot split a multi-byte character.

Changes that move behavior across those boundaries should update this guide and the matching focused state or raster check. The `.modr` format should remain strict: invalid magic, dimensions outside the supported range, truncated payloads, and trailing bytes must fail without replacing the current canvas.

Verification scripts: see [Development Scripts](../development/scripts.md). The focused Drawing checks are:

```bash
./scripts/verify_drawing_integration.sh
g++ -std=c++23 scripts/test_drawing_roadmap.cpp src/app/DrawingRaster.cpp -o build/test_drawing_roadmap && ./build/test_drawing_roadmap
g++ -std=c++23 scripts/test_drawing_state.cpp src/app/DrawingApp.cpp src/app/DrawingRaster.cpp src/fs/Filesystem.cpp $(pkg-config --cflags --libs sdl2 SDL2_ttf) -o build/test_drawing_state && ./build/test_drawing_state
g++ -std=c++23 scripts/test_modr_format.cpp src/app/DrawingRaster.cpp -o build/test_modr_format && ./build/test_modr_format
```

These checks cover shell wiring, the production `.modr` encoder/decoder, and
stateful editor paths without requiring an interactive desktop session. The
optional smoke script exercises the built application through the headless
display path using a unique temporary HOME, verifies the application's actual
filesystem root, and checks a uniquely named MODR fixture round-trip without
clearing shared paths:

```bash
./scripts/headless_drawing_smoke.sh
```

For a complete repository check that also rebuilds generated sources and runs
the other app and shell tests, use:

```bash
./scripts/run_headless_tests.sh
```

The full runner uses SDL's dummy video and audio drivers by default, so it is
safe to run from a non-interactive terminal. Use the focused commands above
when iterating on Drawing, then run the complete suite before submitting a
cross-app or shell change.

Keep this guide and [Development Scripts](../development/scripts.md) aligned
when a Drawing shortcut, prompt rule, file-format invariant, or shell-routing
behavior changes.
