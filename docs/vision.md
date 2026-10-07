# Monolith Vision

> This document contains the original vision and philosophy for Monolith.
> It is kept for reference. For practical information about building and running the project,
> see the main [README.md](../README.md), the [documentation hub](README.md), [architecture.md](architecture.md),
> and the active [1.0 roadmap](ROADMAP.md).

---

# MONOLITH

**A personal mini operating system that runs as one application.**

Monolith is a single Linux program that contains its own little world. It has a filesystem, a terminal, and multiple built-in apps. You can keep adding new native apps and features to it over the years.

It takes some inspiration from TempleOS - the idea of a personal, direct, self-contained machine that you live inside - but it is much easier because it runs as a normal application on Linux instead of being a full operating system.

You don't use Monolith like a regular program. You enter it.

## What Monolith Is

- One executable that feels like a small operating system
- Everything important for daily use lives inside it (filesystem, apps, settings)
- Built primarily as a personal machine for one person
- Designed to grow slowly and deliberately over a long time
- Keyboard-driven and direct

## What Monolith Is Not

- Not a real operating system
- Not primarily for other people
- Not trying to be a modern productivity tool or creative suite
- Not bare metal
- Not the home of a custom programming language (that is a separate project; see below)

---

## Built-in Apps

Monolith is meant to contain several distinct apps and subsystems. Implemented apps have user guides under `docs/apps/`:

| Status | App | Guide |
|--------|-----|-------|
| Shipped | Terminal | [apps/terminal.md](apps/terminal.md) |
| Shipped | Filesystem Browser | [apps/filesystem-browser.md](apps/filesystem-browser.md) |
| Shipped | Text Editor | [apps/text-editor.md](apps/text-editor.md) |
| Shipped | Drawing | [apps/drawing.md](apps/drawing.md) |
| Shipped | Settings | [apps/settings.md](apps/settings.md) |
| Shipped | Snake | [apps/snake.md](apps/snake.md) |
| Shipped | Minesweeper | [apps/minesweeper.md](apps/minesweeper.md) |
| Shipped | Pong | [apps/pong.md](apps/pong.md) |
| Shipped | Breakout | [apps/breakout.md](apps/breakout.md) |
| Shipped (partial) | Wallpaper images (BMP/PNG/JPEG via Settings; solid color fallback) | [apps/settings.md](apps/settings.md) |

New native apps can still be added after 1.0 when they earn a place. Settings for the 1.0 tag ships as-is (no open-ended appearance expansion before the release).

## Custom language (not this repository)

A custom programming language and IDE were part of the original long-range sketch (old Phase 5 / Phase 6). That work is **out of this repository entirely**.

- **MONOLITH 1.0** (this repo) is the personal mini-OS: shell, apps, VFS, stability, polish.
- **MONOLITH 2** is a **separate repository**, built in Daniel's own custom language. It is not a "2.0" tag of this tree.

Do not plan language or IDE chunks against this codebase.

## Core Principles

- Everything for daily use lives inside one program
- It is *your* machine first
- Constraints are intentional
- The system should feel solid and direct
- It should be possible to keep expanding it for years without it becoming a mess

## Technical Direction

- Written in C++ and runs on modern Linux
- Uses SDL2 for windowing, input, graphics, and audio
- Self-contained (single executable + supporting files)
- Has its own internal filesystem that lives on the host machine

## Roadmap

Active release planning lives in [ROADMAP.md](ROADMAP.md) (MONOLITH 1.0 portfolio track).

Historical build order (mostly complete for this repo):

1. **Core Foundation** - window, filesystem, Terminal, text editor (done)
2. **Desktop shell** - multi-instance titles, session restore, Alt+Tab, taskbar clock (done)
3. **Native apps** - Drawing, Settings, games, wallpaper (done through Phase 7 growth work)
4. **1.0 packaging** - surface freeze, stability bar, version/tag (in progress; see roadmap)

Language / IDE phases from the old plan are retired here and moved to the separate MONOLITH 2 repo.

## Final Note

Monolith is intended as a long-term personal project. The near-term bar is a complete 1.0 you can show; further native growth can continue after that without turning this tree into a language testbed.
