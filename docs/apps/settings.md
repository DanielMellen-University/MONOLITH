# Settings App

The Settings app shows environment information and exposes a small set of live desktop preferences. It is the implementation behind the Start menu **Settings** entry.

## Launching

Open **Settings** from the Start menu. Multiple instances are supported:

- `Settings`
- `Settings 2`
- `Settings 3`

## Appearance

The **APPEARANCE** section at the top lets you change live desktop preferences.

### Desktop background

- Six preset swatches: Default, Deep Blue, Slate, Forest, Wine, and Teal.
- Select a preset to apply it immediately behind all windows (and under any wallpaper image).
- The active swatch is highlighted with a white border.

### Wallpaper image

- Enter a virtual filesystem path to a **BMP** file (for example `/Wallpapers/sample.bmp`) and press **Set**.
- **Clear** removes the image and returns to solid color only.
- Empty path means solid color only. Missing or unloadable files fail soft (solid color stays).
- A sample BMP is seeded at `/Wallpapers/sample.bmp` on first launch (from `assets/wallpapers/sample.bmp`).
- The image is cover-scaled to fill the logical desktop.

### Taskbar clock

- Two options: **12-hour** (default) and **24-hour**.
- The active option is highlighted with a white border, matching the swatch pattern.
- Changing the format updates the taskbar clock immediately.

Scroll with the mouse wheel or Page Up/Down if the window is resized smaller. Background, wallpaper path, and clock choices are saved to `~/.monolith/desktop_settings.txt` and restored on the next launch.

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

- Desktop background color uses six presets only (no custom RGB picker).
- Wallpaper images are BMP-only (`SDL_LoadBMP`; no SDL_image / PNG / JPEG yet).
- Path entry is typed (no full file picker dialog yet).
- Session restore and other shell prefs are not controlled from Settings (session is automatic via `~/.monolith/session.txt`).
- Other preferences (keybindings, default paths, taskbar style) are not exposed yet.
- Shut Down remains a separate Start menu item.

## Developer Notes

Main implementation files:

- `src/app/SettingsApp.hpp`
- `src/app/SettingsApp.cpp`
- `src/settings/DesktopSettings.hpp` / `.cpp` — load/save host settings file (`wallpaper_path=`)
- `src/window/WindowManager.cpp` — owns live settings, wallpaper texture load/paint, `loadDesktopSettings()`, `setDesktopBackground()`, `setWallpaperPath()`, `setClock24Hour()`
- `src/app/App.hpp` — `IWindowController` desktop color / wallpaper path / clock helpers
- `src/main.cpp` — loads settings at startup, clears solid background, seeds sample wallpaper

Settings changes go through `IWindowController` so the app does not reach into WindowManager internals directly.

