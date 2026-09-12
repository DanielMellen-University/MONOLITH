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

## Terminal Lexer Check

Headless test of Terminal quoting, UTF-8-safe command inputs, and quoted or escaped path completion context:

```bash
g++ -std=c++23 scripts/test_terminal_lexer.cpp src/app/TerminalLexer.cpp -o build/test_terminal_lexer && ./build/test_terminal_lexer
```

Headless Terminal filesystem command test for empty directories, regular files, missing paths, root completion, scrollback bounds, and undersized input-bar containment:

```bash
g++ -std=c++23 scripts/test_terminal_filesystem_state.cpp src/app/TerminalApp.cpp src/app/TerminalLexer.cpp src/fs/Filesystem.cpp $(pkg-config --cflags --libs sdl2 SDL2_ttf) -o build/test_terminal_filesystem_state && ./build/test_terminal_filesystem_state
```

Headless Pong state test:

```bash
g++ -std=c++23 scripts/test_pong_state.cpp src/app/PongLogic.cpp -o build/test_pong_state && ./build/test_pong_state
```

Headless Snake state and tiny-client layout test for tail movement, growth collisions, font-scaled HUD geometry, and keeping the board inside its content area:

```bash
g++ -std=c++23 scripts/test_snake_state.cpp src/app/SnakeApp.cpp $(pkg-config --cflags --libs sdl2 SDL2_ttf) -o build/test_snake_state && ./build/test_snake_state
```

Headless Minesweeper state and tiny-client layout test for scaled HUD controls, shared control hitboxes, precise focus pause/resume timing, and keeping the Expert board inside its content area:

```bash
g++ -std=c++23 scripts/test_minesweeper_state.cpp src/app/MinesweeperApp.cpp $(pkg-config --cflags --libs sdl2 SDL2_ttf) -o build/test_minesweeper_state && ./build/test_minesweeper_state
```

## Filesystem Roadmap Checks

Headless test of shipped `Filesystem` initialization, multi-item copy/paste, `/`-rejecting rename, and listing filter:

```bash
g++ -std=c++23 scripts/test_fs_roadmap.cpp src/fs/Filesystem.cpp -o build/test_fs_roadmap && ./build/test_fs_roadmap
```

Headless Filesystem Browser state test for filtering, multi-selection restoration, scaled chrome bands, status-bar hit testing, complete-row hit testing in tiny clients, resize and scale scroll clamping, delete confirmation, and partial cut/paste:

```bash
g++ -std=c++23 scripts/test_filesystem_app_state.cpp src/app/FilesystemApp.cpp src/fs/Filesystem.cpp $(pkg-config --cflags --libs sdl2 SDL2_ttf) -o build/test_filesystem_app_state && ./build/test_filesystem_app_state
```

## Drawing Raster / `.modr` Roadmap Checks

Headless test of shipped line/rect raster, custom RGB parse, eyedropper pixel reads, and `.modr` round-trip:

```bash
g++ -std=c++23 scripts/test_drawing_roadmap.cpp src/app/DrawingRaster.cpp -o build/test_drawing_roadmap && ./build/test_drawing_roadmap
```

Headless Drawing state test for clean loads, resize dirty tracking, history reset, font-scaled chrome, and scaled canvas pointer mapping:

```bash
g++ -std=c++23 scripts/test_drawing_state.cpp src/app/DrawingApp.cpp src/app/DrawingRaster.cpp src/fs/Filesystem.cpp $(pkg-config --cflags --libs sdl2 SDL2_ttf) -o build/test_drawing_state && ./build/test_drawing_state
```

## Desktop Settings Persistence Check

Headless test of desktop preference save/load, UI scale persistence, legacy files, and bounds handling:

```bash
g++ -std=c++23 scripts/test_desktop_settings.cpp src/settings/DesktopSettings.cpp -o build/test_desktop_settings && ./build/test_desktop_settings
```

Headless Settings app test for wallpaper directory/BMP filename completion and shell binding:

```bash
g++ -std=c++23 scripts/test_settings_app_state.cpp src/app/SettingsApp.cpp src/fs/Filesystem.cpp $(pkg-config --cflags --libs sdl2 SDL2_ttf) -o build/test_settings_app_state && ./build/test_settings_app_state
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

Headless Text Editor state test for Save As collisions, failed-write recovery, font-scaled status geometry, complete-row mouse hit testing, and resize scroll bounds:

```bash
g++ -std=c++23 scripts/test_text_editor_state.cpp src/app/TextEditorApp.cpp src/fs/Filesystem.cpp $(pkg-config --cflags --libs sdl2 SDL2_ttf) -o build/test_text_editor_state && ./build/test_text_editor_state
```

Headless WindowManager test that failed Editor and Drawing opens do not reserve stale file singletons and can be retried:

```bash
cmake --build build --target monolith_settings_bodies monolith_stb_image
g++ -std=c++23 -Ibuild/generated -Ibuild/generated/settings scripts/test_window_file_open.cpp src/window/WindowManager.cpp src/window/WallpaperImage.cpp src/app/*.cpp src/fs/Filesystem.cpp src/settings/DesktopSettings.cpp $(pkg-config --cflags --libs sdl2 SDL2_ttf) -o build/test_window_file_open && ./build/test_window_file_open
```

Headless test of scaled WindowManager hit testing, drag math, resize edges, client coordinates, and narrow or undersized desktop geometry:

```bash
g++ -std=c++23 -Ibuild/generated -Ibuild/generated/settings scripts/test_window_coordinates.cpp src/window/WindowManager.cpp src/window/WallpaperImage.cpp src/app/*.cpp src/fs/Filesystem.cpp src/settings/DesktopSettings.cpp $(pkg-config --cflags --libs sdl2 SDL2_ttf) -o build/test_window_coordinates && ./build/test_window_coordinates
```

Headless test of WindowManager client mouse capture across focus changes and pointer exit:

```bash
g++ -std=c++23 -Ibuild/generated -Ibuild/generated/settings scripts/test_window_mouse_capture.cpp src/window/WindowManager.cpp src/window/WallpaperImage.cpp src/app/*.cpp src/fs/Filesystem.cpp src/settings/DesktopSettings.cpp $(pkg-config --cflags --libs sdl2 SDL2_ttf) -o build/test_window_mouse_capture && ./build/test_window_mouse_capture
```

Headless test of keyboard focus handoff when the active window is minimized and when a minimized file singleton is reopened:

```bash
g++ -std=c++23 -Ibuild/generated -Ibuild/generated/settings scripts/test_window_focus.cpp src/window/WindowManager.cpp src/window/WallpaperImage.cpp src/app/*.cpp src/fs/Filesystem.cpp src/settings/DesktopSettings.cpp $(pkg-config --cflags --libs sdl2 SDL2_ttf) -o build/test_window_focus && ./build/test_window_focus
```

Headless test that Shut Down honors app dirty-document guards before allowing the shell to exit:

```bash
g++ -std=c++23 -Ibuild/generated -Ibuild/generated/settings scripts/test_window_quit.cpp src/window/WindowManager.cpp src/window/WallpaperImage.cpp src/app/*.cpp src/fs/Filesystem.cpp src/settings/DesktopSettings.cpp $(pkg-config --cflags --libs sdl2 SDL2_ttf) -o build/test_window_quit && ./build/test_window_quit
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
