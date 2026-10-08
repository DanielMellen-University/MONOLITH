# Window manager internals

Reference for the rules `WindowManager` follows. For the overview, see [architecture.md](../architecture.md#window-manager).

Code: `src/window/WindowManager.{hpp,cpp}`, `src/window/Window.hpp`, and the bodies in `src/window/detail/wm_*.inc`.

## Geometry

- The desktop is a logical surface, 1280x720 by default. Taskbar bounds, the usable desktop rectangle, title-bar button rectangles and logical-to-screen conversion are computed in one place and shared by rendering, hit testing, maximize, resize, drag clamping and new-window placement. Drawn controls and click targets therefore always line up.
- Screen-to-logical conversion floors scaled coordinates, so a point just outside the desktop never lands in row or column zero.
- New and restored windows get the shared minimum size, then are clamped so the title bar stays above the taskbar. On a very narrow desktop a frame can shrink below the minimum. If the usable height is shorter than a title bar, the app gets a zero-height client rather than negative geometry.
- Maximize fills the usable area. Restoring clamps the old rectangle back above the taskbar. A minimized maximized window picks up the current usable area when it comes back, even if the desktop grew meanwhile.
- `onResize` is sent once the final client size is known, after all clamping.

## Focus

- Closing or minimizing the focused window moves focus to the topmost visible window. If every remaining window is minimized, nothing has focus until you pick one.
- Focus is updated before `onFocusLost` and `onFocusGained` run, so a callback that closes a window cannot restore a stale pointer.
- Minimized apps never get keyboard events.
- Dismissing the Start menu by clicking a window, a taskbar button or using Alt+Tab is one focus change. The previous app is not resumed and then immediately suspended again.
- If the host window loses focus, app input is suppressed until it returns. An open Start menu can still be closed with Escape.
- Focusing a file that is already open (for example `open notes.txt` in Terminal) restores its window if minimized and brings it to the front.

## Taskbar

- Buttons are ordered focused-first. Each button measures its UTF-8 title and clips it, so long names, larger fonts and non-ASCII titles stay inside their own button. Measured widths are cached per window and dropped on rename or font change.
- With many windows the strip scrolls with arrow buttons or the mouse wheel. Scrolling is clamped to the measured strip, so repeated clicks cannot scroll every button out of view. Arrows appear only when both fit; a stale offset resets after the desktop shrinks.
- On a narrow desktop the button viewport, the Start button and the clock tray are all clipped to the desktop edge, and the clock gives up its space before it overlaps buttons.
- Clicking the focused window's button minimizes it; clicking any other button restores and focuses that window.
- Hit targets for the taskbar, clock and Start menu are rebuilt during render, or on demand before an event if something changed (window created or closed, z-order, title, scroll, font, clock format, desktop size). Queued input between frames never sees a stale layout.
- The clock text is formatted once per minute. Hovering it shows the full date in a tooltip that is clamped to the desktop.

## Numbered titles

Several windows of one app get distinct titles without gaps:

- `claimNextAppInstanceTitle(base)` takes the lowest free number for a base name. Number 1 is the bare name ("Settings"), then "Settings 2", and so on.
- Each `Window` records `appBaseTitle` and `appInstanceNumber`.
- When a numbered window closes, `compactAppInstances(base)` renumbers the survivors from 1 in their previous order and refreshes their titles. Closing "Settings" turns "Settings 2" into "Settings".
- Editor and Drawing windows with a file open are titled after the file ("Editor - notes.txt", "Drawing - sketch.modr") and do not take a number. Saving a bare window to a file releases its number and compacts the rest.

## Callback safety

All app callbacks run inside a small lifecycle scope:

- A close requested from a callback is recorded by window ID and carried out after the outermost callback returns, after checking the window still exists.
- Update, render and change broadcasts iterate over a snapshot of `(pointer, id)` pairs. An index keyed by pointer and ID makes the liveness check constant time. Snapshot buffers are reused per nesting depth, so steady-state frames do not allocate.
- Windows opened during a pass are picked up on the next pass. Windows closed during a pass are skipped.
- Editor and Drawing claim a newly saved file before the creation broadcast goes out, so an app that reacts by opening that file focuses the existing window instead of creating a second one.

## Change broadcasts

After a successful filesystem change, the app that made it calls `notifyVirtualPath...`, and the shell forwards it to every app:

- **Created / changed:** Filesystem windows refresh if the path is in, or is, the folder they show (including ancestors when parent folders were created). Recursive `cp -r` into an existing tree reports each changed path, so editors below it see the change.
- **Moved:** Editor and Drawing windows follow their file (title, save target, session entry and open prompt all update). Terminals and Filesystem windows inside a moved folder follow it. The shared clipboard follows moved sources.
- **Removed:** Editor and Drawing windows keep their content but drop the file binding and become untitled. Terminals and Filesystem windows inside a deleted folder move to the nearest existing parent. Deleted paths leave the cut clipboard.
- The wallpaper is reloaded when its file changes, or when a move or create supplies a configured image that was missing.

## Closing and quitting

- Before a window closes, the shell calls `App::allowClose()`. Editor and Drawing refuse while they have unsaved changes and show their own Ctrl+D / Ctrl+S / Esc choice. Pressing close again never discards.
- The close path rechecks the target after `allowClose()` and after the focused app's `onFocusLost()`, because either may close other windows.
- Start menu **Shut Down** and the host window close button both ask every open app, including apps opened by another app's callback during the check. Quit happens only if all agree.

## Session restore

Saved to `~/.monolith/session.txt` on exit through the [atomic writer](atomic-writes.md).

Format:

```text
session_v1
# kind x y w h minimized maximized quoted-path
terminal 120 80 640 420 0 0 "-"
editor 300 140 700 500 0 0 "/home/monolith/my notes.txt"
```

- Saved: app kind (from the registry `sessionKey`), geometry (pre-maximize geometry for maximized windows), minimized and maximized flags, and the file path for Editor and Drawing windows.
- Paths are always quoted, with `"-"` meaning no file. Backslashes and quotes are escaped with a backslash and newlines are written as `\n`, so any file name survives a restart. Older unquoted paths still load.
- At most 128 windows are saved: the topmost 128, in stacking order.
- Loading reads at most 1,024 records (blank and comment lines count), with lines capped at 16 KiB, and stops once 128 windows are open.
- A valid file that restores nothing is still accepted, so missing files are skipped instead of opening blank windows or the demo set. A missing file or a bad header falls back to the demo set.
- Geometry is applied by window ID, so an app that opens another window during restore cannot redirect the saved geometry.
- If the last restored window is minimized, focus goes to the topmost visible window.
- If saving fails, a warning dialog appears before SDL shuts down and the previous session file is left as it was.

## Mouse input

- Mouse events are converted to logical pixels once, at the shell boundary.
- A left press inside an app captures the pointer until release. The app gets the matching motion and release even outside its window or after a focus change. Drawing clamps captured strokes to the canvas edge; the editor clamps selection to the visible text.
- A press on the desktop, a frame, the taskbar or the Start menu is never followed by a synthetic app release, and it suppresses app hover motion until an app owns a press.
- A release finishes an active frame drag or resize at its own coordinates, so batched events cannot lose the final position.
- If the host window loses focus mid-drag, the shell sends the captured app a release and finishes the frame drag at the last known position.
- Wheel events refresh the pointer position from SDL, so scrolling without moving the mouse uses the right target.

## Desktop settings

Settings changes go through `IWindowController` to the window manager, which applies them live and saves the whole set to `~/.monolith/desktop_settings.txt`. A rejected value or a failed save is reported back to Settings. After a failed save the new value stays active for the session, Settings shows a warning, and the next successful save writes everything and clears it.

Changing the text size sets the point size on the shared font, invalidates shell text, and calls `onUiScaleChanged()` on every app, including minimized ones.
