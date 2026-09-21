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

- Enter a virtual filesystem path to a **BMP**, **PNG**, or **JPEG** file (for example `/Wallpapers/sample.png`) and press **Set**.
- While the path field is focused, Left/Right/Home/End move the caret, typed text is inserted at the caret, Delete removes the next complete UTF-8 character, and Backspace removes the previous one.
- Press Tab to complete a directory or image filename (`.bmp`/`.png`/`.jpg`/`.jpeg`). With several matches, completion extends the shared prefix; the caret must be in the final path component.
- Ambiguous completion stops at a complete UTF-8 codepoint, so filenames that share only leading bytes cannot insert an invalid partial character.
- Long paths scroll horizontally to keep the caret visible while editing.
- If a configured wallpaper path moves while the field is focused, the prompt follows the move and preserves the caret's suffix position on a UTF-8 boundary.
- **Clear** removes the image and returns to solid color only.
- Empty path means solid color only. Missing or unloadable files fail soft (solid color stays).
- Sample wallpapers are seeded at `/Wallpapers/sample.bmp` and `/Wallpapers/sample.png` on first launch (from `assets/wallpapers/`).
- By default the image is cover-scaled to fill the logical desktop.
- Renaming or moving the configured wallpaper in Filesystem Browser or Terminal updates the setting and persists the new virtual path. If the wallpaper field is being edited at the same time, its prompt follows the move too.
- Deleting the configured wallpaper clears the setting and persists solid-color mode. An active wallpaper path prompt is cleared if its file or parent directory is deleted.

### Wallpaper fit

- Three options: **Cover** (default), **Contain**, and **Center**.
- The active option is highlighted with a white border, matching the clock and text-size controls.
- **Cover** fills the desktop and crops overflow.
- **Contain** keeps the entire image visible and letterboxes or pillarboxes with the solid desktop background.
- **Center** draws the image at natural 1:1 size, centered, clipping if it is larger than the desktop.
- Changing the fit updates the desktop immediately and persists as `wallpaper_fit=` in `desktop_settings.txt`. Unsupported values coerce to cover.

### Taskbar clock

- Two options: **12-hour** (default) and **24-hour**.
- The active option is highlighted with a white border, matching the swatch pattern.
- Changing the format updates the taskbar clock immediately.

### Interface text size

- Three options: **Small (90%)**, **Default (100%)**, and **Large (115%)**. These are the only supported persisted values.
- The active option is highlighted with a white border.
- Changing the size updates shared app and window text immediately, and open text-heavy apps keep their cursor and scroll views within the new font geometry.
- Settings section spacing, wallpaper fields, clock/scale controls, and footer height grow from the active font metrics, keeping labels inside their controls at the supported 115% scale.
- The wallpaper path field gives up width before the Set and Clear buttons do, so the action controls remain inside narrow Settings clients.
- Cached swatch, wallpaper-fit, clock, text-size, and wallpaper-control hit rectangles are cleared immediately when the client resizes, the interface scale changes, or the scroll offset moves, then rebuilt on demand for input or during the next render so queued events use the current layout. The cached rectangles use the same scrolled client coordinates as the drawn controls.
- Settings preserves the shell's renderer clip while applying its scroll-area clip, so a partially visible window cannot paint outside its client intersection.
- The footer is clamped inside the client rectangle when a Settings window is shorter than the scaled footer band.

Scroll with the mouse wheel or Page Up/Down if the window is resized smaller. Background, wallpaper path, wallpaper fit, clock, and interface text choices are saved to `~/.monolith/desktop_settings.txt` through a temporary sibling and atomically replaced after the complete write, then restored on the next launch. Malformed or unsupported persisted values are ignored so defaults remain intact (unsupported wallpaper fit coerces to cover); omitted values in a valid legacy file use their defaults.

The settings file accepts both Unix and Windows line endings, so copying it between systems does not add a hidden carriage return to a wallpaper path or other value.

Wallpaper paths are canonicalized when settings load, so legacy values containing repeated separators or `.` and `..` segments are rewritten to their normalized virtual path before use.

## Information Panel

Below the appearance controls, Settings shows read-only details:

**About**
- Monolith name and version
- Engine (SDL2 + custom window manager)

**Environment**
- Current logical desktop size, excluding host-window scaling
- Filesystem host root on disk (e.g. `~/.monolith/fs/`)
- Virtual home path (`/home/monolith`)

**Notes**
- Development status reminder
- Hint to use the Start menu or taskbar to launch apps

Long information labels and values stay at their normal text size. If a Settings window is too narrow to show all of a string, the excess is clipped at the panel boundary instead of being stretched.

## Current Limitations

- Desktop background color uses six presets only (no custom RGB picker).
- Wallpaper images: BMP via `SDL_LoadBMP`; PNG/JPEG via build-time `stb_image` (`WallpaperImage`). No `SDL_image` package.
- Wallpaper fit is cover, contain, or center only (no custom crop/zoom UI yet).
- Path entry is typed with Tab completion (no full file picker dialog yet).
- Session restore and other shell prefs are not controlled from Settings (session is automatic via `~/.monolith/session.txt`).
- Other preferences (keybindings, default paths, taskbar style) are not exposed yet.
- Shut Down remains a separate Start menu item.

## Developer Notes

Main implementation files:

- `src/app/SettingsApp.hpp`
- `src/app/SettingsApp.cpp`
- `src/settings/DesktopSettings.hpp` / `.cpp` - load/save host settings file (`wallpaper_path=`, `wallpaper_fit=`, `ui_scale_percent=`)
- `src/window/WindowManager.cpp` - owns live settings, wallpaper texture load/paint, shared font sizing, `loadDesktopSettings()`, `setDesktopBackground()`, `setWallpaperPath()`, `setWallpaperFit()`, `setClock24Hour()`, `setUiScalePercent()`
- `src/app/App.hpp` - `IWindowController` desktop color / wallpaper path / wallpaper fit / clock / UI scale helpers
- `src/main.cpp` - loads settings at startup, clears solid background, seeds sample wallpaper

Settings changes go through `IWindowController` so the app does not reach into WindowManager internals directly.

Settings caches renderer-independent SDL_ttf surfaces for its repeated labels, option text, information lines, and footer. The cache is cleared when the shared interface scale or client size changes, when the wallpaper path changes, and while the wallpaper field is actively rendered so caret variants cannot accumulate. Per-frame SDL textures remain short-lived and are created from the cached surfaces.
