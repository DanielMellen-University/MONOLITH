# Development Scripts

Headless verification scripts for Monolith. These run without a full interactive desktop session and are useful for local sanity checks. GitHub Actions builds the application and runs the complete headless suite normally and under AddressSanitizer/UndefinedBehaviorSanitizer for pushes to `main` or `beta`, pull requests targeting either branch, and manual dispatches. The jobs use `ubuntu-24.04`, `actions/checkout@v5`, and SDL's dummy video and audio drivers; they do not need a physical display.

## Complete Headless Suite

Run every static integration check and documented state test with one command:

```bash
ctest --test-dir build --output-on-failure
```

After configuring and building with CMake, this registered CTest test runs the complete headless suite. It builds generated source fragments, compiles the existing tests, and executes SDL tests with dummy video and audio drivers by default. Set `BUILD_DIR`, `CXX`, `CXXFLAGS`, `SDL_VIDEODRIVER`, or `SDL_AUDIODRIVER` to override those defaults. `CXXFLAGS` are passed to each test compiler invocation, including the shared Window Manager test objects. The runner can also be invoked directly with `./scripts/run_headless_tests.sh`; individual commands below remain useful when iterating on one subsystem.

The six Window Manager integration tests link against one set of shared app/runtime objects, so the same implementation is not recompiled for each test binary. Lifecycle and coordinate coverage also exercises nested callback snapshots, close-during-render, identity-index consistency, snapshot-buffer reuse after warm-up, focused-first taskbar order, and the non-owning taskbar layout invariant.

Both cloud jobs configure and build the complete `monolith` executable before running the registered test through CTest. The sanitizer job additionally instruments the application and every headless test with AddressSanitizer and UndefinedBehaviorSanitizer.

Headless tests that need host files use `scripts/TestTempDir.hpp` and its `ScopedTempDirectory`. It creates a fresh directory with `mkdtemp` and removes only that directory at scope exit. Do not use fixed temporary paths or clear PID-derived names before use; stale paths can survive a crash and PIDs are reused.

## Sanitized Suite

Run the same suite with address and undefined-behavior checks locally:

```bash
cmake -S . -B build-sanitized -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined"
cmake --build build-sanitized --parallel 2
CXXFLAGS="-g -fsanitize=address,undefined -fno-omit-frame-pointer" \
  ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
  UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
  ctest --test-dir build-sanitized --output-on-failure
```

CMake tracks every decompressed main and Settings fragment as an output and watches the compressed-fragment globs for additions or removals. An incremental build therefore regenerates missing secondary includes and reconfigures when the fragment set changes.

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

The complete headless runner also checks that session-save failures are reported
while SDL is still live, and that the Window Manager and its SDL-backed apps are
destroyed before renderer and SDL shutdown:

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

Headless test of Terminal quoting and escapes, literal backslashes in quoted paths, UTF-8-safe command inputs, and quoted or escaped path completion context:

```bash
g++ -std=c++23 scripts/test_terminal_lexer.cpp src/app/TerminalLexer.cpp -o build/test_terminal_lexer && ./build/test_terminal_lexer
```

Headless Terminal state test for UTF-8-safe input selection and clipboard editing, reusable prompt/render/hit-test buffers, mouse selection, filesystem commands, completion, bounded history loading and streamed saving (including exact 2 MiB output), vertical and horizontal scrollback, and undersized input-bar containment:

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

Headless renderer-backed check for game text texture reuse, font/color key separation, owned cache keys, LRU eviction across map rehashes, byte limits, and renderer switching:

```bash
g++ -std=c++23 scripts/test_text_texture_cache.cpp $(pkg-config --cflags --libs sdl2 SDL2_ttf) -o build/test_text_texture_cache
SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ./build/test_text_texture_cache
```

Headless Minesweeper state and tiny-client layout test for best-time persistence, scaled HUD controls, shared control hitboxes, precise focus pause/resume timing, and keeping the Expert board inside its content area:

```bash
g++ -std=c++23 scripts/test_minesweeper_state.cpp src/app/MinesweeperApp.cpp $(pkg-config --cflags --libs sdl2 SDL2_ttf) -o build/test_minesweeper_state && ./build/test_minesweeper_state
```

## Filesystem Roadmap Checks

Headless atomic-writer regression for per-directory first-touch and interval sweeps, setup-lock exclusion, incomplete v3 recovery, marked v2 recovery, active lease skipping, and preservation of incomplete v2 and legacy workspaces:

```bash
g++ -std=c++23 scripts/test_atomic_file.cpp -o build/test_atomic_file && ./build/test_atomic_file
```

Headless test of shipped `Filesystem` initialization, safe last-write-time updates, atomic streaming-writer success and rollback, preservation of neighboring `.tmp` files/symlinks, multi-item copy/paste, `/`-rejecting rename, and listing filter:

```bash
g++ -std=c++23 scripts/test_fs_roadmap.cpp src/fs/Filesystem.cpp -o build/test_fs_roadmap && ./build/test_fs_roadmap
```

Headless Filesystem Browser state test for filtered snapshot reuse and F5 refresh, retained filter-label and rename-caret storage, multi-selection restoration, direct inline rename notifications, scaled chrome bands, status-bar hit testing, complete-row hit testing in tiny clients, resize and scale scroll clamping, delete confirmation, and partial cut/paste:

```bash
g++ -std=c++23 scripts/test_filesystem_app_state.cpp src/app/FilesystemApp.cpp src/fs/Filesystem.cpp $(pkg-config --cflags --libs sdl2 SDL2_ttf) -o build/test_filesystem_app_state && ./build/test_filesystem_app_state
```

## Drawing Raster / `.modr` Roadmap Checks

Headless test of line/rect raster, batched brush-stroke equivalence, clipped and extreme rectangles, invalid buffers, scanline fill connectivity and large regions, custom RGB parse, eyedropper pixel reads, and `.modr` round-trip:

```bash
g++ -std=c++23 scripts/test_drawing_roadmap.cpp src/app/DrawingRaster.cpp -o build/test_drawing_roadmap && ./build/test_drawing_roadmap
```

Headless Drawing state test for clean loads, bounded `.modr` opens and retry-state preservation, saved-baseline allocation reuse, resize and tile-dirty tracking, sparse Fill/Clear history and scratch-map reuse (including canvases above 64 MiB), save-mid-history undo/redo, sparse multi-tile stroke history (including over-budget stroke fallback), cached byte-account consistency through undo/redo/reset/eviction, sparse-state eviction at the 32-state and 64 MiB caps, font-scaled chrome, scaled canvas pointer mapping, duplicate file singleton rejection, and creation-notification binding order:

```bash
g++ -std=c++23 scripts/test_drawing_state.cpp src/app/DrawingApp.cpp src/app/DrawingRaster.cpp src/fs/Filesystem.cpp $(pkg-config --cflags --libs sdl2 SDL2_ttf) -o build/test_drawing_state && ./build/test_drawing_state
```

## Desktop Settings Persistence Check

Headless test of desktop preference save/load, UI scale persistence, legacy files, bounded 16 KiB lines and 64-record loading, wallpaper-path persistence limits, and atomic-save behavior beside a neighboring symlink:

```bash
g++ -std=c++23 scripts/test_desktop_settings.cpp src/settings/DesktopSettings.cpp -o build/test_desktop_settings && ./build/test_desktop_settings
```

Headless wallpaper decoder check for BMP/PNG decoding, pixel-count rejection before decode, and unsupported-extension rejection:

```bash
cmake --build build --target monolith_stb_image
g++ -std=c++23 -Ibuild/generated scripts/test_wallpaper_image.cpp src/window/WallpaperImage.cpp $(pkg-config --cflags --libs sdl2 SDL2_ttf) -o build/test_wallpaper_image && ./build/test_wallpaper_image
```

Headless Settings app test for wallpaper directory/image filename completion, shell binding, font-scaled layout metrics, and tiny-client footer containment:

```bash
g++ -std=c++23 -Ibuild/generated/settings scripts/test_settings_app_state.cpp src/app/SettingsApp.cpp src/fs/Filesystem.cpp $(pkg-config --cflags --libs sdl2 SDL2_ttf) -o build/test_settings_app_state && ./build/test_settings_app_state
```

Headless test of quoted session paths, escaped line breaks, legacy unquoted paths, and bounded session-record reads:

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

Headless Text Editor state test for bounded streamed file loading and saving, CRLF across read-chunk boundaries, line separators across save chunks, oversized open/save rejection, Save As collisions, creation-notification binding order, failed-write recovery, Unicode-safe path completion, font-scaled status geometry, complete-row mouse hit testing, and resize scroll bounds:

```bash
g++ -std=c++23 scripts/test_text_editor_state.cpp src/app/TextEditorApp.cpp src/fs/Filesystem.cpp $(pkg-config --cflags --libs sdl2 SDL2_ttf) -o build/test_text_editor_state && ./build/test_text_editor_state
```

Headless WindowManager test that failed Editor and Drawing opens do not reserve stale file singletons, can be retried, skip stale or overlong session records, stop after 1,024 session rows or once 128 windows are live, save only the topmost 128 restorable windows in stacking order, and release bare-app instance slots when they become file-backed:

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

Compiles and runs a headless test against the production Drawing raster codec, including split-chunk streaming decode, byte-identical streaming output, and bounded writes for a maximum-size canvas:

```bash
g++ -std=c++23 scripts/test_modr_format.cpp src/app/DrawingRaster.cpp -o build/test_modr_format && ./build/test_modr_format
```

## Headless Drawing Smoke (Optional)

```bash
./scripts/headless_drawing_smoke.sh
```

See script source for requirements and what it exercises.

The smoke script creates a unique temporary HOME (optionally under
`MONOLITH_TEST_HOME_PARENT`), verifies Monolith's actual `$HOME/.monolith/fs`
root and drawings directory, then writes and reads a uniquely named `.modr`
fixture there. It removes only its own temporary data; it never clears a shared
test path.

## Adding New Scripts

When adding verification scripts:

1. Place them under `scripts/`.
2. Document usage here with the exact command.
3. Link from the relevant `docs/apps/<app>.md` developer notes section if app-specific.
