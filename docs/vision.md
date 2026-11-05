# Vision

MONOLITH is a finished personal mini-OS demo for a portfolio. It is one Linux program: a small desktop inside a normal application. It is not a machine you boot.

## What it is

- A C++23 program. SDL2 provides the window, input, and drawing. SDL2_ttf provides text.
- A window manager in a fixed 1280x720 window: overlapping windows, a taskbar, a Start menu, desktop icons, Alt+Tab, and Ctrl+Escape.
- Nine built-in apps: Terminal, Text Editor, Filesystem, Drawing, Settings, Snake, Minesweeper, Pong, and Breakout. The four games sit under Games in the Start menu.
- A host-backed filesystem. Apps use virtual paths such as `/home/monolith/notes.txt`. Those are ordinary files under `~/.monolith/fs/` on the host. The session, desktop settings, and the Snake and Minesweeper records sit beside that tree, in `~/.monolith/`.

## What it is not

- Not an operating system kernel. It does not boot hardware, replace Linux, or manage devices. It is a normal Linux application.
- Not a Linux desktop clone, a productivity suite, or a professional drawing program. The app set is frozen at nine.
- Not where the programming language lives. The language is [Relic](https://github.com/DanielMellen-University/Relic). The successor desktop, written in Relic, is [M0N0LITH-2](https://github.com/DanielMellen-University/M0N0LITH-2) (MONOLITH 2). Both are separate repositories. This repository stays the C++ demo.

## Why it has this shape

The aim was a desktop one person could build and then open: windows, a terminal, an editor, files, a sketchpad, and a few games. TempleOS was part of the reference, a small self-contained machine, without writing a real operating system. Saves are atomic. An editor or a drawing with unsaved work asks before it closes. Apps talk to the shell through one interface and do not call each other.

That scope is closed. Version 1.0.2 (November 2026) is the current release. The 1.0 milestone shipped in October 2026. Later desktop and language work is in M0N0LITH-2 and Relic, not here. See the [roadmap](ROADMAP.md).
