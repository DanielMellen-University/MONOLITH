# Changelog

## 2026-06: Window Geometry Cleanup

Focused Window Manager cleanup to keep taskbar, usable desktop, and title-bar button geometry consistent.

- Added shared geometry helpers for the taskbar rectangle, usable desktop rectangle, usable desktop bottom edge, logical-to-screen rect conversion, and title-bar button hitboxes.
- Window creation now clamps new windows into the usable desktop area before app resize notification.
- Maximized windows now use the same usable desktop rectangle as drag and resize clamping.
- Title-bar button rendering and click handling now use the same rectangle calculations, reducing drift between what is drawn and what is clickable.
- Start menu and taskbar rendering now use the shared taskbar geometry helper.

Builds cleanly.

## 2026-06: Terminal Command Execution Polish + Path Logic Hardening

Focused updates to Terminal (the primary command/FS interaction surface) and cross-consistency with the graphical Filesystem browser.

- Command parsing upgraded: simple but reliable `vector<string> args` splitter (whitespace) replaces the old cmd + raw "rest" getline. Enables proper flag/operand handling without changing behavior of untouched commands.
- `cp` now supports `-r`/`-rf` for recursive directory copies and follows standard Unix dst-dir semantics (if dst resolves to a directory, the source basename is placed inside it). Uses new private `copyRecursive` helper.
- `rm` flag detection is now robust (scans args for `-r`/`-rf` instead of brittle `substr` prefix checks on the remainder).
- `mv` gains the same dst-as-directory behavior.
- Path construction centralized/hardened inside Terminal:
  - New private `joinPath(base, name)` (glue + `Filesystem::normalize`) used by `resolvePath`, `removeRecursive`, cp/mv dst logic, etc. Removed ad-hoc `back() == '/' ? "" : "/"` gluing in FS-path sites.
  - `removeRecursive` refactored to use the helper.
- FilesystemApp path logic updated for consistency (no FS public API changes):
  - `goUp()` now computes parent via `.../..` + `normalize` (more robust than raw `find_last_of` + substr).
  - `fullPathFor` (and thus all creation, delete, rename, activate, etc. targets) always goes through `normalize` after construction.
- Help text updated to document the new `cp [-r]` and `mv`/`rm` behaviors.
- All changes are Terminal-private or use only existing public `Filesystem` surface (`normalize`, `listEntries`, `is*`, read/write/create/remove/rename). No impact on other apps, WM, or instance titling.

Builds cleanly and launches. Full verification (including mixed Terminal + GUI sessions, `cp -r` on nested trees, dst-dir cases, `rm -r`, `..` paths, tab completion, multiple windows) performed manually in the running environment.

## 2026-06: Dynamic Multi-Window Instance Titling with Live Compaction

Major improvements to the Window Manager for robust handling of multiple instances of the same app type (Terminal, Filesystem, Settings, bare Editor).

### Key Features
- `claimNextAppInstanceTitle(base)` allocates the lowest available instance number per app type.
  - Returns the canonical display title and the number.
  - Primary instance uses the bare name ("Terminal", "Settings"); additional use "Type 2", "Type 3", etc.
- `Window` struct now carries `appBaseTitle` and `appInstanceNumber` metadata (internal to WM).
- `createWindow` extended (back-compat) to accept and record the tracking info.
- All launchers (`launchTerminal`, `launchFilesystem`, `launchSettings`, bare `launchTextEditor`) use the claimer for consistent per-type numbering.
- File-backed Text Editor windows continue to use descriptive titles ("Editor - welcome.txt") and are excluded from the bare "Editor" numbering pool (protected by the existing singleton mechanism via `m_fileEditors`).

### Live Dynamic Renumbering / Compaction
- On close of a tracked window, `compactAppInstances(base)` runs over all *remaining* live windows of that type.
- They are re-numbered contiguously from 1 (sorted by previous instance number to preserve relative order).
- Titles are mutated in place on the `Window` objects.
- Title caches are invalidated so title bars and the XP-style taskbar update on the next frame.
- The active set is rebuilt.
- Result: no gaps and no duplicate titles among currently open windows.
  - Example: Open "Settings", open "Settings 2"; close the primary "Settings" → the second window's title automatically becomes plain "Settings".
  - Closing any instance compacts the survivors (e.g. 1+2+4 becomes 1+2 after closing 3, etc.).

- Old snapshot-based `makeInstanceTitle` counting logic removed.
- State lives only for the duration of a Monolith session (numbers reset on restart of the outer app, as intended).

This makes long-running personal sessions with many overlapping windows of the same types feel solid and predictable.

See:
- `src/window/WindowManager.{hpp,cpp}` (claimNextAppInstanceTitle, compactAppInstances, updates to createWindow/closeWindow/launchers)
- `src/window/Window.hpp`
- Updated architecture docs and this changelog (historical entries cleared for a fresh start on the current feature set).

## Previous History

Historical detailed entries from earlier development (Advanced Filesystem Browser, Terminal/Editor polish, initial Start Menu + Settings, App hosting model, etc.) have been removed to keep the log focused on the active state of the project. Refer to git history for prior details if needed.
