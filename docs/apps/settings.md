# Settings

Desktop preferences that apply immediately and are kept for the next launch, plus a read-only information panel.

## Using it

Open **Settings** from the Start menu or desktop. You can open several windows (`Settings`, `Settings 2`, ...); they all change the same settings. Each change applies to the whole desktop at once. The selected option in each group has a white border.

The **APPEARANCE** section has five groups.

### Desktop background

Six preset colors:

| Preset | RGB |
|--------|-----|
| Default | 25,25,30 |
| Deep Blue | 18,24,42 |
| Slate | 32,36,48 |
| Forest | 20,32,24 |
| Wine | 36,20,28 |
| Teal | 16,28,32 |

The color shows behind the wallpaper image, or on its own when no image is set.

### Wallpaper image

Type a path in the internal filesystem to a `.bmp`, `.png`, `.jpg` or `.jpeg` file and press **Set** (or Enter). **Clear** removes the image.

- `~` means `/home/monolith`. Tab completes folders and image files.
- An empty path, a missing file or a file that cannot be decoded leaves the plain background color.
- Images over 16,777,216 pixels are refused.
- If you rename or move the image in Terminal or Filesystem, the setting follows it. If you delete it, the setting is cleared.
- `/Wallpapers/sample.bmp` is always available. `/Wallpapers/sample.png` exists only if `assets/wallpapers/sample.png` was present at launch; run `python3 scripts/gen_sample_wallpaper.py` to create both sample files. See [wallpaper.md](../internals/wallpaper.md#sample-wallpapers).

### Wallpaper fit

| Option | Result |
|--------|--------|
| Cover (default) | Fills the desktop, cropping the overflow |
| Contain | Shows the whole image with background-color bars |
| Center | Shows the image at actual size, centered, clipped if larger |

### Taskbar clock

**12-hour** (default) or **24-hour**.

### Interface text size

**Small (90%)**, **Default (100%)** or **Large (115%)**. This changes the text in every app and window frame. Open apps keep their cursor and scroll position valid at the new size.

### Information panel

| Section | Shows |
|---------|-------|
| MONOLITH | Version (`1.0 (October 2026)`), Engine (`SDL2 + custom window manager`) |
| ENVIRONMENT | Logical desktop size, filesystem root on the host, virtual home (`/home/monolith`) |
| NOTES | Status (`Personal environment`) and a hint to use the Start menu or taskbar |

The footer normally reads `Changes take effect immediately.`

## Keyboard shortcuts

| Key | Where | Action |
|-----|-------|--------|
| Enter | Wallpaper field | Set the wallpaper |
| Esc | Wallpaper field | Leave the field and undo the unsaved edit |
| Tab | Wallpaper field | Complete the path |
| Left / Right, Home / End | Wallpaper field | Move the caret |
| Backspace / Delete | Wallpaper field | Delete a character |
| Page Up / Page Down, mouse wheel | Anywhere else | Scroll the window |
| Home / End | Anywhere else | Scroll to the top / bottom |

## Settings file

Settings are saved to `~/.monolith/desktop_settings.txt` with the [atomic writer](../internals/atomic-writes.md). One `key=value` per line:

| Key | Values |
|-----|--------|
| `desktop_background` | `r,g,b` |
| `wallpaper_path` | Virtual path, or empty |
| `wallpaper_fit` | `cover`, `contain` or `center` |
| `clock_24_hour` | `0` or `1` |
| `ui_scale_percent` | `90`, `100` or `115` |

- Unix and Windows line endings are both accepted.
- Unknown or malformed values are ignored and the default is used. An unknown fit becomes `cover`. Missing keys use their defaults.
- Wallpaper paths are normalized on load. They cannot contain line breaks or NUL bytes.
- The loader reads at most 64 records of 16 KiB. It stops at the first oversized record and keeps what it already read.
- If a save fails, the change still applies for this session and the footer reads `Change is live, but could not be saved.` Selecting the same option again retries the save.

## Limits

- Background color is one of the six presets; there is no custom color picker.
- Wallpaper fit is one of the three modes; no crop or zoom.
- The wallpaper path is typed, with Tab completion; there is no file picker.
- Session restore is automatic and has no setting.

## Developer notes

- `src/app/SettingsApp.{hpp,cpp}`: the app. Most of its body is stored compressed in `src/app/SettingsApp_body_*.inc.z64`; see [building.md](../development/building.md#generated-sources).
- `src/settings/DesktopSettings.{hpp,cpp}`: reading and writing the settings file.
- The window manager holds the live settings, loads the wallpaper and sizes the shared font. Settings changes them through `IWindowController` (`setDesktopBackground`, `setWallpaperPath`, `setWallpaperFit`, `setClock24Hour`, `setUiScalePercent`).
- Wallpaper decoding and drawing: [wallpaper.md](../internals/wallpaper.md).
- Tests: `test_settings_app_state`, `test_desktop_settings`.
