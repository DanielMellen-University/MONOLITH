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

Headless test of quoted session paths, including spaces and legacy unquoted paths:

```bash
g++ -std=c++23 scripts/test_session_format.cpp -o build/test_session_format && ./build/test_session_format
```

Headless test of case-insensitive file suffix matching used by open-with routing:

```bash
g++ -std=c++23 scripts/test_file_path.cpp -o build/test_file_path && ./build/test_file_path
```

Headless test of shared UTF-8 codepoint editing helpers used by Text Editor, Terminal, and Filesystem Browser prompts:

```bash
g++ -std=c++23 scripts/test_utf8.cpp -o build/test_utf8 && ./build/test_utf8
```

Headless test of scaled WindowManager hit testing, drag math, resize edges, and client coordinates:

```bash
g++ -std=c++23 scripts/test_window_coordinates.cpp src/window/WindowManager.cpp src/app/*.cpp src/fs/Filesystem.cpp src/settings/DesktopSettings.cpp $(pkg-config --cflags --libs sdl2 SDL2_ttf) -o build/test_window_coordinates && ./build/test_window_coordinates
```

Headless test of WindowManager client mouse capture across focus changes and pointer exit:

```bash
g++ -std=c++23 scripts/test_window_mouse_capture.cpp src/window/WindowManager.cpp src/app/*.cpp src/fs/Filesystem.cpp src/settings/DesktopSettings.cpp $(pkg-config --cflags --libs sdl2 SDL2_ttf) -o build/test_window_mouse_capture && ./build/test_window_mouse_capture
```

Headless test of keyboard focus handoff when the active window is minimized:

```bash
g++ -std=c++23 scripts/test_window_focus.cpp src/window/WindowManager.cpp src/app/*.cpp src/fs/Filesystem.cpp src/settings/DesktopSettings.cpp $(pkg-config --cflags --libs sdl2 SDL2_ttf) -o build/test_window_focus && ./build/test_window_focus
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
