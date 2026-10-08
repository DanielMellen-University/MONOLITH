# Testing

All tests run headless, with no display. CI runs the same suite on every push to `main` and `beta` (see [ci.md](ci.md)).

## Run everything

After [building](building.md):

```bash
ctest --test-dir build --output-on-failure
```

This runs one CTest test, `monolith_headless`, which calls `scripts/run_headless_tests.sh`. The script:

1. runs the 5 static checks (`scripts/verify_*.sh`),
2. builds the generated sources it needs (`monolith_settings_bodies`, `monolith_stb_image`),
3. compiles and runs the 34 test programs in `scripts/test_*.cpp`,
4. prints `ALL HEADLESS TESTS PASSED` at the end.

A full run takes a few minutes. You can also call the script directly: `./scripts/run_headless_tests.sh`.

Environment variables the script reads:

| Variable | Default | Use |
|----------|---------|-----|
| `BUILD_DIR` | `build` | Where test binaries go (CTest sets it to its build folder) |
| `CXX` | `c++` | Compiler for the tests |
| `CXXFLAGS` | empty | Extra flags for every test compile |
| `SDL_VIDEODRIVER` | `dummy` | SDL video driver for SDL tests |
| `SDL_AUDIODRIVER` | `dummy` | SDL audio driver for SDL tests |

## Sanitizers

After the [sanitizer build](building.md#sanitizer-build):

```bash
CXXFLAGS="-g -fsanitize=address,undefined -fno-omit-frame-pointer" \
  ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 \
  UBSAN_OPTIONS=halt_on_error=1:print_stacktrace=1 \
  ctest --test-dir build-sanitized --output-on-failure
```

This is what the CI `sanitize` job runs.

## Run one test

Run these from the repository root after a normal build. Each test prints `ok: ...` lines and fails on the first broken check.

There are three kinds of test. Find the test in the table below for its kind and extra sources, then use the matching pattern.

**Plain** (no SDL):

```bash
g++ -std=c++23 scripts/test_NAME.cpp EXTRA_SOURCES -o build/test_NAME && ./build/test_NAME
```

**SDL** (needs SDL2 and SDL2_ttf; uses the dummy drivers so no display is needed):

```bash
g++ -std=c++23 scripts/test_NAME.cpp EXTRA_SOURCES $(pkg-config --cflags --libs sdl2 SDL2_ttf) -o build/test_NAME \
  && SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ./build/test_NAME
```

**Window manager** (links the whole shell and all apps):

```bash
cmake --build build --target monolith_settings_bodies monolith_stb_image
g++ -std=c++23 -Isrc -Ibuild/generated -Ibuild/generated/settings scripts/test_NAME.cpp \
  src/window/WindowManager.cpp src/window/WallpaperImage.cpp src/app/*.cpp src/fs/Filesystem.cpp \
  src/settings/DesktopSettings.cpp src/detail/AtomicFile.cpp src/detail/AtomicTempOutput.cpp \
  $(pkg-config --cflags --libs sdl2 SDL2_ttf) -o build/test_NAME \
  && SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ./build/test_NAME
```

The suite script compiles the window manager sources once and reuses the objects for all seven window manager tests; the command above compiles them per test, which is slower but simpler.

Any test that saves files needs the two atomic writer sources, `src/detail/AtomicFile.cpp` and `src/detail/AtomicTempOutput.cpp`. Without them the link fails with undefined `AtomicTempCleanup` or `AtomicTempOutputBuffer` symbols. They are listed as `ATOMIC` in the table.

## Test list

`ATOMIC` = `src/detail/AtomicFile.cpp src/detail/AtomicTempOutput.cpp`.

| Test | Kind | Extra sources | Covers |
|------|------|---------------|--------|
| `test_file_path` | Plain | none | Case-insensitive suffix matching for open routing |
| `test_utf8` | Plain | none | UTF-8 editing and completion-prefix helpers |
| `test_random` | Plain | none | Bounded random helper used by the games |
| `test_temp_dir` | Plain | none | `ScopedTempDirectory` test helper |
| `test_atomic_file` | Plain | `ATOMIC` | Workspace sweeps, setup locking, recovery, lease skipping |
| `test_tick_math` | Plain | none | Wrap-safe SDL tick conversion and the 50 ms cap |
| `test_terminal_lexer` | Plain | `src/app/TerminalLexer.cpp` | Quoting, escapes, completion context |
| `test_terminal_filesystem_state` | SDL | `src/app/TerminalApp.cpp src/app/TerminalLexer.cpp src/fs/Filesystem.cpp ATOMIC` | Commands, input editing, history, scrolling |
| `test_text_editor_state` | SDL | `src/app/TextEditorApp.cpp src/fs/Filesystem.cpp ATOMIC` | Load and save, limits, external changes, prompts, layout |
| `test_filesystem_app_state` | SDL | `src/app/FilesystemApp.cpp src/fs/Filesystem.cpp ATOMIC` | Filter, selection, rename, delete, paste feedback |
| `test_fs_roadmap` | Plain | `src/fs/Filesystem.cpp ATOMIC` | `Filesystem` API, recovery, startup maintenance, copy, rename |
| `test_drawing_roadmap` | Plain | `src/app/DrawingRaster.cpp` | Line, rectangle, fill, RGB parsing, `.modr` round trip |
| `test_drawing_state` | SDL | `src/app/DrawingApp.cpp src/app/DrawingRaster.cpp src/fs/Filesystem.cpp ATOMIC` | Open and save, undo history, resize, prompts, layout |
| `test_modr_format` | Plain | `src/app/DrawingRaster.cpp` | `.modr` codec, streaming, size limits |
| `test_desktop_settings` | Plain | `src/settings/DesktopSettings.cpp ATOMIC` | Settings file save and load, limits |
| `test_wallpaper_image` | SDL | `-Ibuild/generated src/window/WallpaperImage.cpp` | BMP and PNG decoding, pixel cap, extensions |
| `test_settings_app_state` | SDL | `-Ibuild/generated/settings src/app/SettingsApp.cpp src/fs/Filesystem.cpp ATOMIC` | Wallpaper completion, layout, footer |
| `test_text_texture_cache` | SDL | none | Text texture cache keys, LRU eviction, byte limit |
| `test_session_format` | Plain | none | Session file quoting, escapes, bounded reads |
| `test_snake_state` | SDL | `src/app/SnakeApp.cpp ATOMIC` | Movement, collisions, high score, layout |
| `test_minesweeper_state` | SDL | `src/app/MinesweeperApp.cpp ATOMIC` | Focus pause, best times, layout |
| `test_breakout_render` | SDL | `src/app/BreakoutApp.cpp src/app/BreakoutLogic.cpp` | Breakout drawing |
| `test_pong_state` | Plain | `src/app/PongLogic.cpp` | Pong rules |
| `test_breakout_state` | Plain | `src/app/BreakoutLogic.cpp` | Breakout rules |
| `test_desktop_icons` | SDL | none | Icon layout and hit testing |
| `test_app_registry` | SDL | none | App registry contents and order |
| `test_start_menu_filter` | SDL | none | Start menu type-ahead matching |
| `test_window_file_open` | Window manager | | File singletons, failed opens, session limits |
| `test_window_coordinates` | Window manager | | Hit testing, dragging, resize edges, clipping |
| `test_window_mouse_capture` | Window manager | | Mouse capture across focus changes |
| `test_window_focus` | Window manager | | Focus handoff on minimize and close |
| `test_minesweeper_window_size` | Window manager | | Window resizing when the level changes |
| `test_window_lifecycle` | Window manager | | Closing windows from inside callbacks, path broadcasts |
| `test_window_quit` | Window manager | | Shut Down honoring unsaved-change guards |

`test_desktop_icons`, `test_app_registry` and `test_start_menu_filter` only need SDL2 headers; the suite compiles them with `$(pkg-config --cflags --libs sdl2)`. The SDL pattern above also works.

## Static checks

These are shell scripts that grep the source to confirm wiring. They need no compiler.

| Script | Checks |
|--------|--------|
| `scripts/verify_drawing_integration.sh` | Drawing launcher, Start menu entry, open routing |
| `scripts/verify_games_integration.sh` | Game launchers, Start menu actions, update dispatch, CMake entries, and that `docs/README.md` links every game page |
| `scripts/verify_desktop_icons.sh` | Desktop icons and the app registry |
| `scripts/verify_start_menu_filter.sh` | Start menu type-ahead |
| `scripts/verify_main_lifecycle.sh` | Session-save error reporting and shutdown order in `main` |

Because of the second check, renaming or unlinking a game page in `docs/README.md` breaks the suite.

## Tests outside the suite

| Test | How to run | Notes |
|------|------------|-------|
| `scripts/test_wallpaper_fit_controller.cpp` | `g++ -std=c++23 scripts/test_wallpaper_fit_controller.cpp $(pkg-config --cflags --libs sdl2) -o build/test_wallpaper_fit_controller && ./build/test_wallpaper_fit_controller` | Wallpaper fit get and set through a fake `IWindowController`. Passes, but the runner does not include it. |
| `scripts/headless_drawing_smoke.sh` | `Xvfb :198 -screen 0 1280x720x24 &` then `./scripts/headless_drawing_smoke.sh` | Starts `build/monolith` for 3 seconds with a temporary `HOME` and checks the filesystem root and a `.modr` file. Needs Xvfb. Set `MONOLITH_TEST_DISPLAY` to use another display number. |

## Writing tests

- Put new tests in `scripts/` as `test_<area>.cpp` and add the compile and run lines to `scripts/run_headless_tests.sh`. Then add a row to the table above.
- Tests use a small local `check(condition, "message")` helper and print `ok: message` for each pass.
- Use `scripts/TestTempDir.hpp` (`monolith::test::ScopedTempDirectory`) for files. It creates a fresh directory with `mkdtemp` and removes only that directory. Do not use fixed paths or PID-based names in `/tmp`.
- SDL tests must work with `SDL_VIDEODRIVER=dummy`. The dummy driver cannot create an accelerated renderer, so tests use a software renderer.
- Link any test that writes files with the atomic writer sources.
