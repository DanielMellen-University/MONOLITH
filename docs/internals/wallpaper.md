# Wallpaper internals

User-facing behavior is in [settings.md](../apps/settings.md#wallpaper-image). This page covers how images are loaded and drawn.

## Decoding

- Supported: `.bmp`, `.png`, `.jpg`, `.jpeg` (case-insensitive).
- BMP is decoded by SDL (`SDL_LoadBMP`). PNG and JPEG are decoded by `stb_image` in `src/window/WallpaperImage.cpp`. `SDL_image` is not used.
- `stb_image.h` is not stored in the repo. CMake downloads it at build time with `third_party/stb/amalgamate.py` and checks its SHA-256; see [third_party/stb/README.md](../../third_party/stb/README.md).
- Every format is checked for size before pixels are decoded. Images over 16,777,216 pixels (for example 4096x4096) are rejected. For BMP the header is read from the same open file handle that SDL then decodes. PNG and JPEG are decoded straight from the host file without first copying it into memory.
- An empty path, a missing file or a file that fails to decode leaves the solid background color.

## Fit modes

The setting is stored as `wallpaper_fit=` in `desktop_settings.txt`.

| Mode | Drawing |
|------|---------|
| `cover` (default) | Scaled to fill the desktop, centered, overflow cropped |
| `contain` | Scaled so the whole image is visible; the background color fills the bars |
| `center` | Drawn at 1:1, centered, clipped if larger than the desktop |

Unknown values are treated as `cover`, both when loading and when setting. The image is clipped to the logical desktop, so `contain` and `center` never draw outside it. Drawing happens in `WindowManager::renderWallpaper` (`wm_body_08b.inc`), before desktop icons and windows.

## Reloading

- A path that failed to load is remembered so the shell does not retry every frame.
- That memory is cleared, and the image is retried on the next frame, when the configured file or one of its parent folders is created, when a move delivers a file to that path, or when the file changes.
- Renaming or moving the configured image in Terminal or Filesystem updates the stored path. Deleting it clears the setting.

## Sample wallpapers

On every launch MONOLITH creates `/Wallpapers/sample.bmp` if it is missing: it copies `assets/wallpapers/sample.bmp` (or `../assets/wallpapers/sample.bmp`, relative to the current directory) when present, otherwise it writes a 64x48 gradient BMP generated in code.

`/Wallpapers/sample.png` is created the same way from `assets/wallpapers/sample.png` (or `../assets/wallpapers/sample.png`), with no generated fallback. The code is in `src/detail/main_seed_png.inc`.

Both files are committed in `assets/wallpapers/` (64x48 gradients, 9,270 and 2,966 bytes). They were made with `python3 scripts/gen_sample_wallpaper.py`, which writes identical bytes on every run; rerunning it is optional.

## Tests

- `scripts/test_wallpaper_image.cpp`: BMP and PNG decoding, rejection of oversized images before decoding, unsupported extensions.
- `scripts/test_desktop_settings.cpp`: persistence of `wallpaper_path` and `wallpaper_fit`.
- `scripts/test_wallpaper_fit_controller.cpp`: fit get/set through `IWindowController`. This one is not part of the headless suite; see [testing.md](../development/testing.md#tests-outside-the-suite).
