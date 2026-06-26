# Settings App

The Settings app shows environment information and exposes a small set of live desktop preferences. It is the implementation behind the Start menu **Settings** entry.

## Launching

Open **Settings** from the Start menu. Multiple instances are supported:

- `Settings`
- `Settings 2`
- `Settings 3`

## Appearance

The **APPEARANCE** section at the top lets you change the desktop background color.

- Six preset swatches: Default, Deep Blue, Slate, Forest, Wine, and Teal.
- Select a preset to apply it immediately behind all windows.
- Scroll with the mouse wheel or Page Up/Down if the window is resized smaller.
- The active swatch is highlighted with a white border.
- Your choice is saved to `~/.monolith/desktop_settings.txt` and restored on the next launch.

## Information Panel

Below the appearance controls, Settings shows read-only details:

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

## Current Limitations

- Only desktop background color is configurable so far (no custom RGB picker).
- No wallpaper image support yet.
- Other preferences (keybindings, default paths, taskbar style) are not exposed yet.
- Shut Down remains a separate Start menu item.

## Developer Notes

Main implementation files:

- `src/app/SettingsApp.hpp`
- `src/app/SettingsApp.cpp`
- `src/settings/DesktopSettings.hpp` / `.cpp` — load/save host settings file
- `src/window/WindowManager.cpp` — owns live settings, `loadDesktopSettings()`, `setDesktopBackground()`
- `src/app/App.hpp` — `IWindowController::get/setDesktopBackgroundColor()`
- `src/main.cpp` — loads settings at startup and uses them when clearing the desktop

Settings changes go through `IWindowController` so the app does not reach into WindowManager internals directly.