# Roadmap

Status of each milestone, as tick boxes: [1.0 checklist](1.0-CHECKLIST.md).

## 1.0: shipped (October 2026)

The goal of 1.0 was a complete, presentable portfolio piece: one Linux app that feels like a personal mini-OS, which someone can clone, build, run and use without reading any internal notes.

What 1.0 contains:

- **Desktop shell.** Overlapping windows with drag, 8-way resize, minimize, maximize and focus. Taskbar with Start menu (Games category, type-ahead filter) and clock. Desktop icons, Alt+Tab, Ctrl+Escape, numbered window titles, session restore, and open-by-file-type routing.
- **Nine apps.** Terminal, Text Editor, Filesystem, Drawing, Settings, Snake, Minesweeper, Pong, Breakout.
- **Internal filesystem** under `~/.monolith/fs/` with atomic saves.
- **Tests and CI.** A headless suite of 34 test programs and 5 static checks, run in GitHub Actions on every push to `main` and `beta`, both normally and under AddressSanitizer and UndefinedBehaviorSanitizer.
- **Version 1.0** reported by CMake, the Settings app (`1.0 (October 2026)`) and the Terminal `version` command. The string lives in `src/app/Version.hpp`.
- **Tag `v1.0.0`** on the commit where `main` and `beta` match.

Not included: screenshots. The checklist keeps that item open.

### Milestones

| Milestone | Chunk | Outcome |
|-----------|-------|---------|
| A. Retarget planning | `1.0-docs-retarget` | Added this roadmap and moved the chunk pointer to the 1.0 track |
| B. Surface freeze | `1.0-surface-freeze` | App set frozen at nine; Settings shipped as it was; parked branch left unmerged |
| C. Stability | `1.0-stability-audit` | Headless and sanitizer CI green on the release commit; zero compiler warnings; CI package install bounded with timeouts and retries |
| D. Release packaging | `1.0-release-packaging` | Version strings, README, CHANGELOG entry, tag |

Two decisions from the 1.0 work:

- **The host-writer race stays a documented limitation.** MONOLITH's own writers serialize on a directory lock, but an outside program that ignores that lock can still replace a file between MONOLITH's last check and its rename. See [atomic-writes.md](internals/atomic-writes.md#the-host-writer-race).
- **`wip/7.52-drawing-modr-io` stays parked.** It is not part of 1.0 and should not be merged into it.

## Out of scope

| Item | Decision |
|------|----------|
| A custom programming language | A separate future project, MONOLITH 2, in its own repository |
| Open-ended Settings growth | Settings shipped as it is. More options would be small post-1.0 polish at most. |

## After 1.0

No chunk is required after the tag. Post-1.0 work is limited to small, concrete fixes, for example:

- Add screenshots under `docs/images/`.
- Commit `assets/wallpapers/sample.png` (or generate it during the build) so `/Wallpapers/sample.png` is seeded on first launch. Today only the BMP sample is created; see [settings.md](apps/settings.md#wallpaper-image).

## How this list was built

Work was done in small chunks. The active chunk name is kept in [`CURRENT_CHUNK`](../CURRENT_CHUNK) (copy in [`.agents/CURRENT_CHUNK`](../.agents/CURRENT_CHUNK)), which now reads `1.0-released`. Earlier phases:

| Phase | Content |
|-------|---------|
| 1. Core | Window, filesystem, Terminal, Text Editor |
| 2. Desktop shell | Numbered titles, session restore, open-with, Alt+Tab, Ctrl+Escape, clock |
| 3. Native apps | Drawing, Settings, Snake, Minesweeper, BMP wallpaper, Pong |
| 4. Living in it | Terminal quoting, text size setting, editor horizontal scroll, Drawing color picker |
| 7. Growth | PNG and JPEG wallpaper, Breakout, desktop icons, app registry, wallpaper fit, Start type-ahead, then a long run of memory bounds, texture reuse, unsaved-work guards and atomic-save hardening (chunks up to 7.196) |

Phases 5 and 6 of the original plan were moved out of this repository.
