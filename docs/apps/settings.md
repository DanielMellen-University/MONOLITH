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
- While the path field is focused, Left/Right/Home/End move the caret, typed text is inserted at the caret, Delete removes the next complete UTF-8 character, and Backspace removes the previous one.
- Press Tab to complete a directory or BMP filename. With several matches, completion extends the shared prefix; the caret must be in the final path component.
- Long paths scroll horizontally to keep the caret visible while editing.
- **Clear** removes the image and returns to solid color only.
- Empty path means solid color only. Missing or unloadable files fail soft (solid color stays).
- A sample BMP is seeded at `/Wallpapers/sample.bmp` on first launch (from `assets/wallpapers/sample.bmp`).
- The image is cover-scaled to fill the logical desktop.
- Renaming or moving the configured wallpaper in Filesystem Browser or Terminal updates the setting and persists the new virtual path. If the wallpaper field is being edited at the same time, its prompt follows the move too.
- Deleting the configured wallpaper clears the setting and persists solid-color mode. An active wallpaper path prompt is cleared if its file or parent directory is deleted.

### Taskbar clock

- Two options: **12-hour** (default) and **24-hour**.
- The active option is highlighted with a white border, matching the swatch pattern.
- Changing the format updates the taskbar clock immediately.

### Interface text size

- Three options: **Small (90%)**, **Default (100%)**, and **Large (115%)**. These are the only supported persisted values.
- The active option is highlighted with a white border.
- Changing the size updates the shared app and window text immediately.

Scroll with the mouse wheel or Page Up/Down if the window is resized smaller. Background, wallpaper path, clock, and interface text choices are saved to `~/.monolith/desktop_settings.txt` and restored on the next launch. Malformed or unsupported persisted values are ignored so defaults remain intact; omitted values in a valid legacy file use their defaults.

The settings file accepts both Unix and Windows line endings, so copying it between systems does not add a hidden carriage return to a wallpaper path or other value.

Wallpaper paths are canonicalized when settings load, so legacy values containing repeated separators or `.` and `..` segments are rewritten to their normalized virtual path before use.

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

Long information labels, values, and the footer stay at their normal text size. If a Settings window is too narrow to show all of a string, the excess is clipped at the panel boundary instead of being stretched.

## Current Limitations

- Desktop background color uses six presets only (no custom RGB picker).
- Wallpaper images are BMP-only (`SDL_LoadBMP`; no SDL_image / PNG / JPEG yet).
- Path entry is typed with Tab completion (no full file picker dialog yet).
- Session restore and other shell prefs are not controlled from Settings (session is automatic via `~/.monolith/session.txt`).
- Other preferences (keybindings, default paths, taskbar style) are not exposed yet.
- Shut Down remains a separate Start menu item.

## Developer Notes

Main implementation files:

- `src/app/SettingsApp.hpp`
- `src/app/SettingsApp.cpp`
- `src/settings/DesktopSettings.hpp` / `.cpp` — load/save host settings file (`wallpaper_path=`, `ui_scale_percent=`)
- `src/window/WindowManager.cpp` — owns live settings, wallpaper texture load/paint, shared font sizing, `loadDesktopSettings()`, `setDesktopBackground()`, `setWallpaperPath()`, `setClock24Hour()`, `setUiScalePercent()`
- `src/app/App.hpp` — `IWindowController` desktop color / wallpaper path / clock / UI scale helpers
- `src/main.cpp` — loads settings at startup, clears solid background, seeds sample wallpaper

Settings changes go through `IWindowController` so the app does not reach into WindowManager internals directly.
