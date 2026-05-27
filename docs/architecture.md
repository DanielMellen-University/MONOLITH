# Monolith Architecture

## Overview

Monolith is a single Linux application that contains a complete, self-contained desktop-like environment. It is designed to feel like a small personal operating system that you "enter," while remaining a normal application on the host.

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
│  - Terminal, Filesystem, Text Editor, Drawing, Settings...   │
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
- Windows can be minimized and restored via the taskbar.
- No persistence of window position or size between sessions.
- No snapping or automatic tiling.

**Design Notes:**
The Window Manager should be relatively self-contained. Individual apps should not need to know how window frames are drawn or how input is routed.

### 2. Application Model

Core applications are written in **native C++**.

Each app runs inside a window provided by the Window Manager. The app is primarily responsible for:
- Rendering its content area (the area inside the window frame)
- Handling input events delivered to its client area
- Managing its own internal state

The Window Manager handles the frame, decorations, and top-level input routing.

### 3. Rendering

- The entire environment is rendered inside a single SDL2 window.
- The Window Manager is responsible for compositing window frames and delegating content drawing to apps.
- Early versions will likely use software rendering or simple OpenGL for drawing primitives and text.

### 4. Input System

- All input enters through the main SDL2 event loop.
- The Window Manager performs hit testing to determine which window (and which part of the window) should receive the event.
- Window frame interactions (dragging, resizing, buttons) are handled by the Window Manager.
- Client area events are forwarded to the active application.

### 5. Filesystem

The filesystem is **basic** by design.

**Current understanding:**
- Hierarchical directory structure
- Simple file operations (create, read, write, delete, list)
- Persisted on the host filesystem (likely as a directory tree inside the Monolith data folder)
- Used by both native apps and the scripting language

Advanced features (permissions, metadata, versioning, etc.) are explicitly out of scope for the foreseeable future.

### 6. Language Runtime

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

- Detailed Window Manager design (data structures, rendering of frames, input routing)
- Filesystem implementation model (how files are stored on disk)
- Core application lifecycle and how apps register with the Window Manager
- Rendering strategy and text/graphics primitives

---

*This document reflects the current understanding as of the latest design discussion. It will evolve as more decisions are made.*