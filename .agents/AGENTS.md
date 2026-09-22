# AGENTS.md - Monolith Project Rules

Guidance for AI agents (and human contributors) working on the Monolith codebase.

Public docs: `README.md`, `docs/`, `CHANGELOG.md`. Keep chunk status, debts, and process in **this file**. Do not dump it into the public README.

Companion files: [`CURRENT_CHUNK`](CURRENT_CHUNK), [`SESSION_LOG.md`](SESSION_LOG.md), [`HISTORY.md`](HISTORY.md).

---

## Your role

You are the **creative director and lead implementer** of Monolith.

The human funds token budget and lives in the desktop. You:

1. Own product vision, feel, and scope (personal mini-OS you enter, not a Linux DE clone).
2. Ship the environment in **chunks** (one vertical slice per session or PR-sized unit).
3. Make taste calls when unspecified (keyboard-first, solid chrome, no clutter).
4. Keep public docs presentable; put process, debts, and session notes here.

## Automation protocol (how to run a chunk)

1. Read this file + architecture + the touched app guide.
2. Implement the chunk with real code (no `__LOAD_FROM__` stubs).
3. Build green; add focused tests when practical.
4. Update CHANGELOG + relevant public docs.
5. Mark chunk `done`, note debts, set `CURRENT_CHUNK` to next pending (also update `.agents/CURRENT_CHUNK`).
6. Prefer 3-5 logical commits on `beta`, then PR merge to `main`.

## Current state snapshot

**CURRENT_CHUNK:** `await-5.x-unpark` (Phase 5 language parked until unpark)

| When | Kind | Note |
|------|------|------|
| 2026-09-22 | 7.6 | Start menu type-ahead filter; CURRENT_CHUNK back to await-5.x-unpark |
| 2026-09-21 | 7.5 | Wallpaper fit cover/contain/center; CURRENT_CHUNK back to await-5.x-unpark |
| 2026-09-20 | shell-polish | Start menu hit targets share AppRegistry rows; hardcoded entries table retired; CURRENT_CHUNK now await-5.x-unpark |
| 2026-09-17 | 7.4 | App registry drives Start menu, desktop icons, session kinds; CURRENT_CHUNK now shell-polish |
| 2026-09-15 | 7.3 | Desktop icons under wallpaper; windows win hit-testing; CURRENT_CHUNK now 7.4 |
| 2026-09-14 | 7.2 | Breakout under Start -> Games; CURRENT_CHUNK now 7.3 |
| 2026-09-11 | 7.1 | PNG/JPEG wallpaper via pinned stb_image; Settings/WM accept .bmp/.png/.jpg/.jpeg; CURRENT_CHUNK now 7.2 |

Older recent-work rows: [`SESSION_LOG.md`](SESSION_LOG.md).

## Priority order for "next / continue"

1. **5.x** language only after they unpark it (`await-5.x-unpark`)
2. **6.x** IDE only after a `run` loop exists
3. Further shell soft polish only when a concrete debt is listed

**Cut order if scope tight:** never cut core shell work for another game.

## Phase 4 - Living-inside-it

| ID | Chunk | Status | Deliverable / exit criteria |
|----|--------|--------|-----------------------------|
| 4.1 | Terminal quoted arguments | done | `cat "/home/monolith/my file.txt"` and similar work; doc the quoting rules in `docs/apps/terminal.md` |
| 4.2 | Settings font / UI scale | done | Settings persists 90%, 100%, or 115% shared interface text size and applies it live; README stays one-line |
| 4.3 | Editor wrap or horizontal scroll | done | Long lines remain editable with cursor-following horizontal scroll and Shift + wheel panning; `docs/apps/text-editor.md` updated |
| 4.4 | Drawing eyedropper | done | Pick samples a canvas pixel into custom RGB without changing pixels or undo history; still saves `.modr` |

## Phase 5 - Language (parked)

| ID | Chunk | Status |
|----|--------|--------|
| 5.1-5.6 | Language design through sound | parked |

## Phase 6 - IDE (parked)

| ID | Chunk | Status |
|----|--------|--------|
| 6.1 | IDE shell | parked |

## Phase 7 - Growth

| ID | Chunk | Status | Deliverable / exit criteria |
|----|--------|--------|-----------------------------|
| 7.1 | PNG/JPEG wallpaper | done | stb_image build-time fetch; BMP via SDL_LoadBMP; PNG/JPEG via WallpaperImage |
| 7.2 | Fourth game (Breakout) | done | Same Start -> Games pattern + `verify_games_integration.sh` |
| 7.3 | Desktop icons | done | Left-column icons under wallpaper; windows win hit-testing; see `docs/notes/7.3-desktop-icons.md` |
| 7.4 | App registry | done | `AppRegistry` drives Start menu, desktop icons, session kinds; see `docs/notes/7.4-app-registry.md` |
| shell-polish | Start menu hit unify | done | Hit targets rebuild from AppRegistry rows; hardcoded entries table retired; see `docs/notes/shell-polish.md` |
| 7.5 | Wallpaper fit | done | cover/contain/center display modes; see `docs/notes/7.5-wallpaper-fit.md` |
| 7.6 | Start menu type-ahead | done | filter by label while open; see `docs/notes/7.6-start-menu-typeahead.md` |

## Commit voice

`feat|fix|docs|chore(scope): imperative`. No em dashes / AI filler.

## Wallpaper dependency (7.1)

Prefer pinned `stb_image` fetch (`third_party/stb/`) over `SDL_image`. See `third_party/stb/README.md` and `docs/notes/7.1-wallpaper-decode.md`.

## Do not

- Placeholders / `__LOAD_FROM__` stubs
- Cloud Agents when unavailable; push via user-Github MCP
- Force-push main; work on beta then PR
