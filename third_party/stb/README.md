# stb_image

MONOLITH uses `stb_image.h` (v2.30 at the pinned hash) from https://github.com/nothings/stb to decode PNG and JPEG wallpapers. BMP wallpapers use SDL's built-in `SDL_LoadBMP`.

## Why stb_image

`SDL_image` is not part of the build. `stb_image` is a single header (public domain or MIT) that decodes PNG and JPEG without another system package.

## How it gets into the build

The header is not stored in the repository.

- The CMake target `monolith_stb_image` runs `amalgamate.py`.
- `amalgamate.py` downloads `stb_image.h` from the pinned upstream commit `5c205738c191bcb0abc65c4febfa9bd25ff35234` and writes it to `build/generated/stb_image.h`.
- The download must match the SHA-256 pinned in `CMakeLists.txt` (`594c2fe35d49488b4382dbfaec8f98366defca819d916ac95becf3e75f4200b3`). Otherwise the build fails.

The first build therefore needs network access. To update the header, review the new upstream version, then change both `STB_COMMIT` in `amalgamate.py` and the pinned SHA-256 in `CMakeLists.txt`.

`third_party/stb/stb_image.h` in this folder is a placeholder containing only an `#error`, so including it by mistake stops the build.

`src/window/WallpaperImage.cpp` is the only file that defines `STB_IMAGE_IMPLEMENTATION`.
