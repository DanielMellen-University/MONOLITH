# Monolith

A personal mini operating system that runs as one application.

Monolith is an experimental, self-contained environment written in C++ using SDL2. It aims to feel like a small personal operating system inside a single Linux program, with its own window manager, apps, filesystem, and (planned) scripting language.

**This project is in early development.**

## Current Status

Monolith has a working desktop environment with overlapping windows:

- **Window Manager** — Dragging, 8-way resizing, title bars, z-order, focus, taskbar with Start menu (including a **Games** category), multi-instance titles, session restore (`~/.monolith/session.txt`), and extension-based open routing.
- **Built-in Apps** — Terminal, Text Editor, Filesystem Browser, Drawing, Settings (desktop background color), Snake, and Minesweeper. Each has its own documentation (see below).
- **Internal Filesystem** — Host-backed persistence under `~/.monolith/fs/` with shared recursive copy/remove and a clean virtual path namespace.

**Still early** — no custom language yet, wallpaper is solid color only, and polish is ongoing. The focus is a coherent, self-contained environment that grows over time.

## Building

### Requirements

- C++23 compatible compiler (GCC 11+ or Clang 14+ recommended)
- CMake 3.16 or newer
- SDL2 and SDL2_ttf development libraries
- pkg-config

### Ubuntu / Debian / Pop!_OS

```bash
sudo apt update
sudo apt install build-essential cmake pkg-config libsdl2-dev libsdl2-ttf-dev
```

Optional (system font fallback if the vendored DejaVu font is missing): `fonts-dejavu`.

### Build

```bash
git clone https://github.com/DanielMellen-University/MONOLITH.git
cd MONOLITH

mkdir build
cd build
cmake ..
make -j$(nproc)
```

The resulting binary will be at `build/monolith`. Run it from the repo root (or ensure `assets/fonts/` is findable) so the bundled font loads.

## Running

```bash
# from repository root
./build/monolith
```

**First launch** (no session file): opens a demo set — Terminal, Filesystem Browser, Text Editor on `welcome.txt`, and Settings. The virtual filesystem is seeded with `/home/monolith/documents/`, `/home/monolith/drawings/`, and `welcome.txt` if needed.

**Later launches**: restores windows from `~/.monolith/session.txt` when present; otherwise uses the demo set again.

## Documentation

Full documentation lives in [`docs/`](docs/README.md).

| Topic | Link |
|-------|------|
| Documentation hub | [docs/README.md](docs/README.md) |
| Vision & philosophy | [docs/vision.md](docs/vision.md) |
| Architecture | [docs/architecture.md](docs/architecture.md) |
| Filesystem | [docs/filesystem.md](docs/filesystem.md) |
| Changelog | [CHANGELOG.md](CHANGELOG.md) |

### App Guides

| App | Guide |
|-----|-------|
| Terminal | [docs/apps/terminal.md](docs/apps/terminal.md) |
| Text Editor | [docs/apps/text-editor.md](docs/apps/text-editor.md) |
| Filesystem Browser | [docs/apps/filesystem-browser.md](docs/apps/filesystem-browser.md) |
| Drawing | [docs/apps/drawing.md](docs/apps/drawing.md) |
| Settings | [docs/apps/settings.md](docs/apps/settings.md) |
| Snake | [docs/apps/snake.md](docs/apps/snake.md) |
| Minesweeper | [docs/apps/minesweeper.md](docs/apps/minesweeper.md) |

## License

See [LICENSE](LICENSE) for details.
