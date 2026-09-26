# Development Scripts

Headless verification scripts for Monolith. These run without a full interactive desktop session and are useful for local sanity checks. GitHub Actions runs the application build and complete headless suite for pushes to `main` or `beta`, pull requests targeting either branch, and manual dispatches. The job uses `ubuntu-24.04`, `actions/checkout@v5`, and SDL's dummy video and audio drivers; it does not need a physical display.

## Complete Headless Suite

Run every static integration check and documented state test with one command:

```bash
./scripts/run_headless_tests.sh
```

The runner builds the generated source fragments, compiles the existing tests into `build/`, and executes SDL tests with dummy video and audio drivers by default. Set `BUILD_DIR`, `CXX`, `SDL_VIDEODRIVER`, or `SDL_AUDIODRIVER` to override those defaults. The individual commands below remain useful when iterating on one subsystem.

The cloud job also configures and builds the complete `monolith` executable before running this suite. A green workflow therefore covers both application compilation and the headless state, renderer, lifecycle, and integration checks.

The focused SDL commands below only show the compile step and binary name to keep them readable. When running them without a display, use the same drivers as the suite, for example:

```bash
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ./build/test_settings_app_state
```

Keep the driver variables unset when testing against a real SDL display. The plain-logic checks do not need these variables.

## Compressed Settings Sources

Settings implementation fragments are stored as wrapped base64/zlib files under `src/app/` so large generated bodies stay manageable in the agent workflow. Decompress them before building, or let CMake do it through `monolith_settings_bodies`:

```bash
python3 src/app/decompress_settings_bodies.py build/generated/settings
```

After editing a decompressed fragment, regenerate the tracked source representation with the matching compressor:

```bash
python3 src/app/compress_settings_bodies.py build/generated/settings src/app
```

The compressor writes deterministic 80-column ASCII output and preserves the `.inc.z64` naming expected by CMake.

## Compressed Main Sources

The main loop uses the same format. After editing `build/generated/main/main_body_*.inc`, regenerate its tracked source representation with:

```bash
python3 src/compress_main_bodies.py build/generated/main src
```

The complete headless runner also checks that the Window Manager and its
SDL-backed apps are destroyed before renderer and SDL shutdown:

```bash
./scripts/verify_main_lifecycle.sh
```

## Drawing Integration Check

Static grep-based check that Drawing is wired into the window manager and Start menu:

```bash
./scripts/verify_drawing_integration.sh
```

Verifies `launchDrawing` declarations, Start menu entry, and related integration points.

## Games Integration Check

Static checks that Snake, Minesweeper, Pong, and Breakout are wired into the shell:

```bash
./scripts/verify_games_integration.sh
```

Verifies launchers, Start menu actions (including Breakout), `App::update()` dispatch, CMake entries, and docs hub links.

## Terminal Lexer Check

Headless test of Terminal quoting, UTF-8-safe command inputs, and quoted or escaped path completion context:

```bash
g++ -std=c++23 scripts/test_terminal_lexer.cpp src/app/TerminalLexer.cpp -o build/test_terminal_lexer && ./build/test_terminal_lexer
```

Headless Terminal filesystem command test for empty directories, regular files, missing paths, recursive-copy notifications, root completion, scrollback bounds, and undersized input-bar containment:

```bash
g++ -std=c++23 scripts/test_terminal_filesystem_state.cpp src/app/TerminalApp.cpp src/app/TerminalLexer.cpp src/fs/Filesystem.cpp $(pkg-config --cflags --libs sdl2 SDL2_ttf) -o build/test_terminal_filesystem_state && ./build/test_terminal_filesystem_state
```

Headless Pong state test:

```bash
g++ -std=c++23 scripts/test_pong_state.cpp src/app/PongLogic.cpp -o build/test_pong_state && ./build/test_pong_state
```

Headless Breakout state test:

```bash
g++ -std=c++23 scripts/test_breakout_state.cpp src/app/BreakoutLogic.cpp -o build/test_breakout_state && ./build/test_breakout_state
```

Headless Snake state and tiny-client layout test for score persistence, tail movement, growth collisions, font-scaled HUD geometry, and keeping the board inside its content area:

```bash
g++ -std=c++23 scripts/test_snake_state.cpp src/app/SnakeApp.cpp $(pkg-config --cflags --libs sdl2 SDL2_ttf) -o build/test_snake_state && ./build/test_snake_state
```

Headless check of the bounded random helper shared by the built-in games:

```bash
g++ -std=c++23 scripts/test_random.cpp -o build/test_random && ./build/test_random
```

Headless check of the shared SDL tick conversion used by the real-time games:

```bash
g++ -std=c++23 scripts/test_tick_math.cpp -o build/test_tick_math && ./build/test_tick_math
```

Headless renderer-backed check for game text texture reuse, color-key separation, LRU limits, and renderer switching:

```bash
g++ -std=c++23 scripts/test_text_texture_cache.cpp $(pkg-config --cflags --libs sdl2 SDL2_ttf) -o build/test_text_texture_cache
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ./build/test_text_texture_cache
```

Headless Minesweeper state and tiny-client layout test for best-time persistence, scaled HUD controls, shared control hitboxes, precise focus pause/resume timing, and keeping the Expert board inside its content area:

```bash
g++ -std=c++23 scripts/test_minesweeper_state.cpp src/app/MinesweeperApp.cpp $(pkg-config --cflags --libs sdl2 SDL2_ttf) -o build/test_minesweeper_state && ./build/test_minesweeper_state
```

## Filesystem Roadmap Checks

Headless test of shipped `Filesystem` initialization, multi-item copy/paste, `/`-rejecting rename, and listing filter:

```bash
g++ -std=c++23 scripts/test_fs_roadmap.cpp src/fs/Filesystem.cpp -o build/test_fs_roadmap && ./build/test_fs_roadmap
```

Headless Filesystem Browser state test for filtering, multi-selection restoration, direct inline rename notifications, scaled chrome bands, status-bar hit testing, complete-row hit testing in tiny clients, resize and scale scroll clamping, delete confirmation, and partial cut/paste:

```bash
g++ -std=c++23 scripts/test_filesystem_app_state.cpp src/app/FilesystemApp.cpp src/fs/Filesystem.cpp $(pkg-config --cflags --libs sdl2 SDL2_ttf) -o build/test_filesystem_app_state && ./build/test_filesystem_app_state
```

## Drawing Raster / `.modr` Roadmap Checks

Headless test of shipped line/rect raster, custom RGB parse, eyedropper pixel reads, and `.modr` round-trip:

```bash
g++ -std=c++23 scripts/test_drawing_roadmap.cpp src/app/DrawingRaster.cpp -o build/test_drawing_roadmap && ./build/test_drawing_roadmap
```

Headless Drawing state test for clean loads, resize dirty tracking, history reset, font-scaled chrome, scaled canvas pointer mapping, duplicate file singleton rejection, and creation-notification binding order:

```bash
g++ -std=c++23 scripts/test_drawing_state.cpp src/app/DrawingApp.cpp src/app/DrawingRaster.cpp src/fs/Filesystem.cpp $(pkg-config --cflags --libs sdl2 SDL2_ttf) -o build/test_drawing_state && ./build/test_drawing_state
```

## Desktop Settings Persistence Check

Headless test of desktop preference save/load, UI scale persistence, legacy files, and bounds handling:

```bash
g++ -std=c++23 scripts/test_desktop_settings.cpp src/settings/DesktopSettings.cpp -o build/test_desktop_settings && ./build/test_desktop_settings
```

Headless Settings app test for wallpaper directory/image filename completion, shell binding, font-scaled layout metrics, and tiny-client footer containment:

```bash
g++ -std=c++23 -Ibuild/generated/settings scripts/test_settings_app_state.cpp src/app/SettingsApp.cpp src/fs/Filesystem.cpp $(pkg-config --cflags --libs sdl2 SDL2_ttf) -o build/test_settings_app_state && ./build/test_settings_app_state
```

Headless test of quoted session paths, including spaces and legacy unquoted paths:

```bash
g++ -std=c++23 scripts/test_session_format.cpp -o build/test_session_format && ./build/test_session_format
```

Headless test of case-insensitive file suffix matching used by open-with routing:

```bash
g++ -std=c++23 scripts/test_file_path.cpp -o build/test_file_path && ./build/test_file_path
```

Headless test of shared UTF-8 codepoint editing and completion-prefix helpers used by Text Editor, Terminal, Drawing, and Filesystem Browser prompts:

```bash
g++ -std=c++23 scripts/test_utf8.cpp -o build/test_utf8 && ./build/test_utf8
```

Headless Text Editor state test for Save As collisions, creation-notification binding order, failed-write recovery, Unicode-safe path completion, font-scaled status geometry, complete-row mouse hit testing, and resize scroll bounds:

```bash
g++ -std=c++23 scripts/test_text_editor_state.cpp src/app/TextEditorApp.cpp src/fs/Filesystem.cpp $(pkg-config --cflags --libs sdl2 SDL2_ttf) -o build/test_text_editor_state && ./build/test_text_editor_state
```

Headless WindowManager test that failed Editor and Drawing opens do not reserve stale file singletons, can be retried, skip stale file-backed session entries without seeding demo windows, and release bare-app instance slots when they become file-backed:

```bash
cmake --build build --target monolith_settings_bodies monolith_stb_image
g++ -std=c++23 -Ibuild/generated -Ibuild/generated/settings scripts/test_window_file_open.cpp src/window/WindowManager.cpp src/window/WallpaperImage.cpp src/app/*.cpp src/fs/Filesystem.cpp src/settings/DesktopSettings.cpp $(pkg-config --cflags --libs sdl2 SDL2_ttf) -o build/test_window_file_open && ./build/test_window_file_open
```

Headless test of scaled WindowManager hit testing, drag math, resize edges, client coordinates, renderer client clipping, render-time callback removal, and narrow or undersized desktop geometry:

```bash
g++ -std=c++23 -Ibuild/generated -Ibuild/generated/settings scripts/test_window_coordinates.cpp src/window/WindowManager.cpp src/window/WallpaperImage.cpp src/app/*.cpp src/fs/Filesystem.cpp src/settings/DesktopSettings.cpp $(pkg-config --cflags --libs sdl2 SDL2_ttf) -o build/test_window_coordinates && ./build/test_window_coordinates
```

Headless test of WindowManager client mouse capture across focus changes, pointer exit, host focus-loss recovery, and suppression of queued pointer events after focus loss:

```bash
g++ -std=c++23 -Ibuild/generated -Ibuild/generated/settings scripts/test_window_mouse_capture.cpp src/window/WindowManager.cpp src/window/WallpaperImage.cpp src/app/*.cpp src/fs/Filesystem.cpp src/settings/DesktopSettings.cpp $(pkg-config --cflags --libs sdl2 SDL2_ttf) -o build/test_window_mouse_capture && ./build/test_window_mouse_capture
```

Headless test of keyboard focus handoff when the active window is minimized, when a minimized file singleton is reopened, when queued app input is suppressed while the host is unfocused, and when focus callbacks close windows during activation or Start-menu suspension:

```bash
g++ -std=c++23 -Ibuild/generated -Ibuild/generated/settings scripts/test_window_focus.cpp src/window/WindowManager.cpp src/window/WallpaperImage.cpp src/app/*.cpp src/fs/Filesystem.cpp src/settings/DesktopSettings.cpp $(pkg-config --cflags --libs sdl2 SDL2_ttf) -o build/test_window_focus && ./build/test_window_focus
```

Headless test that app-triggered closes during `App::update()` do not invalidate the WindowManager update pass:

```bash
g++ -std=c++23 -Ibuild/generated -Ibuild/generated/settings scripts/test_window_lifecycle.cpp src/window/WindowManager.cpp src/window/WallpaperImage.cpp src/app/*.cpp src/fs/Filesystem.cpp src/settings/DesktopSettings.cpp $(pkg-config --cflags --libs sdl2 SDL2_ttf) -o build/test_window_lifecycle && ./build/test_window_lifecycle
```

The same lifecycle test also covers virtual-path notification dispatch, bound-file remaps, resize callbacks, session restore geometry, click activation, and reentrant close callbacks, including apps closing themselves while the shell broadcasts an event, remaps bindings, reapplies desktop geometry, restores a session entry, finishes focusing an input target, or is already inside `allowClose()` / `onFocusLost()`; it also verifies that a sibling close during focus loss cannot invalidate the outer close.

The lifecycle assertions record callback activity outside the app objects they destroy, so the test can also be run under AddressSanitizer without depending on freed app state.

Headless test that Shut Down honors app dirty-document guards before allowing the shell to exit, including an app opening another window while the close contract is checked and a callback-created editor becoming dirty before validation completes:

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
