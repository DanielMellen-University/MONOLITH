# Changelog

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
- Desktop background color presets via Settings (`~/.monolith/desktop_settings.txt`)
- Controllers owned per window; `App::allowClose` for dirty-document guards

### Filesystem
- Virtual FS under `~/.monolith/fs/` with shared recursive copy/remove, path helpers, `fileSize`
- Terminal: shell commands, capped scrollback/history, safer `touch`, `open` uses shell routing
- Filesystem Browser: multi-select, properties, recursive delete with confirm, open-with menu, cut/copy/paste

### Productivity apps
- **Text Editor**: UTF-8, selection + clipboard, syntax highlight, find (**Ctrl+F**) and replace (**Ctrl+H**), dirty close/open
- **Drawing**: pen/eraser/fill, `.modr` save/load, undo, dirty close/new/open, efficient texture upload
- **Settings**: appearance swatches, taskbar clock 12/24-hour toggle, about / environment panel

### Games
- **Snake** and **Minesweeper** under Start → Games (high scores / best times on host; Minesweeper pauses timer when unfocused)

### Build
- C++23, CMake, SDL2 + SDL2_ttf; see README for package names

Historical feature-by-feature entries were cleared to keep this file short. Use `git log` for full history.
