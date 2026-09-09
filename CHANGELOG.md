# Changelog

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
