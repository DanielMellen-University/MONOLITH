# MONOLITH

## A Personal Digital Cathedral for Linux

**Monolith** is a self-contained, retro-inspired programming environment built on Linux.  
It is not a general-purpose operating system, not a desktop replacement, and not a productivity suite.

Monolith is a **deliberate computational space** — a place to write code, generate visuals, consult oracles, and think clearly inside a constrained but expressive system.

You do not multitask inside Monolith.  
You enter it.

---

## Core Principles

- Single executable, single folder  
- No installation, no root access  
- No background services  
- No hidden state  
- Immediate feedback  
- Intentional constraints  
- The machine as an instrument, not a distraction  

Monolith is designed to feel solid, quiet, and permanent.

---

## Target Platform

- **Host OS:** Ubuntu 22.04 LTS or newer  
  (Also compatible with Pop!_OS, Linux Mint, Debian, Arch, Fedora)
- **Architecture:** x86_64  
- **Distribution model:** Copy folder → run executable  

---

## Display & Resolution Model

Monolith supports **two internal render modes**:

- **1920 × 1080 (1080p)**
- **2560 × 1440 (1440p)**

### Automatic Monitor Detection

- At startup, Monolith detects the resolution of the monitor it is displayed on  
- The closest supported render mode is selected automatically  
- When the window is moved to another monitor, Monolith re-evaluates the display and switches modes if necessary  
- Display changes (resolution changes, docking/undocking, multi-monitor setups) are handled at runtime  

### Rendering Strategy

- Monolith renders into a **fixed internal framebuffer** (1080p or 1440p)  
- The framebuffer is scaled to the actual window size on presentation  
- Nearest-neighbor or controlled scaling is used to preserve sharp visuals  
- Aspect ratio is always preserved (letterboxing if required)  

This guarantees consistent layout, predictable visuals, and a stable mental space.

---

## Color Model

Monolith uses **full 24-bit RGB color**.

- Colors are specified using **hex values** (`#RRGGBB`)  
- No artificial color limits  
- No enforced palette size  
- Optional predefined palettes may exist for stylistic or thematic use  

The visual constraint of Monolith comes from **resolution and layout**, not color depth.

---

## Folder Layout

```
Monolith/
├── monolith              ← Single executable
├── assets/
│   ├── fonts/            ← Bitmap or TTF fonts
│   ├── sounds/           ← Raw beeps, tones, samples
│   └── images/           ← Optional textures or references
├── scripts/              ← Monolith script source files (*.mc)
├── projects/             ← Saved worlds, art, experiments
└── data/                 ← Oracle word lists, texts, configuration
```

Everything Monolith needs lives inside this directory.

---

## Technology Stack

### Core Language
- **C++23**
  - Explicit control
  - Predictable performance
  - Native Linux support

### Libraries
- **SDL2** – Windowing, input, audio, display management  
- **OpenGL** (via glad) – Graphics backend  
- **Dear ImGui** – Immediate-mode tooling and shell UI  
- **miniaudio** – Low-level audio and synthesis  
- **stb_image / stb_truetype** – Asset loading  
- **TinyCC (optional)** – Native code hot-reloading at later stages  

The long-term goal is minimal runtime dependencies beyond the Linux kernel.

---

## Interaction Model

- Keyboard-first interface  
- Immediate-mode shell  
- Integrated editor  
- Software-rendered text and primitives  
- No window chrome or OS-level UI metaphors inside the environment  

Monolith does not imitate modern desktops.

---

## Scripting Language

**Monolith Script** (`.mc`)

- C-like syntax  
- Immediate execution  
- Designed for drawing, sound, procedural generation, and oracles  

Example:

```c
U0 OraclePrayer() {
  Clear(#000000);
  PrintCenter("SPEAK", #FFFFFF);
  WaitKey();
  Oracle(50);
}

U0 Main() {
  SetMode(1440);
  OraclePrayer();
  DrawCathedral();
}
```

---

## Development Environment

### One-Time Setup (Ubuntu)

```bash
sudo apt update
sudo apt install build-essential cmake ninja-build git                  libsdl2-dev libgl1-mesa-dev clang
```

### Build System

- CMake + Ninja  
- Reproducible builds  
- No IDE required  

---

## Distribution & Portability

- Runs on any modern Linux distribution  
- No installer  
- No system integration  
- Can be copied to USB drives or shared folders  
- Optional AppImage packaging in the future  

Monolith is meant to be carried.

---

## Roadmap

### Phase 1 – Awakening the Stone
- SDL2 window  
- Automatic resolution detection  
- 1080p / 1440p render modes  
- Immediate-mode shell  
- Text editor  
- Oracle command  
- PC-speaker-style startup tone  

### Phase 2 – The Inner Chamber
- Bitmap font renderer  
- Console scrollback  
- Drawing primitives  
- Palette and color utilities  
- Command scripting  

### Phase 3 – The Word
- Monolith Script interpreter  
- File execution  
- Hot reload  
- Procedural graphics and sound  

---

## Final Note

Monolith is not designed to scale endlessly.  
It is designed to **hold attention**.

If you want convenience, use a modern desktop.  
If you want focus, enter the stone.
