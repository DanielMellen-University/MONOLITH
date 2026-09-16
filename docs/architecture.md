# Monolith Architecture

## Overview

Monolith is a single Linux application that contains a complete, self-contained desktop-like environment. It is designed to feel like a small personal operating system that the user "enters," while remaining a normal application on the host.

The core experience is built around **overlapping windows** with traditional desktop behaviors. Most functionality lives in native applications that run inside these windows.

## Goals

- Create a cohesive, personal computational environment that feels like a mini OS.
- Support multiple things open at once with a polished windowing experience.
- Keep the system growable over many years as a personal project.
- Maintain a clear separation between the core environment and the individual apps.
- Provide a custom scripting language primarily for automation and extension.

## High-Level Model

```
┌──────────────────────────────────────────────────────────────┐
│                        Monolith (SDL2)                       │
├──────────────────────────────────────────────────────────────┤
│  Window Manager / desktop shell                              │
│  - Frames, drag/resize, focus, taskbar (+ clock), Start menu │
│  - Session restore, openPath routing, launchers              │
├──────────────────────────────────────────────────────────────┤
│  App Host / Client Areas                                     │
│  - Native C++ apps render into their window content          │
├──────────────────────────────────────────────────────────────┤
│  Built-in Apps (native)                                      │
│  - Terminal, Filesystem, Editor, Drawing, Settings,          │
│    Snake, Minesweeper, Pong, Breakout                         │
├──────────────────────────────────────────────────────────────┤
│  Basic Filesystem                                            │
│  - Hierarchical, persisted under ~/.monolith/fs/             │
├──────────────────────────────────────────────────────────────┤
│  Language Runtime (planned — not implemented)                │
│  - Scripting / automation from Terminal and other apps       │
└──────────────────────────────────────────────────────────────┘
```

## Core Components

### 1. Window Manager

The Window Manager is the most foundational subsystem.

**Responsibilities:**
- Owns all top-level windows
- Draws window frames (title bar + three buttons)
- Handles window dragging and resizing
- Manages z-order and focus
- Provides a taskbar for window switching and status (local-time clock on the right)
- Routes input events to the correct window

**Window Model:**
- Every window has a title bar with a title and three standard buttons.
- Windows support dragging by the title bar.
- Windows support resizing from edges and corners.
- Windows can be minimized via the title-bar minimize button or by clicking the active window's taskbar button (XP-style toggle), and restored by clicking its taskbar entry.
- When the focused window is closed, focus moves to the topmost non-minimized remaining window (z-order), with `onFocusLost` / `onFocusGained` fired so apps stay consistent. Focus is published before those callbacks, so a callback-triggered close cannot restore a destroyed pointer afterward. If every survivor is minimized, focus stays clear until the user activates a window.
- Start-menu dismissal is a single focus transition: clicking another window, choosing a taskbar entry, or using Alt+Tab consumes the menu's suspension marker during the real handoff, so the previously focused app is not resumed and immediately suspended again.
- Minimizing the focused window uses the same handoff rule: the topmost non-minimized survivor becomes focused, or focus is cleared when none remain. Minimized apps never receive keyboard events.
- When focus is cleared because every window is minimized, the shell invalidates taskbar hit targets immediately because the focused-first taskbar ordering no longer applies.
- When many windows are open, the taskbar scrolls horizontally (arrow buttons and mouse wheel). Arrow hit targets are recorded during render (same pattern as taskbar window buttons) and handled in the taskbar click path.
- Closing a window immediately removes its cached taskbar hit target, so keyboard- or app-triggered closes cannot leave a raw pointer for a later event between renders.
- Minimizing a window invalidates cached taskbar hit targets immediately, including when the minimized window was not focused and no z-order handoff occurs.
- Taskbar scrolling is clamped to the measured button strip after arrow controls reserve their space, so repeated input cannot scroll every window button out of view.
- On narrow logical desktops, the taskbar button viewport is clamped to non-negative space; scroll arrows are shown only when both controls fit, stale scroll offsets reset after a shrink, and button rendering plus hit rectangles are clipped to the visible viewport.
- The Start button is also clipped to the logical desktop edge, and its hit target stops there, so an undersized desktop cannot paint or activate shell chrome outside its own surface.
- The Start menu clamps its width to the logical desktop and its visible height to the usable area above the taskbar; partially visible rows are clipped and only their visible portions remain clickable on undersized desktops.
- The clock tray yields the button strip when the available width is too small for both controls, preventing taskbar status UI from overlapping window-button input.
- Maximize, restore, and logical desktop resizing keep maximized frames aligned to the usable area, clamp restored frames above the taskbar, and notify the app after the final client geometry is known.
- Bringing a minimized maximized window forward reapplies the current usable desktop rectangle immediately, including after the logical desktop grows while it was hidden.
- Taskbar window labels stay at native text size, render through SDL_ttf's UTF-8 API, and each button measures its UTF-8 title before allocating width, so Unicode filenames, larger fonts, and long titles clip inside their own button without drawing over neighbors.
- Taskbar buttons and the clock tray also derive their height from the active font, capped by the taskbar band, so scaled labels remain vertically contained.
- Window title labels stay at native text size, render through SDL_ttf's UTF-8 API, and clip before the minimize button, so Unicode filenames and long file-backed titles do not get misdecoded, horizontally distorted, or cover title-bar controls.
- The taskbar shows a compact local-time clock on the right (12-hour by default; Settings can switch to 24-hour via `DesktopSettings`). The time texture is rebuilt when the minute or format changes; hovering the clock tray shows the full local date in a small tooltip above the bar.
- The clock date tooltip clamps to the usable desktop in both axes and clips its contents to the caller's renderer clip, so short or narrow clients cannot place it outside the shell surface.
- Settings can change the shared interface font to 90%, 100%, or 115%. `WindowManager` applies the selected point size to the shared `TTF_Font`, then invalidates title, clock, and Start-menu header textures so the change appears immediately in existing windows.
- Static shell labels and taskbar titles are keyed by UTF-8 text and color and cached between frames; this includes the taskbar Start button, window buttons, Start-menu header, category labels, and normal or hovered rows. The cache is rebuilt only after the shared font changes.
- Desktop icon glyphs and labels are cached per icon, with separate normal and selected label textures, and are rebuilt only after the shared font changes.
- After a shared font change, `WindowManager` calls `App::onUiScaleChanged()` on every app, including minimized ones, from a stable window snapshot, so cached layout and pixel-based scroll state can be rebuilt without pretending the window itself was resized.
- Desktop wallpaper images (BMP through SDL, PNG/JPEG through `WallpaperImage`) are optional: Settings stores a virtual FS path; the Window Manager cover-scales the texture over the solid background color before drawing windows. Empty or unloadable paths fall back to solid color only. A later creation of a missing configured image (or one of its parent directories) invalidates the failed-load cache so the next render retries it.
- **Session restore**: on exit, open windows (kind, geometry, minimize/maximize, file paths for editors/drawings) are written to `~/.monolith/session.txt` through a temporary sibling snapshot, then atomically replaced after the complete stream is validated. File paths are quoted so virtual names containing spaces, quotes, or backslashes survive a restart; older unquoted path tokens remain readable. On next launch a valid session file is accepted even when it restores no windows, so missing or invalid file-backed entries are skipped rather than becoming blank untitled apps or triggering the demo window set; missing files and invalid headers still use the demo fallback. Restore applies geometry by the launched window's monotonic ID, so a lifecycle callback that opens another window cannot redirect the saved geometry to the callback-created window.
- Restoring a minimized final session entry hands keyboard focus to the topmost visible survivor instead of leaving focus attached to the hidden app.
- Session geometry callbacks are identity-checked after `onResize()`, so an app that closes during restore cannot leave the loader inspecting a dead window or count a vanished entry as restored.
- Screen-to-logical pointer conversion floors scaled coordinates, so positions just above the host header or left of the desktop remain outside logical row and column zero instead of leaking into an edge window.
- **Open-with routing**: `WindowManager::openPath` / `IWindowController::openPath` maps a case-insensitive `.modr` suffix → Drawing and all other files → Text Editor (used by Terminal `open` and the Filesystem Browser default Open).
- Focusing an already-open file through the editor or Drawing singleton bridge also restores that window from minimized state before bringing it forward.
- Bringing a minimized window forward re-applies desktop clamping first, so stale session geometry cannot put its title bar under the taskbar or off the desktop.
- New and restored frames honor the shared minimum size before desktop clamping. Desktop clamping keeps visible frames above the taskbar, shrinking below that normal minimum only when a narrow logical desktop cannot fit a full-size window. If the usable region is shorter than the title bar, the frame stays anchored at a non-negative origin and rendering passes apps a zero-height client instead of negative geometry.
- When a logical desktop resize or interactive resize changes a visible window's frame, the Window Manager sends `App::onResize` with the final client dimensions after all clamping. Resize callbacks use a stable window snapshot, so an app can close or open windows without invalidating the remaining geometry pass. Apps never have to infer shell geometry changes from stale render rectangles.
- The Alt+Tab title overlay converts measured text from screen pixels to logical width before sizing its box, then clips the native-size label inside that box.
- No snapping or automatic tiling.

**Design Notes:**
The Window Manager should be relatively self-contained. Individual apps should not need to know how window frames are drawn or how input is routed.

Each window that hosts an app owns a small `IWindowController` (`Window::controller`) created by `createControllerFor`. Controllers are not process-global: they are destroyed with the window after the app’s controller pointer is cleared.

Window geometry that affects the desktop shell is centralized in the Window Manager. Taskbar bounds, usable desktop bounds, logical-to-screen rectangle conversion, and title-bar button rectangles are shared by rendering, hit testing, maximize, resize, drag clamping, and new-window placement. This keeps drawn controls aligned with click targets and keeps window title bars accessible above the taskbar.

### Instance Management for Multiple Windows of the Same Type

The WM provides first-class support for opening many instances of the same native app (Terminal, Filesystem, Drawing, Settings, bare Text Editor, future apps) without title collisions or confusing numbering.

- `claimNextAppInstanceTitle(const std::string& base)` finds the lowest free positive instance number for a base ("Terminal", "Settings", "Editor" for bare editors, etc.), reserves it, and returns the display title + number.
  - Instance 1 → bare name ("Settings").
  - Instance 2+ → "Settings 2", etc.
- Each `Window` carries `appBaseTitle` and `appInstanceNumber` (populated by launchers via an extended `createWindow`).
- On `closeWindow`, if the window held a tracked instance, `compactAppInstances(base)` re-numbers all *remaining* live windows of that base contiguously from 1 (sorted by prior instance number to preserve relative order).
  - Titles on the survivor `Window` objects are updated in place.
  - Title caches are invalidated so the change appears immediately in title bars and the taskbar on the next render.
  - The active set is rebuilt from the compacted numbers.
- Result: among currently open windows there are never gaps or duplicates for a given type. Closing a lower number causes higher ones to "slide down" (e.g. "Settings" + "Settings 2"; close the first → the second becomes "Settings").
- File-backed editors use content-derived titles ("Editor - foo") and are deliberately excluded from the bare "Editor" numbering pool (they are already unique and protected by the `m_fileEditors` singleton + `associateEditorWithFile`).
- Saving a bare Editor or Drawing as a file releases its old bare-app instance reservation and compacts the remaining bare windows, so file-backed titles never consume numbered bare-app slots.
- Direct `createWindow` calls (rare fallback paths) can opt out of tracking.

Launchers are the canonical place that request instance titles. The mechanism is intentionally centralized in the desktop shell (`WindowManager`) so new app types get correct behavior for free.

See `src/window/WindowManager.cpp` (`claimNextAppInstanceTitle`, `compactAppInstances`, `closeWindow`, launcher bodies) and `Window.hpp`. The design follows the same "annotate the Window + cleanup on close" pattern used for editor singletons (`editedFilePath` / `m_fileEditors`).

### 2. Application Model

Core applications are written in **native C++**.

Each app runs inside a window provided by the Window Manager. The app is primarily responsible for:
- Rendering its content area (the area inside the window frame)
- Handling input events delivered to its client area
- Managing its own internal state
- Managing app-local modes such as editor find, rename prompts, or terminal search

The Window Manager handles the frame, decorations, and top-level input routing.

### Desktop Shell & App Coordination

The Window Manager also acts as a small "desktop shell". It provides launcher methods (`launchTerminal()`, `launchTextEditor(path)`, `launchFilesystem()`, `launchDrawing()`, `launchSettings()`, `launchSnake()`, `launchMinesweeper()`, `launchPong()`, `launchBreakout()`) used by the Start Menu and by apps.

The Start menu keeps most apps as top-level entries. Games that clearly form a group (**Snake**, **Minesweeper**, **Pong**, **Breakout**) sit under a non-clickable **Games** category header with a slight indent; only categories that make sense are introduced this way.

Each frame, `WindowManager::update()` calls `App::update()` on every non-minimized window's app. The dispatch uses a per-frame pointer snapshot and skips windows that an earlier app closed, so controller-driven self-closes cannot invalidate the live window vector. Most apps leave this as a no-op; games use it for fixed-rate ticks and timers. Pong and Breakout convert SDL's wrapping 32-bit tick counter through the shared `detail::tickDeltaSeconds()` helper, which caps a stalled frame at 50 ms before passing it to their rule engines.

Apps can request shell actions through `IWindowController`: `close()`, `setTitle()`, `restoreTrackedInstanceTitle()`, `openInTextEditor` / `openInDrawing` / **`openPath`** (extension-based default), editor/drawing singleton focus and file-binding helpers, virtual path lifecycle notifications for created, changed, moved, and removed entries, the shared virtual filesystem clipboard, desktop background get/set, wallpaper path get/set, taskbar clock 12/24-hour get/set, and interface text scale get/set. Apps do not depend on each other directly. Temporary title overrides (e.g. Drawing after save) restore via `restoreTrackedInstanceTitle()`.

Renderer clip capture, intersection, and restoration live in `src/detail/RendererClip.hpp`, shared by the shell, native apps, and games. WindowManager client dispatch and Browser context menus use the same intersection helper as app renderers. Any nested clip must intersect the caller's clip and restore it before returning; this keeps embedded app rendering from leaking into neighboring shell regions.

Text Editor, Drawing, Terminal, and Settings share the UTF-8 editing helpers in `src/app/Utf8.hpp`. Path completion uses the same complete-codepoint common-prefix rule as caret movement and deletion, so ambiguous multibyte filenames cannot leave a partial character in an app prompt.

The Window Manager broadcasts virtual path creation, change, move, and removal events to every open app from a stable window snapshot. Text Editor and Drawing claim newly saved file bindings before broadcasting creation, so a synchronous observer opening that path focuses the existing owner instead of creating a duplicate. Bound editor and drawing remaps also identity-check each target before invoking app callbacks, so one callback can close a sibling without invalidating the remaining binding pass. An app may close or open windows from a lifecycle callback without invalidating the broadcast or skipping a surviving app. Filesystem Browser uses creation and change events to refresh a visible parent directory, including an ancestor listing when a write creates missing parents, refreshes the directory itself when that directory is the changed path, and remaps a selected direct child when an external rename changes its name. Terminal, Text Editor, and Drawing use move/remove events to keep working directories and file bindings coherent. Recursive Terminal copies into existing trees broadcast each changed destination path so bound documents below the tree are not stale. Text Editor and Drawing keep their in-memory documents stable when another app overwrites a bound file, reporting the change instead of silently reloading it. Change events also invalidate the cached wallpaper when the changed path contains the active wallpaper. The broadcast is sent only after the filesystem operation succeeds.

All app callbacks enter through a small lifecycle scope. If a callback requests a window close, the shell records the window identity and defers destruction until the outermost callback returns, then rechecks that identity before closing it. This keeps synchronous observers, focus handlers, resize callbacks, input handlers, and app updates from destroying an app while its own callback is still executing.

### 3. Rendering

- The entire environment is rendered inside a single SDL2 window. The shell uses a runtime logical desktop size (1280 × 720 by default) and maps it to the host window; apps receive the resulting client geometry through `onResize`.
- The Window Manager is responsible for compositing window frames and delegating content drawing to apps.
- Rendering is clipped to the caller's renderer clip for the full WindowManager frame, then each app is additionally clipped to its window's client rectangle, so tiny or undersized app layouts cannot paint into title bars or the taskbar. WindowManager composes each frame with neutral draw blend and color state, restores both after every app callback, and returns the caller's original blend mode and draw color after the frame. Apps and shell overlays that use narrower internal clips must intersect and restore the caller clip; temporary translucent overlays also restore the caller's draw blend mode. Browser text and context menus, Settings, Terminal, Text Editor, Drawing, Pong, Snake, Minesweeper, Breakout, title bars, taskbar buttons, and Alt+Tab follow this rule explicitly. Client rectangles may be zero-sized on an undersized desktop, but are never negative.
- WindowManager renders from a live identity snapshot, so an app that closes or replaces its window during `render()` cannot invalidate the frame loop or leave the shell drawing through a dead window pointer; newly opened windows render on the next frame.
- Rendering uses SDL2's accelerated renderer with VSYNC; apps draw text via SDL_ttf and primitives via SDL draw calls.
- The main loop explicitly starts SDL text input after the renderer is created and stops it before SDL shutdown, so Terminal, Text Editor, Drawing, and Settings receive printable UTF-8 input through the same owned lifecycle.
- The main loop scopes the Window Manager and its SDL-backed apps before destroying the renderer or shutting down SDL, so texture-owning destructors run while their renderer and SDL services are still valid.
- Filesystem Browser, Text Editor, Terminal, Drawing, Pong, Breakout, Snake, and Minesweeper cache repeated SDL_ttf surfaces for toolbar labels, paths, syntax spans, line numbers, scrollback rows, status text, HUDs, overlays, and repeated glyphs; the caches are renderer-independent and clear when the shared UI scale changes. Text Editor also clears its cache when document content or the loaded document changes, Terminal clears scrollback surfaces when output is cleared or trimmed, and Browser clears its cache on listing or status changes; per-frame textures remain short-lived.

### 4. Input System

- All input enters through the main SDL2 event loop.
- **Shell hotkeys** are handled first (and not forwarded to apps): **Alt+Tab** / **Alt+Shift+Tab** cycles focused windows (minimized ones restore; the shell consumes the matching Tab and Alt releases after the title overlay closes); **Ctrl+Escape** toggles the Start menu and consumes the matching Escape release; while the Start menu is open, **Up/Down** selects an item, **Enter** activates it and consumes its matching release, and **Escape** closes it while consuming its matching release too.
- The Start menu is modal: opening it sends `onFocusLost` to the previously focused visible app and suppresses client keyboard input, while closing it restores `onFocusGained` only when that same app still owns focus. Host focus loss while the menu is open does not resume the app until host focus returns.
- Focus callbacks and input dispatch reflect actual client activity: host-unfocused and Start-menu transitions suppress app input and nested handoff callbacks, and closing the menu resumes the window that owns logical focus after any callback-triggered changes. A direct handoff to another window suppresses the old app's duplicate resume and loss pair.
- The host-focus guard runs before shell-hotkey dispatch, so queued key events cannot open Start, cycle Alt+Tab focus, or reach an app while the SDL host window is unfocused. An already-open Start menu may still be dismissed with Escape without resuming its suspended app.
- The Window Manager performs hit testing to determine which window (and which part of the window) should receive the event.
- Screen-space mouse events are converted to logical desktop pixels once at the shell boundary before window hit testing, drag/resize math, or client-area forwarding.
- A client that receives a left-button press keeps receiving matching motion and release events until that button is released, even if the pointer leaves the window or focus changes. A press handled by the desktop or a window frame is never followed by a synthetic client release. This keeps drag interactions such as Drawing strokes from getting stuck without leaking releases into another app.
- Captured app gestures own the pointer until release: Drawing clamps captured strokes to the canvas edge, Text Editor clamps captured selection motion to the nearest visible document edge, and focus-loss callbacks end active app gestures when a modal shell transition cannot provide a matching release.
- Taskbar and Start-menu left-button presses use a separate shell capture, so their release is consumed by the shell and never appears as an orphaned client mouse-up.
- Shell and window-frame presses also suppress client motion until a client owns a press; dragging a title bar or moving across the desktop cannot inject hover motion into an app.
- If the host SDL window loses focus during a drag, the shell synthesizes the captured client release, clears frame capture, and sends focus callbacks; regaining host focus restores the focused app callback without reviving stale pointer state.
- The shell records the latest pointer position from motion and button events, and refreshes it from SDL for wheel events, so scrolling with a stationary pointer does not reuse a stale position.
- Screen-space taskbar, clock, and Start-menu hit targets are rebuilt during render or on demand before an event when needed. Taskbar geometry, including the Start button and scroll arrows, is shared between rendering and hit testing, and all shell targets are invalidated whenever desktop geometry, display scale, header offset, clock format, interface font metrics, window creation, z-order, close, title, or taskbar-scroll changes, so queued input before the next frame cannot see an empty or stale layout.
- After shell hotkeys are handled, a focused app owns its client keyboard event through the end of dispatch. The shell does not bubble that same event into desktop-icon activation if the callback closes the focused window.
- App callbacks may close or replace their window while shell input is being dispatched. Activation and initial sizing verify the original window identity before continuing, so stale event pointers are never dereferenced after a callback.
- Window frame interactions (dragging, resizing, buttons) are handled by the Window Manager.
- Client area events are forwarded to the active application.

### 5. Filesystem

The filesystem is **basic** by design: hierarchical virtual paths, simple CRUD operations, and host-backed persistence under `~/.monolith/fs/`. Regular file writes use the same shared binary-capable atomic writer as host snapshots, with validated temporary siblings and atomic replacement while preserving existing permission bits and in-root file symlink targets. Terminal, Text Editor, Filesystem Browser, and Drawing all share the same `Filesystem` API, including shared **recursive** helpers (`copyRecursive`, `removeRecursive`, `isSameOrDescendant`, `join`) so apps do not reimplement tree walks.

See [filesystem.md](filesystem.md) for virtual path rules, host mapping, and app usage. Advanced features (permissions, metadata, versioning, etc.) are explicitly out of scope for the foreseeable future.

### 6. Built-in Applications

Native C++ apps render into window client areas and are launched via shell methods on the Window Manager. Each app has dedicated user documentation:

| App | Doc | Implementation |
|-----|-----|----------------|
| Terminal | [apps/terminal.md](apps/terminal.md) | `TerminalApp` |
| Text Editor | [apps/text-editor.md](apps/text-editor.md) | `TextEditorApp` |
| Filesystem Browser | [apps/filesystem-browser.md](apps/filesystem-browser.md) | `FilesystemApp` |
| Drawing | [apps/drawing.md](apps/drawing.md) | `DrawingApp` |
| Settings | [apps/settings.md](apps/settings.md) | `SettingsApp` |
| Snake | [apps/snake.md](apps/snake.md) | `SnakeApp` |
| Minesweeper | [apps/minesweeper.md](apps/minesweeper.md) | `MinesweeperApp` |
| Pong | [apps/pong.md](apps/pong.md) | `PongApp` |
| Breakout | [apps/breakout.md](apps/breakout.md) | `BreakoutApp` |

**Shell coordination:** Apps use `IWindowController` for close, titles, open/openPath, file bindings, shared virtual filesystem clipboard, desktop color, wallpaper path, clock format, and interface text scale. Settings persists these preferences to `~/.monolith/desktop_settings.txt` through the shared atomic text-writer helper. Session layout persists to `~/.monolith/session.txt` via `WindowManager::saveSession` / `loadSession` (wired from `main`), with the same replacement guarantee; Snake and Minesweeper use that helper for their host score records as well.

**Input note:** The Window Manager captures the client that receives `SDL_MOUSEBUTTONDOWN` and forwards matching motion and `SDL_MOUSEBUTTONUP` events to that same client, so drag interactions (e.g. Drawing strokes) end cleanly when the mouse leaves or focus changes.

**Close note:** Before destroying a window, the shell calls `App::allowClose()`. Apps may return false once to warn about unsaved work (second close discards). Close requests made from app callbacks are deferred until the outermost callback returns, then the target identity is rechecked before `allowClose()` or destruction. The close path also rechecks target identity after `allowClose()` and the focused app's `onFocusLost()` callback, re-finding the target after that callback because it may close a sibling and invalidate vector storage. This prevents a callback that closes its own window or a sibling from leaving an outer close operation using destroyed storage. Start menu Shut Down and the host window's `SDL_QUIT` event both call the same contract on every open app in each request, using a live identity snapshot and revisiting IDs opened by callbacks before accepting quit. That prevents a callback-created dirty document from bypassing shutdown validation while still arming multiple original dirty documents together. Default is always allow.

### 7. Language Runtime

The custom language is intended primarily for **scripting and automation**.

Details are deferred. Initial goals include standard language features (variables, functions, control flow, basic data structures, recursion, modules) plus the ability to call useful host functions from scripts.

The language is not expected to create or manage its own windows in the early phases.

## Key Design Decisions

- **Windowed environment over modal/fullscreen switching.** Multiple things can be open and visible at once.
- **Native-first apps.** The main applications are written in C++ rather than the custom language.
- **Window Manager as foundation.** Most other systems sit on top of the windowing layer.
- **Basic filesystem.** Simplicity and reliability over feature richness.
- **Language as automation tool.** Not the primary way to build full applications (at least initially).

## Non-Goals / Constraints

- Not a real operating system.
- Not primarily intended for other users.
- Not trying to match the power or complexity of a modern desktop environment.
- The language is not required to build GUI applications directly.
- Session restore is best-effort layout only (not a full workspace product).

## Host-side paths (outside the virtual FS)

| Path | Purpose |
|------|---------|
| `~/.monolith/fs/` | Virtual filesystem host root |
| `~/.monolith/desktop_settings.txt` | Desktop background color, wallpaper path, clock format, and interface text scale |
| `~/.monolith/session.txt` | Window session for restore |
| `~/.monolith/snake_highscore.txt` | Snake high score |
| `~/.monolith/minesweeper_best.txt` | Minesweeper best times |

## Next Areas to Explore

- Custom language interpreter and host bindings (Phase 2)
- IDE, richer wallpaper controls, and more Settings preferences
- Richer open-with table (more types beyond `.modr` / text)
- Deeper app integration and additional native apps/games

Per-app limitations and planned work are tracked in each [app guide](README.md#built-in-apps).

---

*This document reflects the architecture as of the current codebase. It will evolve as decisions change.*
