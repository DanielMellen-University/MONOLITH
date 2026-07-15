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
┌─────────────────────────────────────────────────────────────┐
│                        Monolith (SDL2)                       │
├─────────────────────────────────────────────────────────────┤
│  Window Manager                                              │
│  - Title bars, 3 buttons (close/minimize/maximize)           │
│  - Dragging, resizing, z-order, focus                        │
│  - Taskbar                                                   │
├─────────────────────────────────────────────────────────────┤
│  App Host / Client Areas                                     │
│  - Native C++ applications render into their window content  │
├─────────────────────────────────────────────────────────────┤
│  Built-in Apps (native)                                      │
│  - Terminal, Filesystem, Editor, Drawing, Settings, Snake, Minesweeper │
├─────────────────────────────────────────────────────────────┤
│  Basic Filesystem                                            │
│  - Hierarchical, persisted on host disk                      │
├─────────────────────────────────────────────────────────────┤
│  Language Runtime (scripting/automation)                     │
│  - Used mainly from Terminal and other apps                  │
└─────────────────────────────────────────────────────────────┘
```

## Core Components

### 1. Window Manager

The Window Manager is the most foundational subsystem.

**Responsibilities:**
- Owns all top-level windows
- Draws window frames (title bar + three buttons)
- Handles window dragging and resizing
- Manages z-order and focus
- Provides a taskbar for window switching and status
- Routes input events to the correct window

**Window Model:**
- Every window has a title bar with a title and three standard buttons.
- Windows support dragging by the title bar.
- Windows support resizing from edges and corners.
- Windows can be minimized via the title-bar minimize button or by clicking the active window's taskbar button (XP-style toggle), and restored by clicking its taskbar entry.
- When many windows are open, the taskbar scrolls horizontally (arrow buttons and mouse wheel).
- No persistence of window position or size between sessions.
- No snapping or automatic tiling.

**Design Notes:**
The Window Manager should be relatively self-contained. Individual apps should not need to know how window frames are drawn or how input is routed.

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

The Window Manager also acts as a small "desktop shell". It provides launcher methods (`launchTerminal()`, `launchTextEditor(path)`, `launchFilesystem()`, `launchDrawing()`, `launchSettings()`, `launchSnake()`, `launchMinesweeper()`) used by the Start Menu and by apps.

The Start menu keeps most apps as top-level entries. Games that clearly form a group (**Snake**, **Minesweeper**) sit under a non-clickable **Games** category header with a slight indent; only categories that make sense are introduced this way.

Each frame, `WindowManager::update()` calls `App::update()` on every non-minimized window's app. Most apps leave this as a no-op; games use it for fixed-rate ticks and timers.

Apps can request shell actions on behalf of the user through `IWindowController` (currently `close()`, `setTitle()`, `restoreTrackedInstanceTitle()`, `openInTextEditor(virtualPath)`, `openInDrawing(virtualPath)`, editor/drawing file binding helpers, and desktop background get/set). This enables patterns like "double-click a file in the graphical filesystem browser to open it in the text editor or Drawing" without apps directly depending on each other. Apps that temporarily change the title (e.g. Drawing after save) use `restoreTrackedInstanceTitle()` to return to the WM-managed instance name.

### 3. Rendering

- The entire environment is rendered inside a single SDL2 window (currently fixed at 1280 × 720 logical pixels).
- The Window Manager is responsible for compositing window frames and delegating content drawing to apps.
- Rendering uses SDL2's accelerated renderer with VSYNC; apps draw text via SDL_ttf and primitives via SDL draw calls.

### 4. Input System

- All input enters through the main SDL2 event loop.
- The Window Manager performs hit testing to determine which window (and which part of the window) should receive the event.
- Window frame interactions (dragging, resizing, buttons) are handled by the Window Manager.
- Client area events are forwarded to the active application.

### 5. Filesystem

The filesystem is **basic** by design: hierarchical virtual paths, simple CRUD operations, and host-backed persistence under `~/.monolith/fs/`. Terminal, Text Editor, Filesystem Browser, and Drawing all share the same `Filesystem` API.

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

**Shell coordination:** Apps request desktop actions through `IWindowController` (close, set title, open file in editor, get/set desktop background color). The Filesystem Browser uses `openInTextEditor()` so it does not depend on the Text Editor directly. Settings uses `setDesktopBackgroundColor()` to change the live desktop color, persisted by the Window Manager to `~/.monolith/desktop_settings.txt`.

**Input note:** The Window Manager forwards `SDL_MOUSEBUTTONUP` to the focused app's client area so drag interactions (e.g. Drawing strokes) end cleanly when the mouse is released outside the window.

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
- No requirement for window state persistence across sessions (for now).
- The language is not required to build GUI applications directly.

## Next Areas to Explore

- Custom language interpreter and host bindings (Phase 2)
- IDE, wallpaper images, and more Settings preferences beyond desktop background color
- Shell coordination improvements (e.g. richer “open with” routing)
- Deeper app integration and additional native apps/games

Per-app limitations and planned work are tracked in each [app guide](README.md#built-in-apps).

---

*This document reflects the current understanding as of the latest design discussion. It will evolve as more decisions are made.*
