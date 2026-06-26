# Settings App

The Settings app is an informational panel showing version, environment, and filesystem details. It is the implementation behind the Start menu **Settings** entry.

## Launching

Open **Settings** from the Start menu. Multiple instances are supported:

- `Settings`
- `Settings 2`
- `Settings 3`

## What It Shows

The panel is read-only and displays:

**About**
- Monolith name and version
- Engine (SDL2 + custom window manager)

**Environment**
- Logical desktop size (1280 × 720)
- Filesystem host root on disk (e.g. `~/.monolith/fs/`)
- Virtual home path (`/home/monolith`)

**Notes**
- Development status reminder
- Hint to use the Start menu or taskbar to launch apps

A footer note reads: *"Changes take effect immediately where applicable."* This is placeholder text for future configurable settings.

## Interaction

There are no interactive controls yet. The panel does not accept keyboard or mouse input beyond normal window focus.

## Current Limitations

- No actual settings to change (appearance, keybindings, paths, etc.).
- No Shut Down configuration — Shut Down is a separate Start menu item.
- Information is hard-coded in `SettingsApp::buildInfoLines()`.

## Developer Notes

Main implementation files:

- `src/app/SettingsApp.hpp`
- `src/app/SettingsApp.cpp`
- `src/window/WindowManager.cpp` — `launchSettings()`

Future work: replace the static info panel with real preference controls while keeping the same launcher and window title pattern.