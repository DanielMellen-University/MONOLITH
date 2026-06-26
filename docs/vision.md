# Monolith Vision

> This document contains the original vision and philosophy for Monolith.
> It is kept for reference. For practical information about building and running the project,
> see the main [README.md](../README.md), the [documentation hub](README.md), and [architecture.md](architecture.md).

---

# MONOLITH

**A personal mini operating system that runs as one application.**

Monolith is a single Linux program that contains its own little world. It has a filesystem, a custom programming language, a terminal, and multiple built-in apps. You can keep adding new apps and features to it over the years.

It takes some inspiration from TempleOS - the idea of a personal, direct, self-contained machine that you live inside - but it is much easier because it runs as a normal application on Linux instead of being a full operating system.

You don't use Monolith like a regular program. You enter it.

## What Monolith Is

- One executable that feels like a small operating system
- Everything important lives inside it (filesystem, apps, language, settings)
- Built primarily as a personal machine for one person
- Designed to grow slowly and deliberately over a long time
- Keyboard-driven and direct

## What Monolith Is Not

- Not a real operating system
- Not primarily for other people
- Not trying to be a modern productivity tool or creative suite
- Not bare metal

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
| Planned | IDE | — |
| Planned | Wallpaper | — |
| Planned | Games | — |

New apps can be added over time, either as native features or written in the custom language.

## The Language

Monolith has its own custom programming language.

The language sits at a **medium** level of power:
- It includes graphics and drawing primitives
- It can generate and play sound
- It can interact with the filesystem and other parts of the system
- It is powerful enough to write real tools, apps, and automations that run inside Monolith

The goal is to have a language that feels good to use for extending the system, without making the language itself overwhelmingly complex to implement and maintain.

## Core Principles

- Everything lives inside one program
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

The project will be built in phases, roughly in this order:

1. **Core Foundation**
   - Basic window and input
   - Internal filesystem
   - Terminal app
   - Simple text editor

2. **The Language**
   - Custom language interpreter
   - Ability to run programs from the terminal and editor
   - Basic graphics and sound support in the language

3. **More Apps**
   - Simple IDE experience for the language
   - Drawing program
   - Wallpaper and appearance customization
   - Settings panel

4. **Growth**
   - Games and other personal apps
   - Deeper integration between apps
   - Whatever feels worth adding over time

The priority is to make the environment feel alive and usable early, even if many apps are still missing.

## Final Note

Monolith is intended as a long-term personal project. The aim is to build something deliberately over time rather than trying to create a complete product quickly.