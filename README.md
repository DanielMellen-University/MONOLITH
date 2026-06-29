# Monolith

A personal mini operating system that runs as one application.

Monolith is an experimental, self-contained environment written in C++ using SDL2. It aims to feel like a small personal operating system inside a single Linux program, with its own window manager, apps, filesystem, and scripting language.

**This project is in very early development.**

## Current Status

Monolith has a working desktop environment with overlapping windows:

- **Window Manager** — Dragging, 8-way resizing, title bars, z-order, focus, taskbar with Start menu, and dynamic multi-instance window titles.
- **Built-in Apps** — Terminal, Text Editor, Filesystem Browser, Drawing, and Settings (desktop background color). Each has its own documentation (see below).
- **Internal Filesystem** — Host-backed persistence under `~/.monolith/fs/` with a clean virtual path namespace.

**This is still early** — no custom language yet, limited polish, and the set of apps is small. The focus is on building a coherent, self-contained environment over time.

## Building

### Requirements

- C++23 compatible compiler (GCC 11+ or Clang 14+ recommended)
- CMake 3.16 or newer
- SDL2 development libraries

### Ubuntu / Debian / Pop!_OS

```bash
sudo apt update
sudo apt install build-essential cmake libsdl2-dev
```

### Build

```bash
git clone https://github.com/yourusername/monolith.git
cd monolith

mkdir build
cd build
cmake ..
make -j$(nproc)
```

The resulting binary will be at `build/monolith`.

## Running

```bash
./build/monolith
```

On first launch (when a font is available), Monolith opens a small demo set of windows: Terminal, Filesystem Browser, a Text Editor on `welcome.txt`, and Settings. The internal filesystem is seeded with `/home/monolith/documents/`, `/home/monolith/drawings/`, and `welcome.txt` if they do not already exist.

## Documentation

Full documentation lives in [`docs/`](docs/README.md).

| Topic | Link |
|-------|------|
| Documentation hub | [docs/README.md](docs/README.md) |
| Vision & philosophy | [docs/vision.md](docs/vision.md) |
| Architecture | [docs/architecture.md](docs/architecture.md) |
| Filesystem | [docs/filesystem.md](docs/filesystem.md) |

### App Guides

| App | Guide |
|-----|-------|
| Terminal | [docs/apps/terminal.md](docs/apps/terminal.md) |
| Text Editor | [docs/apps/text-editor.md](docs/apps/text-editor.md) |
| Filesystem Browser | [docs/apps/filesystem-browser.md](docs/apps/filesystem-browser.md) |
| Drawing | [docs/apps/drawing.md](docs/apps/drawing.md) |
| Settings | [docs/apps/settings.md](docs/apps/settings.md) |

## License

See [LICENSE](LICENSE) for details.