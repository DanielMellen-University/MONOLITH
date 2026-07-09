# Development Scripts

Headless verification scripts for Monolith. These run without launching the full SDL desktop and are useful for CI or quick sanity checks.

## Drawing Integration Check

Static grep-based check that Drawing is wired into the window manager and Start menu:

```bash
./scripts/verify_drawing_integration.sh
```

Verifies `launchDrawing` declarations, Start menu entry, and related integration points.

## Games Integration Check

Static checks that Snake and Minesweeper are wired into the shell:

```bash
./scripts/verify_games_integration.sh
```

Verifies launchers, Start menu actions, `App::update()` dispatch, CMake entries, and docs hub links.

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