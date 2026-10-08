# Rendering internals

How frames are drawn and how the apps keep per-frame work and memory bounded. For the overview, see [architecture.md](../architecture.md#rendering).

## Frame composition

- Everything renders into one SDL window through SDL's accelerated renderer with VSync.
- `WindowManager::render` draws the wallpaper, desktop icons, windows back to front, the Start menu, the taskbar and the Alt+Tab overlay.
- The whole frame is clipped to the caller's clip. Each app is additionally clipped to its client rectangle, so a tiny or misbehaving app cannot paint over title bars or the taskbar.
- Before each app callback the shell sets neutral blend mode and draw color, and restores both afterwards. At the end of the frame it restores the caller's original blend mode and color.
- The frame iterates over a live snapshot of windows, so an app that closes or replaces its window inside `render()` does not break the loop. New windows appear on the next frame.

## Clip rule

Any code that sets a narrower clip must intersect it with the current clip and restore the previous clip before returning. Code that draws translucent overlays restores the previous blend mode too. `src/detail/RendererClip.hpp` provides capture, intersect and restore helpers used by the shell, every app, the Filesystem context menus, title bars, taskbar buttons and Alt+Tab.

## Text texture caches

Text is rasterized once with SDL_ttf and reused as a texture.

- `src/detail/TextTextureCache.hpp` is an LRU keyed by font, text and color. It holds at most 256 entries and an estimated 16 MiB. The estimate counts pixels plus text storage and bookkeeping.
- Lookups take a borrowed text view and only build an owned key on a miss. LRU nodes point at the map keys, so each string is stored once.
- On a miss the text is measured first. A single texture larger than the budget is refused, so the cache never exceeds its bound.
- Each cache is tied to one renderer. A renderer change or a text-size change clears it.
- Users: the shell (window titles, taskbar, Start menu, Alt+Tab), Terminal, Text Editor, Filesystem, Drawing, Settings, Snake, Minesweeper, Pong and Breakout, each with its own instance.

Shell specifics:

- Taskbar and title labels stay at native size and use the UTF-8 SDL_ttf API.
- A window title texture holds only the complete characters that fit, so a long file name does not create a full-width surface. Focus changes only swap the color variant.
- The Alt+Tab overlay measures in screen pixels, converts to logical width, and reuses the shell cache for its label.

## Per-app work

### Terminal

- Only the visible part of each scrollback row is rasterized. Visible byte ranges are cached per row, viewport width and horizontal offset, and passed to the cache as borrowed views.
- Row widths are measured lazily, only when horizontal panning needs them.
- The abbreviated prompt path is cached until the directory changes. Input, selection and search strings reuse their buffers.
- A warmed frame with clipped scrollback rows makes no C++ heap allocations; a test checks this.

### Text Editor

- Lexical spans are cached for the visible rows, with block-comment state carried forward. Edits invalidate state from the edited row down.
- Scrolling between overlapping viewports of the same size shifts the cached spans and only tokenizes new rows.
- Only spans that intersect the horizontal viewport are rasterized, passed as borrowed views.
- Status, Find and prompt text are built in retained buffers. Find counters use stack formatting.
- Find highlight geometry is cached for visible rows until the query, viewport or font changes. Rebuilds measure in a 4 KiB stack buffer; only unusually long slices use the heap.
- Mouse hit testing maps a pixel to a byte column with one `TTF_MeasureUTF8` pass.
- Cursor and selection width checks use a retained 4 KiB prefix buffer; longer prefixes use temporary storage.
- Find keeps one location checkpoint per 256 non-overlapping matches instead of every hit, and navigation and viewport scans start from the nearest checkpoint.
- Undo snapshots move line buffers between the document and the history stacks. The 64 MiB history budget is an estimate of line vector plus string storage.
- Multi-line paste inserts all new rows at once instead of shifting the rest of the document per line.
- Tests check that warmed frames and isolated highlight rebuilds make no heap allocations for ordinary text.

### Filesystem

- Row clipping uses the cached filename texture widths.
- The status bar reuses one string and appends counts, the selected name and feedback directly.
- Filter and rename caret positions reuse their measured prefix width until the prefix or font changes.
- Filtering reuses the folder snapshot and stores matches as indexes into it. Adding characters at the end narrows the existing matches in place; other edits and refreshes rebuild from the snapshot. Name comparison is case-insensitive without building lowercase copies.
- Toolbar and filter hit rectangles are cached and cleared on resize or text-size change.

### Drawing

- The canvas is a streaming texture, recreated only when its size changes. Edits record the bounding box of changed pixels (or restored undo tiles) and only that rectangle is uploaded. Resize, load and Clear upload the whole canvas.
- Raster helpers in `DrawingRaster.cpp` validate the RGBA buffer once per call, share one Bresenham walk for brush strokes, clip rectangles to the canvas, and fill with horizontal spans (four-connected).
- Status and prompt text are assembled from slices of the path buffer into retained strings, so a warmed prompt frame does not allocate.

### Games

- Snake caches its 200 alternating checkerboard rectangles until the board size changes and draws them in one batch. Its game-over score line is formatted in a stack buffer.
- Minesweeper formats its live status and win-overlay lines in stack buffers.
- Breakout caches brick rectangles until the playfield changes and draws live bricks in at most five batches, one per row color, instead of up to 50 calls.
- Pong and Breakout convert SDL's wrapping 32-bit tick count with `detail::tickDeltaSeconds()` (`src/detail/TickMath.hpp`), which caps a stalled frame at 50 ms.
