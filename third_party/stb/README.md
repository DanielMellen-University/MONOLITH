# stb_image (vendored)

Source: https://github.com/nothings/stb (`stb_image.h`, v2.30)

## Why this dependency

MONOLITH wallpaper decode for PNG/JPEG (slice 7.1) needs an image decoder.
`SDL_image` is not in the CMake build, and the project playbook avoids new packaged
SDL deps unless requested. `stb_image` is a single-header public-domain/MIT library
that loads PNG and JPEG with no pkg-config packages. BMP wallpaper keeps using
`SDL_LoadBMP`.

## Layout

- `stb_image_b64_XX.txt` - zlib-compressed base64 fragments of upstream `stb_image.h`
  (decoded by `amalgamate.py`).
- `amalgamate.py` - decodes and writes the amalgamated header.
- CMake writes `build/generated/stb_image.h` at configure/build time and adds that
  directory to the include path.

Define `STB_IMAGE_IMPLEMENTATION` in exactly one translation unit before including
`stb_image.h`.
