# Development Scripts

Headless verification scripts for Monolith. These run without a full interactive desktop session and are useful for local sanity checks. There is no automated CI pipeline in-repo yet; treat “CI” as optional future use.

## Drawing Integration Check

Static grep-based check that Drawing is wired into the window manager and Start menu:

```bash
./scripts/verify_drawing_integration.sh
```

Verifies `launchDrawing` declarations, Start menu entry, and related integration points.

## Games Integration Check

Static checks that Snake, Minesweeper, and Pong are wired into the shell:

```bash
./scripts/verify_games_integration.sh
```

Verifies launchers, Start menu actions (including Pong), `App::update()` dispatch, CMake entries, and docs hub links.

Headless Pong state test:

```bash
g++ -std=c++23 scripts/test_pong_state.cpp src/app/PongLogic.cpp -o build/test_pong_state && ./build/test_pong_state
```

## Filesystem Roadmap Checks

Headless test of shipped `Filesystem` multi-item copy/paste, `/`-rejecting rename, and listing filter:

```bash
g++ -std=c++23 scripts/test_fs_roadmap.cpp src/fs/Filesystem.cpp -o build/test_fs_roadmap && ./build/test_fs_roadmap
```

## Drawing Raster / `.modr` Roadmap Checks

Headless test of shipped line/rect raster, custom RGB parse, eyedropper pixel reads, and `.modr` round-trip:

```bash
g++ -std=c++23 scripts/test_drawing_roadmap.cpp src/app/DrawingRaster.cpp -o build/test_drawing_roadmap && ./build/test_drawing_roadmap
```

## Desktop Settings Persistence Check

Headless test of desktop preference save/load, UI scale persistence, legacy files, and bounds handling:

```bash
g++ -std=c++23 scripts/test_desktop_settings.cpp src/settings/DesktopSettings.cpp -o build/test_desktop_settings && ./build/test_desktop_settings
```

## `.modr` Format Roundtrip

Compiles and runs a standalone test of the Drawing raster file format:

```bash
g++ -std=c++23 scripts/test_modr_format.cpp -o build/test_modr_format && ./build/test_modr_format
```

## Headless Drawing Smoke (Optional)

```bash
./scripts/headless_drawing_smoke.sh
```

See script source for requirements and what it exercises.

## Adding New Scripts

When adding verification scripts:

1. Place them under `scripts/`.
2. Document usage here with the exact command.
3. Link from the relevant `docs/apps/<app>.md` developer notes section if app-specific.
