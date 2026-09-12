# AGENTS.md — Monolith Project Rules

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
6. Prefer 3–5 logical commits on `beta`, then PR merge to `main`.

## Current state snapshot

**CURRENT_CHUNK:** `7.2` (Fourth game)

| When | Kind | Note |
|------|------|------|
| 2026-09-11 | 7.1 | PNG/JPEG wallpaper via pinned stb_image; Settings/WM accept .bmp/.png/.jpg/.jpeg; CURRENT_CHUNK now 7.2 |

Older recent-work rows: [`SESSION_LOG.md`](SESSION_LOG.md).

## Priority order for "next / continue"

1. **7.2** (Fourth game)
2. **7.3** / **7.4** only if requested
3. **5.x** language only after they unpark it
4. **6.x** IDE only after a `run` loop exists

**Cut order if scope tight:** 7.3 -> 7.4 -> 7.2. Never cut core shell work for another game.

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
| 5.1–5.6 | Language design through sound | parked |

## Phase 6 - IDE (parked)

| ID | Chunk | Status |
|----|--------|--------|
| 6.1 | IDE shell | parked |

## Phase 7 - Growth

| ID | Chunk | Status | Deliverable / exit criteria |
|----|--------|--------|-----------------------------|
| 7.1 | PNG/JPEG wallpaper | done | stb_image build-time fetch; BMP via SDL_LoadBMP; PNG/JPEG via WallpaperImage |
| 7.2 | Fourth game | pending | Same Start -> Games pattern + `verify_games_integration.sh` |
| 7.3 | Desktop icons | pending | Optional; do not fight overlapping windows |
| 7.4 | App registry | pending | Data-driven Start menu; only when adding lots of apps |

## Commit voice

`feat|fix|docs|chore(scope): imperative`. No em dashes / AI filler.

## Wallpaper dependency (7.1)

Prefer pinned `stb_image` fetch (`third_party/stb/`) over `SDL_image`. See `third_party/stb/README.md` and `docs/notes/7.1-wallpaper-decode.md`.

## Do not

- Placeholders / `__LOAD_FROM__` stubs
- Cloud Agents when unavailable; push via user-Github MCP
- Force-push main; work on beta then PR
