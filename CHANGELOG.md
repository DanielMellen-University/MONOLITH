# Changelog

## 2026-09: Drawing app user guide

- Added a first-session walkthrough, reopen workflow, prompt troubleshooting table, and focused contributor verification commands to the Drawing documentation.

## 2026-09: Complete multi-document shutdown confirmation

- Shut Down now arms every dirty open document in the first request, so confirming once handles multiple Editor or Drawing windows together.

## 2026-09: Explicit filesystem read results

- Added a boolean-output `Filesystem::readFile` overload so empty files and read failures are distinguishable.
- Text Editor, Drawing, Terminal `cat`, and Terminal history loading now use the explicit read result where it matters.

## 2026-09: Verified Terminal file copies

- Terminal `cp` now routes regular files through `Filesystem::copyRecursive`, so a failed source read cannot silently create an empty destination.

## 2026-09: Drawing app documentation

- Added a consolidated Drawing guide with the save contract, `.modr` versus `.mod` routing, persisted canvas state, prompt controls, and recovery behavior.

## 2026-09: Scaled Alt+Tab title overlay

- Alt+Tab now sizes long-title overlays in logical coordinates and clips the native-size label inside the overlay at non-1x content scales.

## 2026-09: Guarded Shut Down

- Start menu Shut Down now honors each open app's dirty-document close guard before exiting, so unsaved Editor and Drawing work requires the same second confirmation as closing a window.

## 2026-09: Bounded Snake and Pong HUD text

- Snake and Pong HUD labels now render at native size and clip inside their HUD strips instead of overflowing narrow game windows.

## 2026-09: Bounded Minesweeper HUD text

- Minesweeper status, difficulty labels, and footer text now stay at native size and clip within their controls instead of covering the face button or extending past the window.

## 2026-09: Native-size Settings text

- Settings information and footer text now render at native size and clip at their panel boundaries instead of being horizontally squeezed.

## 2026-09: Native-size window titles

- Long window titles now render at native text size and clip before the title-bar controls instead of being horizontally squeezed.

## 2026-09: Drawing status and recovery reference

- Added a concise Drawing status-bar guide covering prompts, Tab completion, failed operations, invalid RGB input, and unsaved-change recovery.

## 2026-09: Native-size Filesystem chrome

- Filesystem Browser paths and status messages now stay at native text size and clip within their own regions instead of being horizontally squeezed.

## 2026-09: Clamp restored windows before focus

- Reopening a minimized Editor or Drawing window now re-applies desktop geometry clamping before showing it, keeping stale session rectangles above the taskbar and inside the desktop bounds.

## 2026-09: Drawing app reference guide

- Expanded the Drawing documentation with an at-a-glance reference, status-bar prompt model, exact `.modr` save behavior, and the distinction between persisted pixels and transient editor state.

## 2026-09: Predictable Filesystem listing order

- Filesystem Browser listings now sort names case-insensitively, with directories still grouped before files and raw names used as a deterministic tie-break.

## 2026-09: Verified Filesystem file copies

- Recursive file copies now verify the source size and complete read before writing the destination, preventing read failures from becoming empty files.
- Filesystem documentation now reflects Terminal quoting and all persisted desktop settings.

## 2026-09: Native-size Text Editor status prompts

- Long Find, Replace, Open, Save, and Go-to-line prompts now clip at native text size and keep the active caret visible.

## 2026-09: Forward Delete in Terminal input

- Terminal command input now removes the next complete UTF-8 character with Delete, matching the editor cursor model.

## 2026-09: Caret-aware Text Editor find and replace

- Find and Replace fields now support caret movement, insertion, Delete, and UTF-8-safe Backspace.
- Long search prompts keep the active caret visible without compressing the status bar.

## 2026-09: Caret-aware Terminal history search

- Reverse Ctrl+R search now supports caret movement, insertion, Delete, and UTF-8-safe Backspace.
- Long search queries keep the active caret visible in the input strip.

## 2026-09: Caret-aware Filesystem filtering

- Filesystem Browser Ctrl+F filtering now supports caret movement, insertion, Delete, and UTF-8-safe Backspace.
- Long filter queries stay at native size and keep the active caret visible.

## 2026-09: Caret-aware Drawing prompts

- Drawing Save, Open, and RGB prompts now support caret movement, insertion, Delete, and UTF-8-safe Backspace.
- Long prompts keep the active caret visible without compressing the status-bar text.

## 2026-09: Caret-aware Settings wallpaper paths

- Settings wallpaper path editing now supports caret movement, insertion, Delete, and UTF-8-safe Backspace.
- Long wallpaper paths keep the active caret visible inside the field.

## 2026-09: Caret-aware Text Editor prompts

- Text Editor Save, Open, and Go-to-line prompts now support cursor movement, insertion, Delete, and UTF-8-safe Backspace.
- Path completion edits the final component at the caret without rewriting text after it.

## 2026-09: Better Filesystem Browser rename editing

- Rename mode now supports caret navigation, insertion, Delete, and UTF-8-safe Backspace instead of editing only at the end of the name.
- Long names scroll to keep the active caret visible while renaming.

## 2026-09: Quoted Terminal path completion

- Terminal Tab completion now follows the command lexer for quoted paths and backslash-escaped spaces.
- Completions preserve opening quotes and escape syntax characters when inserted into unquoted input.

## 2026-09: Drawing documentation reference

- Documented Drawing shell routing, minimized-window reuse, session restore, and the distinct behaviors of Save, Open, and RGB prompts.

## 2026-09: Safe Terminal input reset

- Clearing Terminal input with Esc now resets the cursor and saved history state, preventing the next typed character from targeting a stale buffer offset.

## 2026-09: Restore focused file windows

- Reopening an already-open Editor or Drawing file now restores its minimized window before focusing it.

## 2026-09: Reliable minimize focus

- Minimizing the active window now hands keyboard focus to the topmost visible survivor and prevents hidden apps from receiving key events.

## 2026-09: Bounded taskbar scrolling

- Taskbar wheel and arrow scrolling now clamps to the actual window-button strip, preventing all buttons from disappearing after excessive scrolling.

## 2026-09: Native-size taskbar labels

- Taskbar window titles now stay at native size and clip within their own buttons instead of being horizontally squeezed or bleeding into neighbors.

## 2026-09: Native-size text clipping

- Terminal scrollback and Text Editor syntax spans now render at native size and clip at the viewport edge instead of being horizontally squeezed.

## 2026-09: Filesystem long-name rendering

- Filesystem list names now clip at native text size instead of being horizontally squeezed, and long rename buffers keep the caret visible.

## 2026-09: Filesystem context-menu selection

- Filesystem context menus now preserve an existing multi-selection when opened on a selected row and clear stale selection when opened on empty space.

## 2026-09: Terminal long-line editing

- Terminal input now clips to its input bar and follows the cursor horizontally through long commands.

## 2026-09: Retryable partial cut paste

- Filesystem Browser keeps a shared cut clipboard when only part of a multi-item paste succeeds, so conflicting sources can be retried after the destination is fixed.

## 2026-09: Fresh mouse position after release

- WindowManager mouse-up events now update the stored pointer position before later wheel routing.

## 2026-09: Unicode-safe Drawing paths

- Drawing save/open prompts now accept UTF-8 path text and remove complete characters with Backspace.

## 2026-09: Unicode-safe Settings paths

- Settings wallpaper path editing now removes complete UTF-8 characters with Backspace.

## 2026-09: Unicode-safe Terminal editing

- Terminal Left/Right and Backspace now move and erase complete UTF-8 characters instead of raw bytes.
- Reverse history search Backspace uses the same codepoint-safe behavior.

## 2026-09: Reliable client mouse release

- Client apps now retain left-button motion and release routing for an active drag, even when the pointer leaves the window or focus changes.
- Closing a captured window clears the capture safely.

## 2026-09: Scale-aware window input

- Window hit testing, title-bar interaction, dragging, resizing, and client mouse coordinates now convert screen pixels to logical desktop pixels consistently.
- Direct clicks no longer depend on a previous mouse-motion event to establish the correct Y coordinate.

## 2026-09: Drawing app documentation

- Added a complete Drawing workflow guide covering tool behavior, canvas resizing, undo history, `.modr` state, path prompts, and current raster-editor limitations.

## 2026-09: Unicode-safe browser prompts

- Filesystem Browser rename and filter Backspace now removes a complete UTF-8 codepoint instead of one raw byte.
- UTF-8 stepping logic is shared with the Text Editor and covered by a headless regression check.

## 2026-09: Safe multi-item cut/paste

- Filesystem Browser cut/paste now moves sources through a shared non-overwriting rename operation.
- When one destination name conflicts during a multi-item paste, only successful moves complete; conflicting originals are preserved.

## 2026-09: Consistent Drawing extension routing

- `.modr` matching is now case-insensitive across shell open-with routing, Drawing save/open, tab completion, Terminal status text, and Filesystem Browser default open.
- `.mod` remains a Text Editor file and is not treated as a Drawing document.

## 2026-09: Session restore paths

- Session files now quote editor and Drawing paths, preserving virtual filenames with spaces, quotes, or backslashes across restarts.
- Older session files with unquoted path tokens remain readable.

## 2026-09: Filesystem empty-file handling

- Reading zero-byte virtual files is now handled without indexing an empty buffer.
- The filesystem roadmap check covers reading, sizing, and copying empty files.

## 2026-09: Drawing app documentation

- Expanded the Drawing guide with a quick-start workflow, canvas resizing behavior, brush sizes, custom RGB input, path prompt editing, failure handling, and `.modr` validation limits.
- Corrected the unsaved-state indicator to match the `[modified]` status shown by the app.

## 2026-09: Shared Filesystem clipboard

- Copy and cut state is now owned by the desktop shell and shared across Filesystem Browser windows.
- Paste still operates on virtual paths only and does not connect to the host OS clipboard.

## 2026-09: Drawing eyedropper

- The Drawing toolbar now includes Pick, which samples a canvas pixel into the existing custom RGB color and returns to Pen.
- Sampling does not modify pixels or add an undo state, and sampled colors continue to save normally in `.modr` files.

## 2026-09: Text Editor horizontal scrolling

- Long lines now remain fully editable instead of clipping at the right edge.
- The cursor auto-scrolls horizontally while moving or typing; Shift + mouse wheel can pan the text viewport manually.
- Selection, find highlights, syntax colors, and mouse hit testing follow the horizontal viewport.

## 2026-09: Drawing app guide

- Documented `.modr` open routing, including the fact that `.mod` remains a text file.
- Updated Drawing developer references for the split WindowManager source layout.

## 2026-09: Settings interface text scale

- Settings now offers Small (90%), Default (100%), and Large (115%) interface text sizes.
- The shared font updates existing windows immediately, including title bars and the taskbar clock.
- The selected size persists in `~/.monolith/desktop_settings.txt` (`ui_scale_percent=`); older settings files keep the 100% default.

## 2026-09: Terminal quoted arguments

- Command lines accept double and single quotes so paths with spaces work (`cat "/home/monolith/my file.txt"`).
- Backslash escapes work outside quotes and for `\` / `"` inside double quotes.
- Unterminated quotes report a parse error instead of running a half-split command.
- Quoting rules live in `docs/apps/terminal.md`; lexer is `TerminalLexer` with a headless test.

## 2026-09: Pong under Start → Games

- Third native game: **Pong** (player vs AI, first to 5).
- Arrow keys / WASD move the paddle; Space pauses; R restarts after a win or loss.

## 2026-09: Drawing line, rectangle, and custom RGB

- Line and Rect tools paint 1px strokes on the canvas (drag from press to release).
- RGB toolbar control accepts a custom `r,g,b` color beyond the swatch set.
- Line/rect/color and `.modr` encode/decode live in `DrawingRaster` so save/load still round-trips those pixels.

## 2026-09: Filesystem Browser multi-copy, filter, rename

- Copy/Cut apply to the whole multi-selection; Paste writes those items into the current folder (`Filesystem::copyItemsInto`).
- Folder listing can be filtered by name (Ctrl+F / Filter box, case-insensitive substring).
- Rename rejects names that contain `/` (and empty / `.` / `..`) via `Filesystem::renameEntry`.

## 2026-09: Desktop wallpaper images (BMP)

- Settings → Appearance: set a wallpaper image by virtual FS path (text field + Set / Clear).
- Empty path keeps the solid color background; presets still apply underneath the image.
- Wallpaper path persists in `~/.monolith/desktop_settings.txt` (`wallpaper_path=`).
- Shell loads BMP via `SDL_LoadBMP` (no SDL_image); cover-scales over the desktop; fails soft on missing/bad files.
- Sample image seeded at `/Wallpapers/sample.bmp` from `assets/wallpapers/sample.bmp`.

## 2026-09: Taskbar clock 12/24-hour toggle

- Settings → Appearance: choose **12-hour** (default) or **24-hour** for the taskbar clock.
- Choice persists in `~/.monolith/desktop_settings.txt` (`clock_24_hour=0|1`) with older files still loading as 12-hour.
- Taskbar clock text updates immediately when the format changes.

## 2026-09: Taskbar clock

- Local-time clock on the right of the taskbar (12-hour by default, updates each minute).
- Hover the clock to see the full local date.

## 2026-07: Alt+Tab switcher and Start hotkey

- **Alt+Tab** / **Alt+Shift+Tab** cycles windows (restores minimized; overlay until Alt up).
- **Ctrl+Escape** toggles the Start menu.

## 2026-07: Editor go-to-line and coalesced undo

- **Ctrl+G** opens a status-bar line-number prompt (Enter jumps, Esc cancels).
- Consecutive typing or in-line backspace within ~1s shares one undo snapshot.

## Current (2026-07)

Focused snapshot of Monolith as of the latest `beta` / `main` tip. Older detail lives in git history.

### Desktop shell
- Overlapping windows: drag, 8-way resize, minimize / maximize, z-order, focus-after-close
- Taskbar with Start menu (**Games** category), scroll arrows + wheel when crowded, local-time clock with 12/24-hour setting (hover for date)
- Multi-instance titles with live compaction (`Terminal`, `Terminal 2`, …)
- Session restore: `~/.monolith/session.txt` on exit; restored on next launch (demo set if missing)
- Open-with routing: `.modr` → Drawing, else Text Editor (`openPath`)
- Desktop background color presets and BMP wallpaper path via Settings (`~/.monolith/desktop_settings.txt`)
- Controllers owned per window; `App::allowClose` for dirty-document guards

### Filesystem
- Virtual FS under `~/.monolith/fs/` with shared recursive copy/remove, path helpers, `fileSize`
- Terminal: shell commands, capped scrollback/history, safer `touch`, `open` uses shell routing
- Filesystem Browser: multi-select, properties, recursive delete with confirm, open-with menu, cut/copy/paste

### Productivity apps
- **Text Editor**: UTF-8, selection + clipboard, syntax highlight, find (**Ctrl+F**) and replace (**Ctrl+H**), dirty close/open
- **Drawing**: pen/eraser/fill, `.modr` save/load, undo, dirty close/new/open, efficient texture upload
- **Settings**: appearance swatches, BMP wallpaper path, taskbar clock 12/24-hour toggle, about / environment panel

### Games
- **Snake**, **Minesweeper**, and **Pong** under Start → Games (high scores / best times on host; Minesweeper pauses timer when unfocused)

### Build
- C++23, CMake, SDL2 + SDL2_ttf; see README for package names

Historical feature-by-feature entries were cleared to keep this file short. Use `git log` for full history.
