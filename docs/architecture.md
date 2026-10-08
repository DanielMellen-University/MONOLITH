# Architecture

MONOLITH is one SDL2 program. Inside its window a small desktop shell (the window manager) draws window frames, a taskbar and a Start menu, and hosts native C++ apps. All apps share one internal filesystem that is stored on the host under `~/.monolith/fs/`.

```text
+-------------------------------------------------------------+
| main.cpp: SDL setup, event loop, session save on exit        |
+-------------------------------------------------------------+
| WindowManager (desktop shell)                               |
|   frames, drag/resize, focus, taskbar, Start menu, clock,   |
|   desktop icons, wallpaper, Alt+Tab, session restore        |
+-------------------------------------------------------------+
| Apps (one App object per window)                            |
|   Terminal, TextEditor, Filesystem, Drawing, Settings,      |
|   Snake, Minesweeper, Pong, Breakout                        |
+-------------------------------------------------------------+
| Filesystem (virtual paths -> ~/.monolith/fs/)               |
|   atomic writes, recursive copy/remove, listings            |
+-------------------------------------------------------------+
```

## Startup and main loop

`src/main.cpp` (its body is stored compressed; see [building.md](development/building.md#generated-sources)) does the following:

1. Starts SDL video and SDL_ttf, then creates a fixed 1280x720 window titled "Monolith" and an accelerated renderer with VSync.
2. Loads `DejaVuSans.ttf` at 14 pt, trying `assets/fonts/` relative to the working directory first, then the usual system font paths.
3. Opens the filesystem at `~/.monolith/fs` (or `./monolith_fs` if `HOME` is unset) and creates `/home/monolith`, `/home/monolith/documents`, `/home/monolith/drawings`, `/Wallpapers`, `welcome.txt` and the two sample wallpapers if they are missing.
4. Creates the `WindowManager`, loads `~/.monolith/desktop_settings.txt`, and restores `~/.monolith/session.txt`. If there is no valid session it opens Terminal, Filesystem, Text Editor on `welcome.txt`, and Settings.
5. Runs the loop: pass each SDL event to the window manager, set the mouse cursor for resize edges, call `update()`, clear to the desktop color, call `render()`, present.
6. On quit, saves the session. If that fails it shows a warning dialog while SDL is still running. The window manager and its apps are destroyed before the renderer and SDL shut down, so app textures are freed while the renderer is still valid.

SDL text input is started once after the renderer exists and stopped before shutdown, so every app receives typed UTF-8 text the same way.

## Window manager

`WindowManager` (`src/window/`) owns every window. Each `Window` holds its geometry, minimized and maximized state, title, and the `App` it hosts.

It is responsible for:

- Drawing frames: a title bar with minimize, maximize and close buttons.
- Dragging by the title bar and resizing from any edge or corner. There is no snapping or tiling.
- Z-order and keyboard focus.
- The taskbar: Start button, one button per window (scrolls when there are many), and a clock (12-hour by default).
- The Start menu, built from the app registry, with a Games group and a type-ahead filter.
- Desktop icons for Terminal, Filesystem, Editor, Drawing and Settings.
- The desktop background color and optional wallpaper image.
- Numbered titles for multiple windows of one app ("Terminal", "Terminal 2", ...). Closing one renumbers the rest so there are no gaps.
- Opening files by type: `.modr` files go to Drawing, everything else to Text Editor. Each file opens in at most one window; opening it again focuses that window.
- Saving and restoring the session.

The desktop is a 1280x720 logical surface. The window manager converts mouse positions to logical pixels once, at the edge, so apps and hit testing never deal with host-window scaling.

Details such as focus handoff, taskbar layout, close guards and restore limits are in [internals/window-manager.md](internals/window-manager.md). The registry, Start menu and icons are in [internals/desktop-shell.md](internals/desktop-shell.md).

## Apps

Every app is a class derived from `monolith::app::App` (`src/app/App.hpp`). The window manager calls:

| Callback | When |
|----------|------|
| `render(renderer, contentRect)` | Every frame, for visible windows. The app draws inside its client rectangle. |
| `handleEvent(event)` | Keyboard and text input when focused; mouse input inside the client area. |
| `update()` | Every frame for non-minimized windows. Games use it for their timers. |
| `onFocusGained()` / `onFocusLost()` | Focus changes, including when the Start menu opens. Games pause here. |
| `onResize(w, h)` | After the client size changes, with the final size. |
| `onUiScaleChanged()` | After Settings changes the text size. |
| `allowClose()` | Before the window closes. Editor and Drawing return false while there is unsaved work. |
| `onVirtualPathCreated/Changed/Moved/Removed` | After any app changes the filesystem, so others can refresh. |
| `onBoundFileMoved` / `onBoundFileRemoved` | When the file an Editor or Drawing window has open is moved or deleted. |

Apps never reference each other. When an app needs the shell, it calls its `IWindowController`:

| Group | Methods |
|-------|---------|
| Window | `close`, `setTitle`, `requestClientSize`, `restoreTrackedInstanceTitle` |
| Opening files | `openPath` (by file type), `openInTextEditor`, `openInDrawing`, `focusEditorForFile`, `focusDrawingForFile` |
| File ownership | `bindEditorFile`, `bindDrawingFile`, and their `clear...` counterparts |
| Change broadcasts | `notifyVirtualPathCreated`, `...Changed`, `...Moved`, `...Removed` |
| Shared clipboard | `get/set/clearFilesystemClipboard` (copy and cut between Filesystem windows) |
| Desktop settings | background color, wallpaper path and fit, 12/24-hour clock, text size, logical desktop size |

Each window gets its own controller, created with the window and destroyed with it.

Apps are started through launcher methods on the window manager (`launchTerminal()`, `launchTextEditor(path)`, `launchFilesystem()`, `launchDrawing()`, `launchSettings()`, `launchSnake()`, `launchMinesweeper()`, `launchPong()`, `launchBreakout()`). The Start menu, desktop icons, session restore and file routing all go through these.

### Callback safety

An app callback may close its own window, close a sibling, or open new windows. The window manager handles this by:

- Deferring a close requested from inside a callback until the outermost callback returns, then checking that the window still exists.
- Iterating over a snapshot of windows (checked by pointer and ID) during update, render and broadcasts, so a window closed mid-pass is skipped instead of dereferenced.

See [internals/window-manager.md](internals/window-manager.md#callback-safety).

## Rendering

- The window manager draws the wallpaper, then desktop icons, then windows back to front, then the shell chrome on top: the Start menu when open, the taskbar, and the Alt+Tab overlay.
- Each app draws with SDL draw calls and SDL_ttf text, clipped to its client rectangle. The client rectangle can shrink to zero on a tiny desktop but is never negative.
- Apps that use a narrower clip inside their window intersect it with the caller's clip and restore it afterwards. The helpers for this are in `src/detail/RendererClip.hpp`.
- Text is rendered once into textures and reused through a bounded LRU cache per app (256 entries, about 16 MiB).

More in [internals/rendering.md](internals/rendering.md).

## Input

Events are handled in this order:

1. **Host focus.** If the MONOLITH window itself is not focused, queued key events are dropped.
2. **Shell hotkeys.** Alt+Tab and Alt+Shift+Tab cycle windows. Ctrl+Escape toggles the Start menu. While the menu is open it takes all keys: type to filter, Up/Down to select, Enter to launch, Escape to clear the filter or close.
3. **Taskbar, Start menu, title bars and frame edges.**
4. **Desktop icons**, when the click hits no window.
5. **The focused app**, for keyboard input, or the app under the pointer for mouse input.

A mouse press inside an app captures the pointer: that app keeps getting motion and the matching release even if the pointer leaves the window, so drags in Drawing or text selection in the editor always end cleanly. If MONOLITH loses host focus mid-drag, the shell sends the app a release and finishes any frame drag.

The Start menu is modal. Opening it sends `onFocusLost` to the focused app; closing it sends `onFocusGained` back if that app still has focus.

## Filesystem

The `Filesystem` class (`src/fs/`) maps virtual paths such as `/home/monolith/notes.txt` to files under `~/.monolith/fs/`. It provides listing, create, rename, remove, recursive copy and remove, and atomic writes. Terminal, Text Editor, Filesystem, Drawing and Settings all use the same instance.

Every write goes to a hidden temporary workspace next to the target and is renamed into place only after the whole file is written and synced. A crash or failed write leaves the old file untouched.

See [filesystem.md](filesystem.md) for paths and the API, and [internals/atomic-writes.md](internals/atomic-writes.md) for how writes are staged and cleaned up.

## Files on the host

| Path | Written by | Format |
|------|------------|--------|
| `~/.monolith/fs/` | Filesystem | Ordinary files and directories |
| `~/.monolith/session.txt` | WindowManager on exit | `session_v1` header, then one line per window: kind, x, y, w, h, minimized, maximized, quoted path |
| `~/.monolith/desktop_settings.txt` | Settings changes | `key=value` lines; see [settings.md](apps/settings.md#settings-file) |
| `~/.monolith/snake_highscore.txt` | Snake | One score |
| `~/.monolith/minesweeper_best.txt` | Minesweeper | Best time per difficulty |

All of them are written atomically.

## Design decisions

- **Windows, not full-screen modes.** Several things can be open and visible at once.
- **Native apps.** Every app is plain C++ compiled into the one binary.
- **The window manager is the foundation.** Everything else sits on top of it, and apps do not need to know how frames are drawn or input is routed.
- **A basic filesystem.** Paths, files and directories, with no permissions or metadata layer. Reliability over features.
- **One registry.** Adding an app means one table row, one launcher and one dispatch case.
- **Session restore is layout only.** It remembers windows and open files, not unsaved content.

For where each piece lives in the tree, see [repo-layout.md](development/repo-layout.md).
