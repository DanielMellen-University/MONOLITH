# MONOLITH

MONOLITH is a personal mini-OS that runs as a single Linux program. It is a desktop environment written in C++23 on SDL2: a window manager with a taskbar, Start menu and desktop icons, nine built-in apps, and its own filesystem stored under `~/.monolith/`. You start one binary and do your work inside it.

**Status:** 1.0, released October 2026 (tag `v1.0.0`). See the [roadmap](docs/ROADMAP.md).

## Apps

| App | What it does | Guide |
|-----|--------------|-------|
| Terminal | Command line for the internal filesystem: `ls`, `cd`, `cp`, `mv`, `rm`, `cat`, `open`, history, Tab completion | [terminal.md](docs/apps/terminal.md) |
| Text Editor | Plain text editor with syntax colors, undo, Find and Replace | [text-editor.md](docs/apps/text-editor.md) |
| Filesystem | Graphical file browser: create, rename, delete, copy, cut, paste, filter | [filesystem-browser.md](docs/apps/filesystem-browser.md) |
| Drawing | Pixel sketching with pen, eraser, fill, line, rectangle and color picker; saves `.modr` files | [drawing.md](docs/apps/drawing.md) |
| Settings | Desktop color, wallpaper image and fit, clock format, text size | [settings.md](docs/apps/settings.md) |
| Snake | Classic Snake on a 20x20 board with a saved high score | [snake.md](docs/apps/snake.md) |
| Minesweeper | Three difficulties with saved best times | [minesweeper.md](docs/apps/minesweeper.md) |
| Pong | One player against a simple AI, first to 5 | [pong.md](docs/apps/pong.md) |
| Breakout | Five rows of bricks, three lives | [breakout.md](docs/apps/breakout.md) |

Every app can run in several windows at once. Open them from the Start menu (games are under **Games**) or from the desktop icons.

## Build and run

You need a C++23 compiler (CI uses GCC 13 on Ubuntu 24.04), CMake 3.16 or newer, pkg-config, Python 3, SDL2, and SDL2_ttf 2.0.18 or newer.

On Ubuntu, Debian or Pop!_OS:

```bash
sudo apt install build-essential cmake pkg-config python3 libsdl2-dev libsdl2-ttf-dev
```

Build:

```bash
git clone https://github.com/DanielMellen-University/MONOLITH.git
cd MONOLITH
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)"
```

The first build downloads `stb_image.h` (used for PNG and JPEG wallpapers) and checks its SHA-256, so it needs network access. See [Building](docs/development/building.md) for details.

Run from the repository root so the bundled font in `assets/fonts/` is found (otherwise MONOLITH looks for DejaVu Sans in the system font folders):

```bash
./build/monolith
```

On first launch MONOLITH creates `~/.monolith/fs/` and opens Terminal, Filesystem, Text Editor (on `welcome.txt`) and Settings. When you quit, it saves your open windows to `~/.monolith/session.txt` and restores them next time.

## How it works

- **One SDL window.** The whole desktop is drawn inside a fixed 1280x720 SDL window. The window manager draws frames, the taskbar and the Start menu, and gives each app a client rectangle to render into.
- **Native apps.** Each app is a C++ class implementing the `App` interface (`render`, `handleEvent`, `update`, plus focus, resize and file callbacks). Apps never call each other. They ask the shell for things through `IWindowController`, for example "open this path" or "set the wallpaper".
- **Internal filesystem.** Apps use virtual paths like `/home/monolith/notes.txt`. The `Filesystem` class maps them to `~/.monolith/fs/` on the host and writes every file atomically, so a failed save never leaves a half-written file.
- **App registry.** One table in `src/window/AppRegistry.hpp` drives the Start menu, the desktop icons and session restore.

Read [Architecture](docs/architecture.md) for the full picture.

## Documentation

| Page | Contents |
|------|----------|
| [Documentation hub](docs/README.md) | Index of every doc |
| [Architecture](docs/architecture.md) | Window manager, app model, rendering, input |
| [Filesystem](docs/filesystem.md) | Virtual paths, host storage, file API |
| [App guides](docs/README.md#apps) | One page per app |
| [Development](docs/README.md#development) | Building, testing, CI, repo layout, contributing |
| [Roadmap](docs/ROADMAP.md) | What 1.0 includes and what it leaves out |
| [Changelog](CHANGELOG.md) | Change history |

## Scope

MONOLITH is a normal Linux application, not an operating system kernel. A custom programming language is a separate future project, MONOLITH 2, in its own repository.

## License

MIT. See [LICENSE](LICENSE).
