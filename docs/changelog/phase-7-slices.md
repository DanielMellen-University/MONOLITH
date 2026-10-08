# Phase 7 slices (September 2026)

Phase 7 added shell features in small slices. These notes were kept as separate files during the work; they are collected here. The current behavior is described in the app and internals pages linked from each entry.

## 7.1 PNG and JPEG wallpaper

- Wallpaper paths may end in `.bmp`, `.png`, `.jpg` or `.jpeg` (any case).
- PNG and JPEG are decoded with `stb_image.h`, downloaded at build time with a pinned SHA-256 (`third_party/stb/`). BMP stays on `SDL_LoadBMP`.
- Settings completion accepts the new extensions. A sample PNG is seeded at `/Wallpapers/sample.png` when `assets/wallpapers/sample.png` exists.
- Now: [wallpaper.md](../internals/wallpaper.md).

## 7.2 Breakout

- Fourth game: paddle, bricks, three lives, win by clearing the board.
- Added to Start menu Games, session restore, numbered titles, CMake and `verify_games_integration.sh`. Tested by `test_breakout_state`.
- Now: [breakout.md](../apps/breakout.md).

## 7.3 Desktop icons

- Icons for Terminal, Filesystem, Editor, Drawing and Settings, drawn on the wallpaper under all windows.
- Click selects, double-click launches; windows always win hit testing. Enter launches only when no visible window has focus.
- Now: [desktop-shell.md](../internals/desktop-shell.md#desktop-icons).

## 7.4 App registry

- `AppRegistry` became the single table for the Start menu, desktop icons and session kinds.
- No behavior change for any app.
- Now: [desktop-shell.md](../internals/desktop-shell.md#app-registry).

## 7.5 Wallpaper fit modes

- `wallpaper_fit=cover|contain|center` in `desktop_settings.txt`, default `cover`; unknown values become `cover`.
- Settings gained Cover, Contain and Center options.
- Now: [settings.md](../apps/settings.md#wallpaper-fit), [wallpaper.md](../internals/wallpaper.md#fit-modes).

## 7.6 Start menu type-ahead (2026-09-22)

- Typing in the open Start menu filters rows by label; Escape clears the filter, then closes.
- The filter is shown under the menu header. Added `test_start_menu_filter` and `verify_start_menu_filter.sh`.
- Now: [desktop-shell.md](../internals/desktop-shell.md#start-menu).

## 7.7 Multi-column desktop icons (2026-09-24)

- Icons wrap into more columns when the desktop is short. Icons that cannot fit whole are still left out.
- Layout tests and static checks extended.

## Shell polish

- Start menu click targets are built from `AppRegistry` rows, the same source as drawing.
- The hand-written `wm_start_menu_entries.inc` app table was removed, and the static checks make sure it stays gone.
