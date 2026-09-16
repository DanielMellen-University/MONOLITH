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
- When the focused window is closed, focus moves to the topmost non-minimized remaining window (z-order), with `onFocusLost` / `onFocusGained` fired so apps stay consistent. If every survivor is minimized, focus stays clear until the user activates a window.
- Minimizing the focused window uses the same handoff rule: the topmost non-minimized survivor becomes focused, or focus is cleared when none remain. Minimized apps never receive keyboard events.
- When many windows are open, the taskbar scrolls horizontally (arrow buttons and mouse wheel). Arrow hit targets are recorded during render (same pattern as taskbar window buttons) and handled in the taskbar click path.
- Closing a window immediately removes its cached taskbar hit target, so keyboard- or app-triggered closes cannot leave a raw pointer for a later event between renders.
- Taskbar scrolling is clamped to the measured button strip after arrow controls reserve their space, so repeated input cannot scroll every window button out of view.
- On narrow logical desktops, the taskbar button viewport is clamped to non-negative space; scroll arrows are shown only when both controls fit, stale scroll offsets reset after a shrink, and button rendering plus hit rectangles are clipped to the visible viewport.
- The clock tray yields the button strip when the available width is too small for both controls, preventing taskbar status UI from overlapping window-button input.
- Maximize, restore, and logical desktop resizing keep maximized frames aligned to the usable area, clamp restored frames above the taskbar, and notify the app after the final client geometry is known.
- Taskbar window labels stay at native text size, and each button measures its UTF-8 title before allocating width, so larger fonts and long titles clip inside their own button without drawing over neighbors.
- Taskbar buttons and the clock tray also derive their height from the active font, capped by the taskbar band, so scaled labels remain vertically contained.
- Window title labels stay at native text size and clip before the minimize button, so long file-backed titles do not get horizontally distorted or cover title-bar controls.
- The taskbar shows a compact local-time clock on the right (12-hour by default; Settings can switch to 24-hour via `DesktopSettings`). The time texture is rebuilt when the minute or format changes; hovering the clock tray shows the full local date in a small tooltip above the bar.
- Settings can change the shared interface font to 90%, 100%, or 115%. `WindowManager` applies the selected point size to the shared `TTF_Font`, then invalidates title and clock textures so the change appears immediately in existing windows.
- After a shared font change, `WindowManager` calls `App::onUiScaleChanged()` on every app, including minimized ones, from a stable window snapshot, so cached layout and pixel-based scroll state can be rebuilt without pretending the window itself was resized.
- Desktop wallpaper images (BMP through SDL, PNG/JPEG through `WallpaperImage`) are optional: Settings stores a virtual FS path; the Window Manager cover-scales the texture over the solid background color before drawing windows. Empty or unloadable paths fall back to solid color only.
- **Session restore**: on exit, open windows (kind, geometry, minimize/maximize, file paths for editors/drawings) are written to `~/.monolith/session.txt` through a temporary sibling snapshot, then atomically replaced after the complete stream is validated. File paths are quoted so virtual names containing spaces, quotes, or backslashes survive a restart; older unquoted path tokens remain readable. On next launch that file is restored if present; otherwise the demo window set opens.
- Restoring a minimized final session entry hands keyboard focus to the topmost visible survivor instead of leaving focus attached to the hidden app.
- **Open-with routing**: `WindowManager::openPath` / `IWindowController::openPath` maps a case-insensitive `.modr` suffix → Drawing and all other files → Text Editor (used by Terminal `open` and the Filesystem Browser default Open).
- Focusing an already-open file through the editor or Drawing singleton bridge also restores that window from minimized state before bringing it forward.
- Bringing a minimized window forward re-applies desktop clamping first, so stale session geometry cannot put its title bar under the taskbar or off the desktop.
- Desktop clamping keeps visible frames above the taskbar, shrinking below the normal minimum when a narrow logical desktop cannot fit a full-size window. If the usable region is shorter than the title bar, the frame stays anchored at a non-negative origin and rendering passes apps a zero-height client instead of negative geometry.
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

Apps can request shell actions through `IWindowController`: `close()`, `setTitle()`, `restoreTrackedInstanceTitle()`, `openInTextEditor` / `openInDrawing` / **`openPath`** (extension-based default), editor/drawing file binding helpers, virtual path lifecycle notifications for created, changed, moved, and removed entries, the shared virtual filesystem clipboard, desktop background get/set, wallpaper path get/set, taskbar clock 12/24-hour get/set, and interface text scale get/set. Apps do not depend on each other directly. Temporary title overrides (e.g. Drawing after save) restore via `restoreTrackedInstanceTitle()`.

The Window Manager broadcasts virtual path creation, change, move, and removal events to every open app from a stable window snapshot. An app may close or open windows from a lifecycle callback without invalidating the broadcast or skipping a surviving app. Filesystem Browser uses creation and change events to refresh a visible parent directory, including an ancestor listing when a write creates missing parents, refreshes the directory itself when that directory is the changed path, and remaps a selected direct child when an external rename changes its name. Terminal, Text Editor, and Drawing use move/remove events to keep working directories and file bindings coherent. Recursive Terminal copies into existing trees broadcast each changed destination path so bound documents below the tree are not stale. Text Editor and Drawing keep their in-memory documents stable when another app overwrites a bound file, reporting the change instead of silently reloading it. Change events also invalidate the cached wallpaper when the changed path contains the active wallpaper. The broadcast is sent only after the filesystem operation succeeds.

### 3. Rendering

- The entire environment is rendered inside a single SDL2 window. The shell uses a runtime logical desktop size (1280 × 720 by default) and maps it to the host window; apps receive the resulting client geometry through `onResize`.
- The Window Manager is responsible for compositing window frames and delegating content drawing to apps.
- Rendering is clipped to the caller's renderer clip for the full WindowManager frame, then each app is additionally clipped to its window's client rectangle, so tiny or undersized app layouts cannot paint into title bars or the taskbar. Apps and shell overlays that use narrower internal clips must intersect and restore the caller clip; Browser, Settings, Terminal, Text Editor, Drawing, title bars, taskbar buttons, and Alt+Tab follow this rule explicitly. Client rectangles may be zero-sized on an undersized desktop, but are never negative.
- Rendering uses SDL2's accelerated renderer with VSYNC; apps draw text via SDL_ttf and primitives via SDL draw calls.

### 4. Input System

- All input enters through the main SDL2 event loop.
- **Shell hotkeys** are handled first (and not forwarded to apps): **Alt+Tab** / **Alt+Shift+Tab** cycles focused windows (minimized ones restore; the shell consumes the matching Tab and Alt releases after the title overlay closes); **Ctrl+Escape** toggles the Start menu and consumes the matching Escape release.
- The Start menu is modal: opening it sends `onFocusLost` to the previously focused visible app and suppresses client keyboard input, while closing it restores `onFocusGained` only when that same app still owns focus. Host focus loss while the menu is open does not resume the app until host focus returns.
- The Window Manager performs hit testing to determine which window (and which part of the window) should receive the event.
- Screen-space mouse events are converted to logical desktop pixels once at the shell boundary before window hit testing, drag/resize math, or client-area forwarding.
- A client that receives a left-button press keeps receiving matching motion and release events until that button is released, even if the pointer leaves the window or focus changes. A press handled by the desktop or a window frame is never followed by a synthetic client release. This keeps drag interactions such as Drawing strokes from getting stuck without leaking releases into another app.
- Taskbar and Start-menu left-button presses use a separate shell capture, so their release is consumed by the shell and never appears as an orphaned client mouse-up.
- Shell and window-frame presses also suppress client motion until a client owns a press; dragging a title bar or moving across the desktop cannot inject hover motion into an app.
- If the host SDL window loses focus during a drag, the shell synthesizes the captured client release, clears frame capture, and sends focus callbacks; regaining host focus restores the focused app callback without reviving stale pointer state.
- The shell records the latest pointer position from motion and button events, so wheel routing does not reuse a stale position after a release outside a client.
- Window frame interactions (dragging, resizing, buttons) are handled by the Window Manager.
- Client area events are forwarded to the active application.

### 5. Filesystem

The filesystem is **basic** by design: hierarchical virtual paths, simple CRUD operations, and host-backed persistence under `~/.monolith/fs/`. Regular file writes use validated temporary siblings and atomic replacement, while preserving existing permission bits and in-root file symlink targets. Terminal, Text Editor, Filesystem Browser, and Drawing all share the same `Filesystem` API, including shared **recursive** helpers (`copyRecursive`, `removeRecursive`, `isSameOrDescendant`, `join`) so apps do not reimplement tree walks.

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

**Close note:** Before destroying a window, the shell calls `App::allowClose()`. Apps may return false once to warn about unsaved work (second close discards). Start menu Shut Down and the host window's `SDL_QUIT` event both call the same contract on every open app in each request, so multiple dirty documents are warned together before the process can exit. Default is always allow.

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
