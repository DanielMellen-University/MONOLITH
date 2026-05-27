# Monolith

A personal mini operating system that runs as one application.

Monolith is an experimental, self-contained environment written in C++ using SDL2. It aims to feel like a small personal operating system inside a single Linux program, with its own window manager, apps, filesystem, and scripting language.

**This project is in very early development.**

## Current Status

The project is in the initial skeleton stage. Right now it can:

- Open a resizable window
- Run a basic event loop

There is no window manager, no apps, no filesystem, and no language yet.

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