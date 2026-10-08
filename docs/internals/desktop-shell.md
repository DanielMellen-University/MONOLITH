# Desktop shell: registry, Start menu, desktop icons

## App registry

`src/window/AppRegistry.hpp` holds `kAppRegistry`, one row per built-in app. The Start menu, desktop icons, launch dispatch and session restore all read it.

Each `AppRegistryEntry` has:

| Field | Use |
|-------|-----|
| `id`, `displayName`, `startMenuLabel` | Names |
| `appBaseTitle` | Base for numbered window titles ("Terminal", "Editor") |
| `sessionKey` | Kind written to `session.txt` (`nullptr` = not restored) |
| `action` | `AppAction` value used by Start menu rows and icons |
| `startMenuCategory` | `nullptr` for top level, `"Games"` for the Games group |
| `showInStartMenu`, `showDesktopIcon`, `desktopOrder` | Where it appears and in what order |
| `desktopLabel`, `desktopGlyph`, icon RGB | Icon label, letter and tile color |

Current rows, in order:

| App | Start menu label | Group | Desktop icon |
|-----|------------------|-------|--------------|
| Terminal | Terminal | top level | yes (1st) |
| Text Editor | Text Editor | top level | yes, labeled "Editor" (3rd) |
| Filesystem | Filesystem | top level | yes (2nd) |
| Drawing | Drawing | top level | yes (4th) |
| Settings | Settings | top level | yes (5th) |
| Snake, Minesweeper, Pong, Breakout | same | Games | no |
| Shut Down | Shut Down | top level, after a separator | no |

Consumers:

- **Start menu rows:** `buildStartMenuRows()` turns the table into rows, inserting category headers and separators. Rendering (`wm_body_03a.inc`) and pre-render hit testing (`ensureStartMenuHitTargets`, through `wm_start_menu_entries.inc`) both build from `wm_start_menu_rows.inc`, so the drawn menu and the clickable menu always match. There is no hard-coded list of apps; `verify_desktop_icons.sh` checks that the old one stays gone.
- **Desktop icons:** `kDesktopIconDefs` is computed at compile time from the registry, sorted by `desktopOrder`. `collectDesktopIconDefs()` returns it, so per-frame layout copies a fixed array instead of rescanning and sorting.
- **Launching:** `WindowManager::launchByAction`.
- **Session:** `findAppByBaseTitle` when saving, `findAppBySessionKey` when loading.

### Adding an app

1. Add a row to `kAppRegistry` (and a value to `AppAction`).
2. Add a `launchXxx()` method on `WindowManager`.
3. Add a case to `launchByAction`.
4. Add the source file to `CMakeLists.txt`.

`scripts/verify_games_integration.sh` and `scripts/test_app_registry.cpp` check the wiring.

## Start menu

- Opened with the Start button or Ctrl+Escape. While open it takes keyboard input and the focused app is suspended (see [window-manager.md](window-manager.md#focus)).
- **Up/Down** select, **Enter** launches, **Escape** closes. Clicking a row launches it. Category headers are not selectable.
- **Type-ahead:** typed text goes to the shell, not to the suspended app. Typing filters rows by case-insensitive substring of the label. The filter holds at most 64 bytes of UTF-8, more than any label, so the cap never hides a match. **Backspace** removes one character. **Escape** clears a non-empty filter first and closes the menu on the next press. Closing the menu clears the filter.
- While filtering, the filter text is shown under the header, and the header grows from 22 to 36 logical pixels.
- The menu is clamped to the desktop width and to the height above the taskbar. Rows that only partly fit are clipped, and only their visible part is clickable.
- Rows, filtered rows and hit targets use fixed-size arrays sized from the registry, so typing does not allocate.

Code: `src/window/StartMenuFilter.hpp` (`filterStartMenuRows`, `startMenuLabelMatches`, `appendStartMenuFilterInput`), key handling in `tryHandleShellHotkeys` (`wm_body_08c.inc`), drawing in `wm_body_03a.inc`.

## Desktop icons

- Five icons: Terminal, Filesystem, Editor, Drawing, Settings. Games stay in the Start menu.
- Drawn on top of the wallpaper and under all windows. Clicks reach an icon only when no window, taskbar or Start menu is hit.
- Single click selects. A second click within 450 ms launches. Timing uses unsigned SDL tick differences, so it still works when the 32-bit tick counter wraps.
- Clicking anywhere else (a window, the taskbar, the Start menu, Alt+Tab, empty desktop) clears the selection and the pending double-click.
- **Enter** launches the selected icon when no visible window has keyboard focus.
- Layout: 48-pixel tiles in 112-pixel-wide cells with a 22-pixel label band, 14-pixel gaps and a 16-pixel margin. Icons fill a column top to bottom, then start a new column to the right; columns are `kDesktopIconColStep` apart (cell width plus gap), so labels never overlap. Icons that do not fit entirely inside the usable area are left out rather than overlapping the taskbar or the right edge. The first row is allowed when it fits exactly.
- Glyph and label textures are cached per icon (normal and selected labels) and rebuilt only after the font changes. Layout and hit testing use fixed-size arrays.

Code: `src/window/DesktopIcons.hpp` (`layoutDesktopIcons`, double-click helper), `src/window/detail/wm_desktop_icons.inc`. Tests: `scripts/test_desktop_icons.cpp`, `scripts/verify_desktop_icons.sh`.
