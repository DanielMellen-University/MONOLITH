# Building

## Requirements

| Tool | Version |
|------|---------|
| C++ compiler | C++23 (CI uses GCC 13 on Ubuntu 24.04) |
| CMake | 3.16 or newer |
| pkg-config | any |
| Python 3 | any recent version (used by build steps) |
| SDL2 | development package |
| SDL2_ttf | 2.0.18 or newer, development package |

Ubuntu, Debian or Pop!_OS:

```bash
sudo apt install build-essential cmake pkg-config python3 libsdl2-dev libsdl2-ttf-dev
```

The first build also needs network access to download `stb_image.h` (see below).

## Build

From the repository root:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
```

The result is `build/monolith`. The build uses `-Wall -Wextra -Wpedantic` and should produce no warnings.

## Run

```bash
./build/monolith
```

Run it from the repository root. Two things are looked up relative to the current directory:

- **Font:** `assets/fonts/DejaVuSans.ttf` (14 pt). If it is not found, MONOLITH tries `/usr/share/fonts/truetype/dejavu/`, `/usr/share/fonts/dejavu/` and `/usr/share/fonts/truetype/`. With no font at all, text does not render and a warning is printed.
- **Sample wallpapers:** `assets/wallpapers/sample.bmp` and `sample.png`, copied into `/Wallpapers/` whenever a copy is missing there. If the folder is not found, MONOLITH generates a small BMP sample and skips the PNG.

On launch MONOLITH creates `~/.monolith/fs/` (or `./monolith_fs` if `HOME` is not set). The host files it uses are listed in [architecture.md](../architecture.md#files-on-the-host).

## Generated sources

Some files are produced at build time into `build/generated/`. CMake does this automatically; you only need the commands below when editing these sources.

### Compressed source fragments

Two large source bodies are stored as zlib-compressed, base64-encoded text so they stay manageable for tooling that has file size limits:

| Tracked files | Decompressed to | Included by |
|---------------|-----------------|-------------|
| `src/main_body_*.inc.z64` | `build/generated/main/main_body_*.inc` | `src/main.cpp` |
| `src/app/SettingsApp_body_*.inc.z64` | `build/generated/settings/SettingsApp_body_*.inc` | `src/app/SettingsApp.cpp` |

CMake targets `monolith_main_bodies` and `monolith_settings_bodies` decompress them. CMake watches the `.z64` files, so adding or removing one triggers a reconfigure.

To edit one:

```bash
# 1. Decompress (CMake also does this during the build)
python3 src/decompress_main_bodies.py build/generated/main
python3 src/app/decompress_settings_bodies.py build/generated/settings

# 2. Edit the .inc files under build/generated/

# 3. Compress back into the tracked files
python3 src/compress_main_bodies.py build/generated/main src
python3 src/app/compress_settings_bodies.py build/generated/settings src/app
```

The compressors are deterministic (80-column output), so an unchanged fragment produces an identical file and a clean `git diff`.

### stb_image

PNG and JPEG wallpapers are decoded with `stb_image.h`. It is not stored in the repository. The `monolith_stb_image` target runs `third_party/stb/amalgamate.py`, which downloads the header from the upstream `master` branch into `build/generated/stb_image.h` and fails unless its SHA-256 matches the value pinned in `CMakeLists.txt`. `third_party/stb/stb_image.h` is a placeholder that stops the build with an error if it is included by mistake. More in [third_party/stb/README.md](../../third_party/stb/README.md).

If upstream `master` changes the file, the hash check fails and the build stops. Update the pinned hash in `CMakeLists.txt` only after reviewing the new header.

## Sanitizer build

```bash
cmake -S . -B build-sanitized -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined"
cmake --build build-sanitized --parallel 2
```

Running the tests against this build is described in [testing.md](testing.md#sanitizers).

## Troubleshooting

| Problem | Fix |
|---------|-----|
| `Could not find a package configuration file provided by "SDL2"` | Install `libsdl2-dev` |
| `SDL2_ttf>=2.0.18` not found | Install `libsdl2-ttf-dev` |
| `stb_image.h sha256 mismatch` or a download error | Check network access; see the stb section above |
| `Warning: Could not find DejaVuSans.ttf` | Run from the repository root, or install `fonts-dejavu` |
| The window opens but `/Wallpapers/sample.png` is missing | MONOLITH was started outside the repository root. Start it from the root (or from `build/`), then restart |
