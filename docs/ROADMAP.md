# MONOLITH 1.0 Roadmap

Planning track for shipping a complete, production-ready personal mini-OS you can show in a portfolio.

Active chunk pointer: [`CURRENT_CHUNK`](../CURRENT_CHUNK) (also [`.agents/CURRENT_CHUNK`](../.agents/CURRENT_CHUNK)).

## Where we are

Tip at retarget time: `9fc16be` on `main` and `beta` (verify before tagging; tip may move).

**Already shipped**

- **Phase 4 (Living-inside-it)** done: Terminal quoted arguments, Settings UI scale, Editor horizontal scroll, Drawing eyedropper.
- **Phase 7 growth through 7.196** done: PNG/JPEG wallpaper, Breakout, desktop icons, `AppRegistry`, wallpaper fit, Start type-ahead, multi-column icons, then a long run of memory bounds, texture reuse, dirty-document guards, atomic-save hardening, and render allocation cleanup. Chunk **7.196** isolates atomic output buffering (`AtomicTempOutput.cpp`).
- **Desktop shell**: window manager (drag, 8-way resize, z-order, focus), taskbar with Start menu (Games category, type-ahead), desktop icons, Alt+Tab, Ctrl+Escape Start, multi-instance titles, session restore (`~/.monolith/session.txt`), extension-based open routing.
- **Apps**: Terminal, Text Editor, Filesystem Browser, Drawing, Settings, Snake, Minesweeper, Pong, Breakout.
- **Host VFS** under `~/.monolith/fs/` with atomic saves, advisory locking among Monolith writers, and documented limits for uncoordinated host writers.
- **CI**: GitHub Actions headless suite plus AddressSanitizer/UndefinedBehaviorSanitizer on pushes and PRs to `main`/`beta` (`.github/workflows/headless.yml`).

**Settings for 1.0**: ship as-is. Presets, typed wallpaper path (BMP/PNG/JPEG), fit modes, clock 12/24, UI scale 90/100/115. No extra appearance slice before the tag.

**Parked branch**: `wip/7.52-drawing-modr-io` stays parked. Do not merge it for 1.0.

## 1.0 goal

Ship a **complete, production-ready portfolio piece**: one Linux app that feels like a personal mini-OS (shell + apps + VFS + stability + polish). Someone can clone, build, run, and use a coherent desktop without reading agent notes.

This is not a language milestone. Custom language work does not belong in this repository.

## Milestones

### A. Retarget planning (this chunk: `1.0-docs-retarget`)

- Add this roadmap.
- Point `CURRENT_CHUNK` at the 1.0 track.
- Update public docs so the scripting language is clearly out of this repo (see Out of scope).
- Link this file from the docs hub.

### B. Surface freeze (`1.0-surface-freeze`)

- Freeze the current app set for the 1.0 tag.
- Keep `wip/7.52-drawing-modr-io` parked; do not merge.
- No new apps or open-ended Settings features before the tag.

### C. Stability bar (`1.0-stability-audit`)

- Headless suite green locally and in CI.
- Sanitizer job green (ASan/UBSan as in `headless.yml`).
- Only concrete polish from known debts (for example visible bugs or docs mismatches), not open-ended feature work.
- Keep the host-writer race as a **documented limitation**: Monolith atomic writers serialize on the destination directory lock; external tools that do not take that lock can still race between final validation and rename (see `docs/filesystem.md`).

### D. Release packaging (`1.0-release-packaging`)

- Bump CMake `project(... VERSION ...)` from `0.1` to `1.0`.
- Match Settings About version text (currently `0.1 (June 2026)` in the Settings info panel) to the 1.0 release string.
- README status line points here and reads as a shippable portfolio project, not "early experiment only."
- Add a short CHANGELOG 1.0 summary entry (file is large; land carefully or via a local checkout).
- Portfolio-facing README polish: build/run clarity; add screenshots under something like `docs/images/` if still missing at packaging time.
- Tag `v1.0.0` on the agreed tip after main and beta match.

## Out of scope for 1.0 (and for this repo)

| Item | Notes |
|------|--------|
| Phase 5 custom scripting language + interpreter | **Not in V1.** Lives in a **separate** repository (MONOLITH 2), built in Daniel's own language. Not a 2.0 of this tree. |
| Phase 6 IDE for that language | **Not in V1.** Same separate MONOLITH 2 repo when that work starts. |
| Networking / multiplayer | Out. |
| Bare-metal or "real kernel" claims | Monolith remains a normal Linux SDL2 application. |
| Open-ended Settings expansion | Ship current Settings; richer appearance stays post-1.0 soft polish at most. |
| Merging `wip/7.52-drawing-modr-io` | Parked; not part of the 1.0 surface. |

## Soft next chunks

Ordered for daily MONOLITH work after this docs land:

1. `1.0-docs-retarget` (this) - planning docs and chunk pointer.
2. `1.0-stability-audit` - headless + sanitize green; concrete polish only; host-writer race stays documented.
3. `1.0-surface-freeze` - confirm app set; keep parked drawing WIP off main.
4. `1.0-release-packaging` - version bumps, README/CHANGELOG/tag, screenshots if needed.

Optional after tag (not required for 1.0): small post-1.0 polish debts only when listed concretely in agent notes.

## Related docs

- [Documentation hub](README.md)
- [Vision](vision.md) (historical philosophy; language moved to MONOLITH 2)
- [Architecture](architecture.md)
- [Filesystem](filesystem.md)
- [Development scripts](development/scripts.md)
