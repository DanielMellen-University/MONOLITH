# stb_image (build-time fetch)

Source: https://github.com/nothings/stb (`stb_image.h`, v2.30)

## Why this dependency

MONOLITH wallpaper decode for PNG/JPEG (slice 7.1) needs an image decoder.
`SDL_image` is not in the CMake build, and the project playbook avoids new packaged
SDL deps unless requested. `stb_image` is a single-header public-domain/MIT library
that loads PNG and JPEG with no pkg-config packages. BMP wallpaper keeps using
`SDL_LoadBMP`.

## Layout

CMake downloads the pinned upstream header into `build/generated/stb_image.h`
via `third_party/stb/amalgamate.py` and verifies SHA-256
`594c2fe35d49488b4382dbfaec8f98366defca819d916ac95becf3e75f4200b3`.

Define `STB_IMAGE_IMPLEMENTATION` in exactly one translation unit before including
`stb_image.h`.
