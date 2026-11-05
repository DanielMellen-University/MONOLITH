# Vision

MONOLITH is a small personal computer that lives inside one Linux program. It has its own desktop, its own apps and its own filesystem. You open it and work inside it, the way you would sit down at a separate machine.

The idea comes partly from TempleOS: a direct, self-contained machine that one person owns completely. MONOLITH keeps that feeling without being a real operating system. It runs as an ordinary SDL2 application on Linux, which keeps it buildable and usable.

## What it is

- One executable that behaves like a small desktop OS.
- A place for everyday tools: a terminal, an editor, a file browser, a drawing app, settings and a few games.
- A personal machine, built for one user first.
- Keyboard-friendly. Every app has shortcuts, and the shell has Alt+Tab and Ctrl+Escape.

## What it is not

- Not an operating system kernel. It is a normal Linux application.
- Not a clone of a Linux desktop environment.
- Not a productivity suite or a professional creative tool.
- Not the home of a programming language. The language is [Relic](https://github.com/DanielMellen-University/Relic). The successor desktop is [M0N0LITH-2](https://github.com/DanielMellen-University/M0N0LITH-2) (MONOLITH 2), in its own repository.

## Principles

- **Everything lives inside.** Files, settings and apps are part of MONOLITH, stored under `~/.monolith/`.
- **Constraints are deliberate.** A fixed 1280x720 desktop, a small set of apps, a simple file format for drawings. Fewer features done properly beats many done halfway.
- **Solid and direct.** Saves are atomic, unsaved work is never discarded without an explicit choice, and the shell recovers cleanly from odd input.
- **Clear boundaries.** Apps talk to the shell through one interface and never to each other, so new apps can be added without touching old ones.

## Technical direction

- C++23 on Linux, using SDL2 for the window, input and drawing, and SDL2_ttf for text.
- One executable plus a bundled font.
- Its own filesystem, stored as ordinary files on the host.

## Where it stands

Version 1.0.2 (November 2026) is the current release: the desktop shell, nine apps, the internal filesystem, and a headless test suite that runs normally and under sanitizers in CI. The 1.0 portfolio milestone shipped in October 2026. See the [roadmap](ROADMAP.md) for what it includes.
