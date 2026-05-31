# Monolith

A personal mini operating system that runs as one application.

Monolith is an experimental, self-contained environment written in C++ using SDL2. It aims to feel like a small personal operating system inside a single Linux program, with its own window manager, apps, filesystem, and scripting language.

**This project is in very early development.**

## Current Status

Monolith has a working desktop environment with overlapping windows:

- **Window Manager**: Dragging, 8-way resizing, title bars with close/min/max, z-order, focus, taskbar with XP-style window buttons + horizontal scrolling, and a functional Start menu.
- **Built-in Apps**:
  - **Terminal**: Real command-line app with scrollback, command history, and integrated commands for the internal filesystem (`ls`, `cd`, `cat`, `mkdir`, `rm`, etc.).
  - **Text Editor**: Functional multi-line editor with cursor movement, editing, and load/save via the internal filesystem (Ctrl+S).
  - **Filesystem Browser**: Graphical directory browser. Navigate folders, create/delete items, and double-click files to open them in the editor.
- **Internal Filesystem**: Host-backed (persisted under `~/.monolith/fs/`), with a clean virtual path namespace. Used by Terminal, Editor, and the graphical browser.
- Fixed-size outer window (1280×720) containing the full self-contained environment.

**This is still early** — no custom language yet, limited polish, and the set of apps is small. But the core "you can live inside this one program" feeling is starting to come together.

The project follows the spirit of the original vision: a personal, long-term, deliberately grown environment rather than a quick productivity tool.

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
./monolith
```

## Documentation

- [Vision & Philosophy](docs/vision.md) — Original long-term goals and design philosophy
- [Architecture](docs/architecture.md) — High-level technical structure

## License

See [LICENSE](LICENSE) for details. 