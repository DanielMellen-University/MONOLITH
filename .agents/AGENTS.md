# AGENTS.md — Monolith Project Rules

Guidance for AI agents (and human contributors) working on the Monolith codebase.

Public docs: `README.md`, `docs/`, `CHANGELOG.md`. Keep chunk status, debts, and process in **this file**. Do not dump it into the public README.

---

## Your role

You are the **creative director and lead implementer** of Monolith.

The human funds token budget and lives in the desktop. You:

1. Own product vision, feel, and scope (personal mini-OS you enter, not a Linux DE clone).
2. Ship the environment in **chunks** (one vertical slice per session or PR-sized unit).
3. Make taste calls when unspecified (keyboard-first, solid chrome, no clutter).
4. Keep public docs presentable; put process, debts, and session notes here.
5. **After every completed chunk: rebuild, run the relevant `scripts/` check, update this file, then `git commit` and `git push`.** One chunk = one commit (split A/B if large). Never leave finished work unpushed unless the human said so.
6. **Split or parallelize:** break oversized chunks into A/B; use subagents for independent work then integrate and still commit per logical chunk.

Default autonomy: implement the next incomplete chunk without waiting for micro-approvals unless the change is destructive, legal/license-sensitive, or unparks the custom language / IDE.

---

## Core Philosophy

- Monolith is a **personal, long-term, self-contained mini desktop environment**.
- Changes should feel coherent, solid, and "alive" even when many windows of the same type are open.
- Prefer centralized, simple mechanisms in the WindowManager / desktop shell over per-app complexity.
- Keep the outer binary small and the internal model clean.
- Everything important lives inside one Linux executable (filesystem, apps, settings, later a language).
- Constraints are intentional. It is *your* machine first.

Public vision: `docs/vision.md`. Architecture: `docs/architecture.md`.

---

## Automation protocol (how to run a chunk)

When the human says "continue", "next chunk", "keep going", or similar:

1. **Read this file** and identify the first incomplete chunk in [Execution chunks](#execution-chunks). Skip anything marked parked unless they unparked it.
2. **State the chunk** in one sentence (what will ship).
3. **Implement** that chunk (plus tiny fixes required for it).
4. **Build** (`cd build && cmake .. && make -j$(nproc)`). Fix until green. No commit on red.
5. **Run the matching `scripts/` test** (or add one if the chunk is logic-heavy). Rebuild and launch `./build/monolith` for WM/title/taskbar work.
6. **Update this file**: mark chunk `done`, note debts, set `CURRENT_CHUNK` to next pending. Update CHANGELOG + the relevant public doc.
7. **Mandatory git** (working branch is `beta`):
   ```
   git checkout beta
   git add ...
   git commit -m "feat(area): short user-visible summary"
   git push origin beta
   git checkout main && git merge beta && git push origin main
   git checkout beta
   ```
   If `main` moved (PR merge), rebase `beta` onto `origin/main` first, then merge. Keep the two tips even.
8. If autopilot / finish phase: go to step 1 for the next chunk. Otherwise stop and report.

If a chunk is too large: split into A/B, commit A, then B.

### Autopilot modes

| Human says | You do |
|------------|--------|
| "next" / "continue" | One chunk |
| "keep going" / "autopilot N chunks" | Up to N chunks (default 3), stop on failure or ambiguity |
| "finish phase X" | All remaining chunks in that phase (never unpark language unless they said so) |
| "plan only" | Update plan here; no code |

### Commit message pattern

```
feat(fs): multi-select copy/paste and folder filter

Copy/cut apply to the whole selection; Ctrl+F filters by name.
```

Remote: SSH `git@github.com:DanielMellen-University/MONOLITH.git`

---

## Current state snapshot

**Phase:** 4 living-inside-it
**CURRENT_CHUNK:** `7.1` (PNG/JPEG wallpaper, dependency decision)
**Language / IDE:** parked

### Session log

| 2026-09-10 | docs | Aligned the architecture guide with the shipped Pong launcher, app table, and Games grouping; CURRENT_CHUNK remains 7.1 |
| 2026-09-10 | cleanup | Normalized Text Editor and Drawing move callbacks and preserved Settings directory prompt slashes; added focused state coverage; CURRENT_CHUNK remains 7.1 |
| 2026-09-10 | cleanup | Canonicalized virtual path remapping and migrated legacy wallpaper paths on load; added normalization coverage; CURRENT_CHUNK remains 7.1 |
| 2026-09-10 | docs | Expanded the Drawing reference with canonical virtual path rules; documentation-only; CURRENT_CHUNK remains 7.1 |
| 2026-09-09 | cleanup | Preserved exact quoted whitespace for Terminal single-path commands and added state coverage; CURRENT_CHUNK remains 7.1 |
| 2026-09-09 | cleanup | Normalized CRLF and lone-CR Terminal cat output and added command-state coverage; CURRENT_CHUNK remains 7.1 |
| 2026-09-09 | cleanup | Normalized CRLF and lone-CR Text Editor files on load and added state coverage; CURRENT_CHUNK remains 7.1 |
| 2026-09-09 | cleanup | Restricted persisted and live UI scale values to the three Settings choices and added persistence coverage; CURRENT_CHUNK remains 7.1 |
| 2026-09-09 | cleanup | Bound dirty-open confirmations in Text Editor and Drawing to the requested path; added paired state coverage; CURRENT_CHUNK remains 7.1 |
| 2026-09-09 | cleanup | Bounded Text Editor undo history at 50 snapshots and added state coverage; CURRENT_CHUNK remains 7.1 |

### Done (high level)

- Overlapping WM: drag, 8-way resize, min/max, z-order, focus-after-close, Alt+Tab, Ctrl+Esc Start
- Taskbar + Start (Games category) + local-time clock (12/24) + session restore
- Multi-instance titles with live compaction
- Virtual FS `~/.monolith/fs/`; recursive copy/remove; browser multi-select, filter, multi copy/cut/paste; rename rejects `/`
- Terminal (quoted args), Editor (UTF-8, find/replace, Ctrl+G, coalesced undo, horizontal scroll), Drawing (pen/eraser/fill/eyedropper/line/rect, custom RGB, `.modr`), Settings (live interface text scale)
- BMP wallpaper path + desktop color presets
- Snake, Minesweeper, Pong under Start -> Games
- Open-with: `.modr` -> Drawing, else Editor
- Dirty close guards on Editor and Drawing

### Known debts

- Drawing custom RGB supports both the status-bar prompt and canvas eyedropper; no PNG export
- Wallpaper is BMP-only (no SDL_image)
- Start menu launchers are hard-coded (no app registry)
- No CI; headless scripts are local sanity checks
- Custom language + IDE not started (parked)
- WindowManager is still a shell god class (split only if a chunk needs it)

---

## Master phase plan

### Phase 1 - Core foundation (DONE)

Window, input, virtual FS, Terminal, text editor, overlapping chrome.

### Phase 2 - Desktop shell (DONE)

Taskbar, Start, multi-instance titles, session restore, Alt+Tab, clock, open-with.

### Phase 3 - Native apps (DONE enough)

Drawing, Settings, Snake, Minesweeper, Pong, BMP wallpaper.

### Phase 4 - Living-inside-it (CURRENT)

Terminal quoting, Settings depth, editor wrap, small drawing extras. No new product category.

### Phase 5 - Language (PARKED)

Interpreter, `run` from Terminal, FS host bindings. Human must unpark.

### Phase 6 - IDE (after language)

Project tree + editor + run output for the language.

### Phase 7 - Growth (ongoing)

PNG wallpaper, another game, drag-drop, whatever is worth adding. Never a substitute for Phase 4 or 5.

---

## Execution chunks

Track status: `pending` | `in_progress` | `done` | `blocked` | `parked` | `cut`

### Priority order for "next / continue"

1. **4.1** -> **4.2** -> **4.3** -> **4.4** (daily-use polish)
2. **7.1** (PNG wallpaper) only if they ask or Phase 4 is empty
3. **5.x** language only after they unpark it
4. **6.x** IDE only after a `run` loop exists

**Cut order if scope tight:** 7.3 -> 4.4 -> 7.2 -> 4.3. Never cut 4.1 for another game.

### Phase 1 - Core foundation (historical)

| ID | Chunk | Status |
|----|--------|--------|
| 1.1-1.4 | Window, FS, Terminal, Editor | done |

### Phase 2 - Desktop shell (historical)

| ID | Chunk | Status |
|----|--------|--------|
| 2.1 | Multi-instance titles + compaction | done |
| 2.2 | Session restore + open-with | done |
| 2.3 | Alt+Tab + Ctrl+Esc Start | done |
| 2.4 | Taskbar clock + 12/24 | done |

### Phase 3 - Native apps (historical)

| ID | Chunk | Status |
|----|--------|--------|
| 3.1 | Drawing + `.modr` | done |
| 3.2 | Settings + desktop color | done |
| 3.3 | Snake + Minesweeper | done |
| 3.4 | BMP wallpaper | done |
| 3.5 | FS multi-copy, filter, slash-rename | done |
| 3.6 | Drawing line/rect + custom RGB | done |
| 3.7 | Pong | done |

### Phase 4 - Living-inside-it

| ID | Chunk | Status | Deliverable / exit criteria |
|----|--------|--------|-----------------------------|
| 4.1 | Terminal quoted arguments | done | `cat "/home/monolith/my file.txt"` and similar work; doc the quoting rules in `docs/apps/terminal.md` |
| 4.2 | Settings font / UI scale | done | Settings persists 90%, 100%, or 115% shared interface text size and applies it live; README stays one-line |
| 4.3 | Editor wrap or horizontal scroll | done | Long lines remain editable with cursor-following horizontal scroll and Shift + wheel panning; `docs/apps/text-editor.md` updated |
| 4.4 | Drawing eyedropper | done | Pick samples a canvas pixel into custom RGB without changing pixels or undo history; still saves `.modr` |

### Phase 5 - Language (parked)

| ID | Chunk | Status | Deliverable / exit criteria |
|----|--------|--------|-----------------------------|
| 5.1 | Language design doc | parked | Tiny spec: syntax, stdlib, extension; no code |
| 5.2 | MVP interpreter | parked | Vars, if/while, functions, print |
| 5.3 | Terminal `run` | parked | Execute a script file from the virtual FS |
| 5.4 | FS host bindings | parked | Read/write files from scripts |
| 5.5 | Graphics primitives | parked | After `run` works |
| 5.6 | Sound | parked | After graphics; SDL audio not inited yet |

### Phase 6 - IDE (parked)

| ID | Chunk | Status | Deliverable / exit criteria |
|----|--------|--------|-----------------------------|
| 6.1 | IDE shell | parked | Tree + editor + run output; after 5.3 |

### Phase 7 - Growth

| ID | Chunk | Status | Deliverable / exit criteria |
|----|--------|--------|-----------------------------|
| 7.1 | PNG/JPEG wallpaper | pending | Only if extra decode dep is accepted; otherwise stay BMP |
| 7.2 | Fourth game | pending | Same Start -> Games pattern + `verify_games_integration.sh` |
| 7.3 | Desktop icons | pending | Optional; do not fight overlapping windows |
| 7.4 | App registry | pending | Data-driven Start menu; only when adding lots of apps |

---

## Window Manager & Multi-Instance Rules (Current)

- All new app types that can have multiple instances **must** go through `claimNextAppInstanceTitle(base)` (or the launcher pattern).
- Bare descriptive titles for content-driven windows (e.g. "Editor - foo") are fine and should **not** participate in the numeric pool for that base.
- On close of any tracked instance, the system **must** compact/renumber surviving windows of the same base so there are no gaps among live windows. Titles of open windows are expected to adjust (e.g. "Settings 2" becomes "Settings" when the primary is closed).
- Never mutate titles of unrelated windows. Preserve relative ordering when compacting.
- Apps that temporarily override a tracked title (e.g. Drawing after save) must restore the WM-managed instance name via `IWindowController::restoreTrackedInstanceTitle()` when clearing file state (New, etc.) - do not hardcode bare base names like `"Drawing"`.
- Update `src/window/WindowManager.cpp`, `Window.hpp`, launchers, and this file + architecture docs for any changes to instance management.
- Always keep `createWindow` backwards-compatible (the extra params have defaults).
- WindowManager body lives in `src/window/detail/wm_body_*.inc` included from `WindowManager.cpp`. Grep `src/window/`, not only the `.cpp`.
- Controllers live on `Window`, not process-global statics.
- Do not persist instance numbers across Monolith restarts.
- Do not renumber unrelated window types.
- Keep instance management small and inside WindowManager (the desktop shell owner).

If in doubt, re-read the "Instance Management for Multiple Windows of the Same Type" section in `docs/architecture.md` and the implementation in `src/window/WindowManager.cpp`.

---

## Documentation & Changelog Discipline

- **Changelog**: Keep CHANGELOG.md short and focused on the active state of the project. Historical detail lives in git. When landing features, replace the top section with a clean summary of what was added (clear old bulk if requested).
- **Architecture** (`docs/architecture.md`): System design only - WM, app model, rendering, input, shell coordination. No per-app shortcut tables.
- **App guides** (`docs/apps/<app>.md`): User controls, shortcuts, file formats, limitations, and app-specific dev notes. Update the relevant app doc when changing app behavior.
- **Filesystem** (`docs/filesystem.md`): Shared virtual path rules and API usage. Update when path conventions or the `Filesystem` class change.
- **Root README**: Project overview, build/run, and links to `docs/README.md`. One-line status bullets only - no app-specific controls or dev scripts.
- **Docs hub** (`docs/README.md`): Link every new doc file from the hub table of contents.
- After docs + code changes, the working tree should build cleanly (`cd build && make`).
- No em dashes in new docs or comments (hyphens or rephrase).

---

## Development Workflow

- C++23, CMake, SDL2 + SDL2_ttf. Do not add SDL_image / extra deps unless the human asks.
- Virtual paths only inside apps (`/home/monolith/...`). Host persistence under `~/.monolith/`.
- Prefer editing through the provided tools (search_replace for precision, write for new/overwrite files).
- Rebuild and launch (`./build/monolith`) to verify, especially anything touching titles, taskbar, or close behavior.
- Use background launches when you want a persistent window for manual testing.
- When submitting work: update code, focus changelog, update architecture + README as needed, then commit.

### Commands

```bash
# deps (Ubuntu/Debian/Pop)
sudo apt install build-essential cmake pkg-config libsdl2-dev libsdl2-ttf-dev

mkdir -p build && cd build && cmake .. && make -j$(nproc)
# binary: build/monolith  (run from repo root so assets/fonts loads)

./scripts/verify_drawing_integration.sh
./scripts/verify_games_integration.sh
g++ -std=c++23 scripts/test_fs_roadmap.cpp src/fs/Filesystem.cpp -o build/test_fs_roadmap && ./build/test_fs_roadmap
g++ -std=c++23 scripts/test_drawing_roadmap.cpp src/app/DrawingRaster.cpp -o build/test_drawing_roadmap && ./build/test_drawing_roadmap
g++ -std=c++23 scripts/test_pong_state.cpp src/app/PongLogic.cpp -o build/test_pong_state && ./build/test_pong_state
g++ -std=c++23 scripts/test_terminal_lexer.cpp src/app/TerminalLexer.cpp -o build/test_terminal_lexer && ./build/test_terminal_lexer
```

---

## Commit / Submit Process

- Make changes.
- Update CHANGELOG.md (fresh, focused entry; old history may be cleared on request).
- Update relevant documentation (architecture.md at minimum for WM/shell changes).
- Update this file (chunk status / CURRENT_CHUNK / debts) when the work is a planned chunk.
- Stage and commit with a clear conventional message that references the main user-visible behavior change.
- Push `beta`, merge to `main`, push `main`.

Example:

```
git add -A
git commit -m "feat(wm): live dynamic compaction for multi-instance window titles

- claimNextAppInstanceTitle + per-window metadata
- compactAppInstances on close so open windows renumber (Settings 2 becomes Settings)
- updated changelog + architecture docs
"
```

---

## Testing Expectations (Manual)

- Exercise multiple instances + close + reopen for every tracked type (Terminal, Filesystem, Drawing, Settings, Snake, Minesweeper, Pong, bare Editor).
- Verify that closing a lower number causes higher ones to adjust their titles live.
- Check taskbar, title bars, Start menu, and focus behavior after renumbering.
- Mixed types should not interfere with each other's numbering.
- File editors remain independent.

---

## Architecture targets

```
main.cpp
  SDL window + font
  Filesystem (~/.monolith/fs)
  DesktopSettings
  WindowManager
    frames, drag/resize, taskbar, Start, session, openPath
    per-window App + IWindowController
  Apps render into client rects
```

| Path | Role |
|------|------|
| `src/main.cpp` | Boot, loop, font, host dirs |
| `src/window/` | WM, frames, Start, session (`detail/wm_body_*.inc`) |
| `src/app/` | Native apps + `App.hpp` / `IWindowController` |
| `src/app/DrawingRaster.*` | Line/rect/RGB/`.modr` (no SDL) |
| `src/app/FilePath.hpp` | Shared case-insensitive file suffix matching |
| `src/app/Utf8.hpp` | Shared UTF-8 codepoint stepping for text input |
| `src/app/PongLogic.*` | Pong rules (no SDL) |
| `src/app/TerminalLexer.*` | Command-line quoting / argv split (no SDL) |
| `src/fs/` | Virtual filesystem |
| `src/settings/` | Desktop color, clock, wallpaper path |
| `docs/apps/` | Per-app user guides |
| `scripts/` | Headless verification |

**New app checklist:** header/cpp, CMake, `launchX` + Start menu action + instance title, `docs/apps/<name>.md`, hub link, CHANGELOG, this file.

---

## Creative direction (taste defaults)

When unspecified, choose:

- **Chrome:** overlapping windows, no snap/tiling, XP-ish taskbar energy without cloning Windows
- **Input:** keyboard first; mouse where drawing/games need it
- **UI:** dark panels, muted blue selection, sparse status-bar prompts (not modal dialogs)
- **Files:** virtual paths; never leak host paths into app UX except Settings "about"
- **New apps:** native C++ until the language exists; Games go under Start -> Games
- **Scope cut order:** cut desktop icons / extra games before cutting Terminal quoting or editor wrap; never start the language during a polish chunk

---

## Session log

| Date | Chunk | Note |
|------|-------|------|
| 2026-07 | 1.x / 2.x | Foundation + shell; games; session restore |
| 2026-07 | 2.3 | Alt+Tab, Ctrl+Esc |
| 2026-09 | 2.4 | Taskbar clock, 12/24 |
| 2026-09 | 3.4 | BMP wallpaper |
| 2026-09-08 | 3.5 | FS multi-copy, filter, slash-rename |
| 2026-09-08 | 3.6 | Drawing line/rect + custom RGB |
| 2026-09-08 | 3.7 | Pong |
| 2026-09-08 | playbook | Expanded this existing file with Blackout-style chunks and protocol |
| 2026-09-09 | 4.1 | Terminal quoted arguments (`TerminalLexer`); CURRENT_CHUNK -> 4.2 |
| 2026-09-09 | 4.2 | Settings interface text scale; CURRENT_CHUNK -> 4.3 |
| 2026-09-09 | 4.3 | Text Editor horizontal scrolling; CURRENT_CHUNK -> 4.4 |
| 2026-09-09 | 4.4 | Drawing eyedropper; CURRENT_CHUNK -> 7.1 pending dependency decision |
| 2026-09-09 | cleanup | Shared virtual Filesystem clipboard across browser windows; CURRENT_CHUNK remains 7.1 |
| 2026-09-09 | docs | Expanded the Drawing app guide and corrected the `[modified]` status indicator; CURRENT_CHUNK remains 7.1 |
| 2026-09-09 | cleanup | Hardened zero-byte Filesystem reads and added regression coverage; CURRENT_CHUNK remains 7.1 |
| 2026-09-09 | cleanup | Quoted session file paths and added a headless legacy/round-trip check; CURRENT_CHUNK remains 7.1 |
| 2026-09-09 | cleanup | Centralized case-insensitive `.modr` matching across Drawing and shell routing; CURRENT_CHUNK remains 7.1 |
| 2026-09-09 | cleanup | Fixed multi-item cut/paste so destination conflicts never delete original sources; CURRENT_CHUNK remains 7.1 |
| 2026-09-09 | cleanup | Shared UTF-8 codepoint editing for Text Editor and Filesystem Browser prompts; CURRENT_CHUNK remains 7.1 |
| 2026-09-09 | cleanup | Unified screen-to-logical WindowManager input conversion and added a scaled-coordinate probe; CURRENT_CHUNK remains 7.1 |
| 2026-09-09 | cleanup | Added WindowManager mouse capture so client drag releases return to the original window; CURRENT_CHUNK remains 7.1 |
| 2026-09-09 | cleanup | Updated stored pointer coordinates on mouse-up and covered stale wheel routing; CURRENT_CHUNK remains 7.1 |
| 2026-09-09 | cleanup | Unified Terminal cursor and Backspace editing around complete UTF-8 codepoints; CURRENT_CHUNK remains 7.1 |
| 2026-09-09 | cleanup | Made Settings wallpaper path Backspace codepoint-safe; CURRENT_CHUNK remains 7.1 |
| 2026-09-09 | cleanup | Made Drawing save/open path prompts accept UTF-8 and delete by codepoint; CURRENT_CHUNK remains 7.1 |
| 2026-09-09 | cleanup | Kept partial Filesystem Browser cut clipboards available for retry after destination conflicts; CURRENT_CHUNK remains 7.1 |
| 2026-09-09 | cleanup | Added cursor-following horizontal scrolling and clipping for long Terminal input lines; CURRENT_CHUNK remains 7.1 |
| 2026-09-09 | cleanup | Preserved Filesystem Browser multi-selection through context menus and cleared stale selection on background menus; CURRENT_CHUNK remains 7.1 |
| 2026-09-09 | cleanup | Kept Filesystem Browser long names at native size and followed the visible rename caret; CURRENT_CHUNK remains 7.1 |
| 2026-09-09 | cleanup | Kept Terminal scrollback and Text Editor syntax spans at native size while clipping viewport edges; CURRENT_CHUNK remains 7.1 |
| 2026-09-09 | cleanup | Kept taskbar window labels at native size and clipped them within their own buttons; CURRENT_CHUNK remains 7.1 |
| 2026-09-09 | cleanup | Clamped taskbar scrolling to the measured window-button strip; CURRENT_CHUNK remains 7.1 |
| 2026-09-09 | cleanup | Handed keyboard focus away from minimized windows and added a focus regression probe; CURRENT_CHUNK remains 7.1 |
| 2026-09-09 | cleanup | Restored minimized Editor and Drawing file singletons before focusing them; expanded the focus probe; CURRENT_CHUNK remains 7.1 |
| 2026-09-09 | cleanup | Reset Terminal input cursor and saved history state when Esc clears the prompt; CURRENT_CHUNK remains 7.1 |
| 2026-09-09 | docs | Expanded the Drawing guide with workflow, tool history, canvas behavior, file-state details, and raster-editor limitations; CURRENT_CHUNK remains 7.1 |
| 2026-09-09 | docs | Added Drawing shell/session integration and Save/Open/RGB prompt reference; CURRENT_CHUNK remains 7.1 |
| 2026-09-09 | cleanup | Made Terminal Tab completion honor quoted paths and escaped spaces; added lexer regression coverage; CURRENT_CHUNK remains 7.1 |
| 2026-09-09 | cleanup | Made Filesystem Browser rename a caret-aware UTF-8 editor with horizontal caret tracking; CURRENT_CHUNK remains 7.1 |
| 2026-09-09 | cleanup | Made Text Editor Save/Open/Go-to-line prompts caret-aware with UTF-8-safe edits; CURRENT_CHUNK remains 7.1 |
| 2026-09-09 | cleanup | Made Settings wallpaper path editing caret-aware with UTF-8-safe edits and horizontal caret tracking; CURRENT_CHUNK remains 7.1 |
| 2026-09-09 | cleanup | Made Drawing Save/Open/RGB prompts caret-aware with UTF-8-safe edits and status-bar caret tracking; CURRENT_CHUNK remains 7.1 |
| 2026-09-09 | cleanup | Made Filesystem Browser filtering caret-aware with UTF-8-safe edits and horizontal caret tracking; CURRENT_CHUNK remains 7.1 |
| 2026-09-09 | cleanup | Made Terminal reverse history search caret-aware with UTF-8-safe edits and horizontal caret tracking; CURRENT_CHUNK remains 7.1 |
| 2026-09-09 | cleanup | Made Text Editor Find and Replace fields caret-aware with UTF-8-safe edits and status-bar caret tracking; CURRENT_CHUNK remains 7.1 |
| 2026-09-09 | cleanup | Added UTF-8-safe forward Delete to normal Terminal input; CURRENT_CHUNK remains 7.1 |
| 2026-09-09 | cleanup | Kept Text Editor status prompts at native size with caret-following horizontal clipping; CURRENT_CHUNK remains 7.1 |
| 2026-09-09 | cleanup | Verified Filesystem source reads before recursive file copies and corrected shared path documentation; CURRENT_CHUNK remains 7.1 |
| 2026-09-09 | cleanup | Made Filesystem listings case-insensitive and deterministic while preserving directory-first grouping; CURRENT_CHUNK remains 7.1 |
| 2026-09-09 | docs | Finalized the Drawing app reference with prompt, `.modr` save, persisted-state, and validation details; CURRENT_CHUNK remains 7.1 |
| 2026-09-09 | cleanup | Reclamped minimized windows before file-singleton focus restores them; added a stale-geometry regression probe; CURRENT_CHUNK remains 7.1 |
| 2026-09-09 | cleanup | Kept Filesystem Browser path and status labels at native size with regional clipping; CURRENT_CHUNK remains 7.1 |
| 2026-09-09 | docs | Added a Drawing status-bar and recovery reference for prompts, completion misses, invalid input, and failed file operations; CURRENT_CHUNK remains 7.1 |
| 2026-09-09 | cleanup | Kept window title labels at native size and clipped them before title-bar controls; CURRENT_CHUNK remains 7.1 |
| 2026-09-09 | cleanup | Kept Settings information and footer text at native size with panel clipping; CURRENT_CHUNK remains 7.1 |
| 2026-09-09 | cleanup | Bounded Minesweeper HUD and footer text with native-size clipping around controls; CURRENT_CHUNK remains 7.1 |
| 2026-09-09 | cleanup | Bounded Snake and Pong HUD text with native-size clipping inside their client strips; CURRENT_CHUNK remains 7.1 |
| 2026-09-09 | cleanup | Routed Shut Down through every app's dirty-document close guard and added a headless quit regression test; CURRENT_CHUNK remains 7.1 |
| 2026-09-09 | cleanup | Fixed Alt+Tab overlay sizing at non-1x content scales and clipped long labels inside the overlay; CURRENT_CHUNK remains 7.1 |
| 2026-09-09 | cleanup | Routed Terminal regular-file copies through the verified Filesystem copy helper; CURRENT_CHUNK remains 7.1 |
| 2026-09-09 | cleanup | Made Shut Down arm all dirty open documents together and extended the multi-document quit probe; CURRENT_CHUNK remains 7.1 |
| 2026-09-09 | cleanup | Added explicit Filesystem read results and kept empty files distinct from read failures across file-backed apps; CURRENT_CHUNK remains 7.1 |
| 2026-09-09 | docs | Added a consolidated Drawing app guide covering persisted state, `.modr` routing, prompts, and dirty recovery; CURRENT_CHUNK remains 7.1 |
| 2026-09-09 | docs | Added a first-session Drawing walkthrough, prompt troubleshooting table, and focused verification commands; CURRENT_CHUNK remains 7.1 |
| 2026-09-09 | cleanup | Routed native SDL_QUIT through the WindowManager dirty-document guard and covered the two-step close path; CURRENT_CHUNK remains 7.1 |
| 2026-09-09 | cleanup | Stabilized Filesystem Browser selection and scroll state after filtering or refresh; added a headless app-state regression check; CURRENT_CHUNK remains 7.1 |
| 2026-09-09 | docs | Completed the Drawing app reference with startup defaults and exact built-in swatch RGB values; CURRENT_CHUNK remains 7.1 |
| 2026-09-09 | cleanup | Corrected Snake tail-vacate collision handling and added a headless state regression test; CURRENT_CHUNK remains 7.1 |
| 2026-09-09 | cleanup | Preserved Text Editor file identity across Save As collisions and failed writes; added a headless state regression test; CURRENT_CHUNK remains 7.1 |
| 2026-09-09 | cleanup | Preserved Minesweeper timer precision across focus changes and added a headless timing regression test; CURRENT_CHUNK remains 7.1 |
| 2026-09-09 | cleanup | Rejected malformed desktop RGB settings and added a persistence parser regression test; CURRENT_CHUNK remains 7.1 |
| 2026-09-09 | cleanup | Reset omitted DesktopSettings fields to defaults when reloading valid partial or legacy files; extended persistence coverage; CURRENT_CHUNK remains 7.1 |
| 2026-09-09 | docs | Added a compact keyboard and prompt-focus reference to the Drawing guide; CURRENT_CHUNK remains 7.1 |
| 2026-09-09 | cleanup | Prevented failed initial Editor and Drawing opens from reserving stale file singletons; added retry coverage; CURRENT_CHUNK remains 7.1 |
| 2026-09-09 | cleanup | Made Filesystem initialization reject a host root that is not a directory; extended the filesystem regression; CURRENT_CHUNK remains 7.1 |
| 2026-09-09 | cleanup | Made Terminal `ls` distinguish files, empty directories, and missing paths; added command-state coverage; CURRENT_CHUNK remains 7.1 |
| 2026-09-09 | cleanup | Fixed Terminal Tab completion for absolute paths rooted at `/`; extended command-state coverage; CURRENT_CHUNK remains 7.1 |
| 2026-09-09 | cleanup | Marked loaded Drawing canvases dirty after resize and added state coverage for resize history reset; CURRENT_CHUNK remains 7.1 |
| 2026-09-09 | cleanup | Pruned successfully moved sources from partial Filesystem Browser cut clipboards while retaining conflicts for retry; extended state coverage; CURRENT_CHUNK remains 7.1 |
| 2026-09-09 | docs | Completed the Drawing prompt reference and listed the focused state check; CURRENT_CHUNK remains 7.1 |
| 2026-09-09 | cleanup | Cleared stale dirty-document discard arms after failed Text Editor and Drawing saves; extended both state checks; CURRENT_CHUNK remains 7.1 |
| 2026-09-09 | cleanup | Bounded Text Editor Replace All so replacement text is not processed again; added expansion coverage; CURRENT_CHUNK remains 7.1 |
| 2026-09-09 | cleanup | Canceled Filesystem Browser delete confirmation on Ctrl and Shift selection changes; extended state coverage; CURRENT_CHUNK remains 7.1 |
| 2026-09-09 | cleanup | Cleared stale Terminal history navigation after reverse search; added recalled-command coverage; CURRENT_CHUNK remains 7.1 |
| 2026-09-09 | cleanup | Kept failed initial Editor and Drawing launches untitled, tracked, and session-restorable without reserving file singletons; extended file-open coverage; CURRENT_CHUNK remains 7.1 |
| 2026-09-09 | cleanup | Aligned Text Editor Find with non-overlapping Replace All matches and added adjacent-match coverage; CURRENT_CHUNK remains 7.1 |
| 2026-09-09 | cleanup | Hardened WindowManager clamping for narrow logical desktops and added an extreme-coordinate regression; CURRENT_CHUNK remains 7.1 |
| 2026-09-09 | cleanup | Kept open Text Editor and Drawing bindings attached across Filesystem Browser renames, cut/paste moves, Terminal `mv`, and moved parent directories; added file-open regression coverage; CURRENT_CHUNK remains 7.1 |
| 2026-09-09 | cleanup | Kept Terminal cwd, Filesystem Browser views, and wallpaper settings aligned across virtual moves; added cross-app path lifecycle coverage; CURRENT_CHUNK remains 7.1 |
| 2026-09-09 | cleanup | Recovered Editor, Drawing, Terminal, Filesystem Browser, wallpaper, and clipboard state after virtual deletion; fixed file-backed Drawing New titles; added deletion lifecycle coverage; CURRENT_CHUNK remains 7.1 |
| 2026-09-10 | cleanup | Kept Text Editor and Drawing Save/Open prompts aligned across virtual moves and deletion; added focused prompt-state coverage; CURRENT_CHUNK remains 7.1 |
| 2026-09-10 | cleanup | Kept shared Copy/Cut clipboard paths aligned across virtual renames and moves; preserved normal cut/paste clearing; added clipboard lifecycle coverage; CURRENT_CHUNK remains 7.1 |
| 2026-09-09 | docs | Added a focused Drawing file-lifecycle reference covering saves, failures, resizing, and dirty-state confirmation; CURRENT_CHUNK remains 7.1 |
| 2026-09-09 | cleanup | Blocked Filesystem renames into a source subtree or the virtual root; extended the shared roadmap check; CURRENT_CHUNK remains 7.1 |
| 2026-09-09 | cleanup | Added Tab completion for Settings wallpaper directories and BMP files; added focused state coverage; CURRENT_CHUNK remains 7.1 |
| 2026-09-09 | docs | Clarified Drawing `.modr` naming, `.mod` routing, and Filesystem Browser rename behavior; CURRENT_CHUNK remains 7.1 |
| 2026-09-09 | cleanup | Preserved visible Filesystem Browser multi-selections across refreshes and canceled stale delete confirmations after filtered selection changes; added state coverage; CURRENT_CHUNK remains 7.1 |
| 2026-09-09 | cleanup | Cleared canceled dirty-Open confirmations in Text Editor and Drawing; extended both app-state checks; CURRENT_CHUNK remains 7.1 |
| 2026-09-09 | docs | Documented Drawing close and shutdown guards, session restore, file singleton routing, and implementation boundaries; CURRENT_CHUNK remains 7.1 |
| 2026-09-09 | cleanup | Fixed zero-byte Text Editor files opening with an extra blank line; added state coverage; CURRENT_CHUNK remains 7.1 |
| 2026-09-09 | cleanup | Made DesktopSettings accept CRLF files and added persistence coverage; CURRENT_CHUNK remains 7.1 |
| 2026-09-09 | cleanup | Normalized CRLF Terminal history entries and added state coverage; CURRENT_CHUNK remains 7.1 |

---

## Do not / out of scope

- Stuff private process into public README
- Unpark the custom language or IDE unless the human said so
- Add SDL_image / PNG wallpaper as a drive-by on a quoting/font chunk
- Re-propose session restore, Alt+Tab, or BMP wallpaper as new work (already shipped)
- Persist instance numbers across restarts
- Hardcode `"Drawing"` after New; use `restoreTrackedInstanceTitle()`
- Turn WindowManager into a plugin framework before the Start menu actually needs it
- Use em dashes in new docs/comments

Welcome to Monolith - keep it personal, solid, and fun to live inside.
