# Repository layout

```text
MONOLITH/
├── CMakeLists.txt          Build definition and the monolith_headless CTest test
├── README.md               Project landing page
├── CHANGELOG.md            Change history, newest first
├── LICENSE                 MIT
├── CURRENT_CHUNK           Name of the current work chunk (1.0-released)
├── .github/workflows/      headless.yml, the CI workflow
├── .agents/                Instructions and logs for coding agents
├── assets/fonts/           DejaVuSans.ttf, the UI font
├── docs/                   Documentation (start at docs/README.md)
├── scripts/                Tests, static checks and helper scripts
├── src/                    Application source
└── third_party/stb/        stb_image fetch script and placeholder header
```

## src/

| Path | Contents |
|------|----------|
| `main.cpp` | Includes the two main body fragments |
| `main_body_*.inc.z64` | Compressed startup and main loop code; see [building.md](building.md#generated-sources) |
| `compress_main_bodies.py`, `decompress_main_bodies.py` | Tools for the fragments above |
| `app/App.hpp` | `App` base class and `IWindowController` |
| `app/*App.{hpp,cpp}` | The nine apps |
| `app/TerminalLexer.*` | Terminal quoting and completion parsing |
| `app/DrawingRaster.*` | Drawing pixel operations and the `.modr` codec |
| `app/PongLogic.*`, `app/BreakoutLogic.*` | Game rules without SDL |
| `app/TerminalApp_rest.inc`, `app/BreakoutApp_body.inc` | Parts of those apps split into include files |
| `app/SettingsApp_body_*.inc.z64` | Compressed Settings code, plus its compress and decompress scripts |
| `app/FilePath.hpp`, `app/Utf8.hpp` | Suffix matching and UTF-8 helpers |
| `app/Version.hpp` | `kVersion` (`1.0`) and `kVersionLabel` |
| `window/WindowManager.{hpp,cpp}` | The shell; the `.cpp` includes the fragments in `window/detail/` |
| `window/WindowManager_private.inc` | Private members of `WindowManager` |
| `window/detail/wm_*.inc` | Window manager code split by area (launchers in `wm_body_07.inc` and `wm_body_08a.inc`, wallpaper in `wm_body_08b.inc`, file bindings and broadcasts in `wm_body_09_*.inc`) |
| `window/AppRegistry.hpp` | The app table behind the Start menu, desktop icons and session restore |
| `window/DesktopIcons.hpp`, `window/StartMenuFilter.hpp` | Icon layout and Start menu matching |
| `window/SessionFormat.hpp` | Reading and writing `session.txt` records |
| `window/WallpaperImage.*` | Wallpaper decoding |
| `window/Window.hpp` | The `Window` struct |
| `fs/Filesystem.*` | The internal filesystem |
| `settings/DesktopSettings.*` | The settings file |
| `detail/AtomicFile.*`, `detail/AtomicTempOutput.cpp` | The atomic writer |
| `detail/TextTextureCache.hpp` | Shared text texture cache |
| `detail/RendererClip.hpp` | Save and restore helpers for the renderer clip, blend mode and draw color |
| `detail/BoundedLineReader.hpp`, `detail/BufferedStreamWriter.hpp` | Bounded reading and buffered writing |
| `detail/Random.hpp`, `detail/TickMath.hpp` | Game random streams and frame timing |
| `detail/main_seed_png.inc` | Startup code that seeds `/Wallpapers/sample.png` |

## scripts/

| Path | Contents |
|------|----------|
| `run_headless_tests.sh` | The full test suite (what CTest runs) |
| `test_*.cpp` | Test programs; see [testing.md](testing.md#test-list) |
| `verify_*.sh` | Static wiring checks |
| `TestTempDir.hpp` | Temporary directory helper for tests |
| `headless_drawing_smoke.sh` | Optional smoke test of the real binary under Xvfb |
| `gen_sample_wallpaper.py` | Creates `assets/wallpapers/sample.bmp` and `sample.png` |

## Build output

Everything under `build/` and `build-sanitized/` is ignored by git.

| Path | Contents |
|------|----------|
| `build/monolith` | The application |
| `build/generated/main/`, `build/generated/settings/` | Decompressed source fragments |
| `build/generated/stb_image.h` | Downloaded stb header |
| `build/test_*` | Test binaries from the suite |
