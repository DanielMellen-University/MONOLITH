# AGENTS.md — Monolith Project Rules

This file provides guidance for AI agents (and human contributors) working on the Monolith codebase.

## Core Philosophy
- Monolith is a **personal, long-term, self-contained mini desktop environment**.
- Changes should feel coherent, solid, and "alive" even when many windows of the same type are open.
- Prefer centralized, simple mechanisms in the WindowManager / desktop shell over per-app complexity.
- Keep the outer binary small and the internal model clean.

## Window Manager & Multi-Instance Rules (Current)
- All new app types that can have multiple instances **must** go through `claimNextAppInstanceTitle(base)` (or the launcher pattern).
- Bare descriptive titles for content-driven windows (e.g. "Editor - foo") are fine and should **not** participate in the numeric pool for that base.
- On close of any tracked instance, the system **must** compact/renumber surviving windows of the same base so there are no gaps among live windows. Titles of open windows are expected to adjust (e.g. "Settings 2" becomes "Settings" when the primary is closed).
- Never mutate titles of unrelated windows. Preserve relative ordering when compacting.
- Apps that temporarily override a tracked title (e.g. Drawing after save) must restore the WM-managed instance name via `IWindowController::restoreTrackedInstanceTitle()` when clearing file state (New, etc.) — do not hardcode bare base names like `"Drawing"`.
- Update `src/window/WindowManager.cpp`, `Window.hpp`, launchers, and this file + architecture docs for any changes to instance management.
- Always keep `createWindow` backwards-compatible (the extra params have defaults).

## Documentation & Changelog Discipline

Follow these documentation boundaries:

- **Changelog**: Keep CHANGELOG.md short and focused on the active state of the project. Historical detail lives in git. When landing features, replace the top section with a clean summary of what was added (clear old bulk if requested).
- **Architecture** (`docs/architecture.md`): System design only — WM, app model, rendering, input, shell coordination. No per-app shortcut tables.
- **App guides** (`docs/apps/<app>.md`): User controls, shortcuts, file formats, limitations, and app-specific dev notes. Update the relevant app doc when changing app behavior.
- **Filesystem** (`docs/filesystem.md`): Shared virtual path rules and API usage. Update when path conventions or the `Filesystem` class change.
- **Root README**: Project overview, build/run, and links to `docs/README.md`. One-line status bullets only — no app-specific controls or dev scripts.
- **Docs hub** (`docs/README.md`): Link every new doc file from the hub table of contents.
- After docs + code changes, the working tree should build cleanly (`cd build && make`).

## Development Workflow
- Prefer editing through the provided tools (search_replace for precision, write for new/overwrite files).
- Rebuild and launch (`./build/monolith`) to verify, especially anything touching titles, taskbar, or close behavior.
- Use background launches when you want a persistent window for manual testing.
- When "submitting" work (as in this session), follow the pattern: update code, clear/focus changelog, update architecture + README, then commit.

## Commit / Submit Process
- Make changes.
- Update CHANGELOG.md (fresh, focused entry; old history may be cleared on request).
- Update relevant documentation (architecture.md at minimum for WM/shell changes).
- Optionally ensure AGENTS.md reflects any new rules.
- Stage and commit with a clear conventional message that references the main user-visible behavior change.
- Example:
  ```
  git add -A
  git commit -m "feat(wm): live dynamic compaction for multi-instance window titles

  - claimNextAppInstanceTitle + per-window metadata
  - compactAppInstances on close so open windows renumber (Settings 2 becomes Settings)
  - no more dups after close/reopen
  - updated changelog (cleared history) + architecture docs
  "
  ```
- Push or create worktree/PR as appropriate for the branch (currently beta in this worktree).

## Testing Expectations (Manual)
- Exercise multiple instances + close + reopen for every tracked type (Terminal, Filesystem, Drawing, Settings, bare Editor).
- Verify that closing a lower number causes higher ones to adjust their titles live.
- Check taskbar, title bars, Start menu, and focus behavior after renumbering.
- Mixed types should not interfere with each other's numbering.
- File editors remain independent.

## Non-Goals / Out of Scope for Agents
- Do not add persistence of instance numbers across Monolith restarts.
- Do not renumber unrelated window types.
- Keep the implementation small and inside WindowManager (the desktop shell owner).

If in doubt about instance management or shell behavior, re-read the "Instance Management for Multiple Windows of the Same Type" section in docs/architecture.md and the implementation in src/window/WindowManager.cpp.

Welcome to Monolith — keep it personal, solid, and fun to live inside.