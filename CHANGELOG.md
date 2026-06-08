# Changelog

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