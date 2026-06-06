# Changelog

## Release - Advanced Filesystem Browser + Terminal & Editor Polish (June 2026)

- **Filesystem Browser** — Major upgrade:
  - New File creation (toolbar button + context menu) with immediate inline rename.
  - Full right-click context menu (Open, Rename, Delete for items; New Folder / New File / Refresh on background).
  - Inline rename (F2 or menu) with live editing and conflict prevention.
  - Two-step delete confirmation for safety.
  - Status bar showing item count and selection.
  - Backend `Filesystem::rename()` support added.

- **Terminal** — Significant quality-of-life improvements:
  - Proper cursor editing (arrows, Home/End, insert at position).
  - Full tab completion for commands and filesystem paths.
  - `cp`, `rm -r` / `rm -rf`, `touch`, `mv` commands.
  - Dynamic prompt showing current working directory.
  - Persistent command history saved to the virtual filesystem.
  - Robust history navigation with safety clamps.

- **Text Editor**:
  - Undo stack (Ctrl+Z, bounded history).
  - Line number gutter with correct alignment.
  - Find mode (Ctrl+F) with live search, highlighted matches, match count, next/previous navigation, and status-line hints.

- Various robustness fixes, better input routing between modes (rename, new file naming, context menu), and documentation updates.

This release brings the core personal environment to a much more usable state.

---

## Latest Changes

### Cleanup And Correctness Pass

- **Window Manager**:
  - Dragging and resizing now keep windows inside the usable desktop area above the taskbar.

- **Text Editor**:
  - Delete now participates correctly in undo when removing text or joining lines.
  - Backspace and Delete no longer create undo states or dirty changes when there is nothing to edit.

- **Filesystem Browser**:
  - Removed leftover debug logging from right-click and context menu rendering.
  - Context menu hover and click handling now use the same clamped rectangle that is drawn on screen.
  - Delete confirmation state is cleared when the menu closes, preventing stale confirmation behavior.
  - Status bar now reports create, rename, delete, refresh, navigation, and failure outcomes.
  - Refresh and file operations preserve or clamp selection more predictably after the entry list changes.
  - Removed unused new-file naming state now that New File uses the inline rename path.
  - Replaced repeated browser layout numbers with named constants to keep rendering and input math aligned.

### Text Editor Find Mode

- **Find mode**: Ctrl+F opens an inline find state in the editor status line.
- **Live matching**: Typing a query updates matches immediately and jumps to the first match at or after the cursor.
- **Navigation**: Enter moves to the next match; Shift+Enter moves to the previous match; Esc closes find mode.
- **Highlighting**: All visible matches are highlighted, with a stronger but readable current-match color.
- **Status hint**: The normal editor status line now advertises Ctrl+F find alongside Ctrl+S save.

### Terminal Completion & Polish

- **Tab completion**: Full tab completion for built-in commands and filesystem paths (with `/` appended for directories).
- **`cp` command**: Copy files between paths in the virtual filesystem.
- **`rm -r` support**: Recursive directory removal via `rm -r` (or `rm -rf`).
- **Improved prompt**: The prompt now displays the current working directory (e.g. `~> ` or `~/projects> `) and updates live on `cd`.
- **Persistent command history**: Command history is now saved to `/home/monolith/.terminal_history` inside the virtual filesystem and automatically restored in new Terminal windows.
- Continued Terminal robustness (UTF-8 rendering fixes for special characters, improved `ls` output with directory indicators, expanded command set).

### Polish Round: Filesystem Toolbar, Terminal Editing, Editor Undo + Line Numbers

- **Filesystem Browser**
  - "New File" toolbar button now fully works: creates a new file with a unique default name and immediately enters inline rename mode (same UX as the context menu "New File").
  - Context menu Delete now has a two-step confirmation flow ("Delete" → "Confirm Delete" / "Cancel") for safety.
  - Various robustness fixes in action handling and coordinate mapping for toolbar + menus.

- **Terminal**
  - Proper in-place cursor for the input line: left/right arrows, Home/End, character insert at cursor position, backspace at cursor.
  - Command history navigation with Up/Down arrows (preserves current unsent input when browsing history).
  - Multiple safety clamps on cursor position to eliminate previous crashes during history recall.
  - Visible cursor block while typing.

- **Text Editor**
  - Added basic undo stack (Ctrl+Z). State is saved before insert, delete, and newline operations (bounded to last ~50 steps).
  - Line numbers shown in a gutter on the left; cursor and text rendering offsets correctly account for the line number width (fixed previous misalignment after adding numbers).
  - Skeleton for find mode (Ctrl+F) added (state tracking; full UI/behavior to come).

### Real Filesystem Browser + Editor File Coordination

- **New real graphical Filesystem browser** (`FilesystemApp`)
  - Scrollable list view with directories shown first (▶ indicator) and files second.
  - Full navigation: double-click to enter folders or open files, "Up" button, keyboard arrows + Enter/Backspace.
  - Basic actions: "New Folder" (auto-generates unique names) and "Delete selected".
  - Double-clicking any file requests the shell to open it in a Text Editor.
  - Added `Filesystem::listEntries()` helper (returns sorted `DirEntry` with type info) for clean UI code.
  - Proper UTF-8 rendering fix (switched to `TTF_RenderUTF8_Blended` for all text in the browser).
  - **Right-click context menu** (major new feature):
    - Context-sensitive actions depending on what you click:
      - Files: Open, Rename, Delete
      - Folders: Open, Rename, Delete
      - Empty space: New Folder, New File, Refresh
  - Added bottom status bar (item count + currently selected item).
  - Full inline rename support (F2 or via context menu) with live text editing.
  - Much more reliable toolbar buttons and context menu (fixed coordinate system for drawing vs input).
  - Backend: Added `Filesystem::rename()` support.

- **Desktop shell launchers and inter-app coordination**
  - `WindowManager` now exposes proper `launchTerminal()`, `launchTextEditor(path)`, and `launchFilesystem()` methods.
  - Extended `IWindowController` with `openInTextEditor(virtualPath)` so apps (especially the filesystem browser) can request the shell to open files.
  - **Singleton editor behavior**: Attempting to open a file that is already open in an editor now brings the existing window to the front instead of creating duplicate independent copies. This prevents divergent in-memory versions of the same file.
  - Proper registration/unregistration on window close. Startup demo editor now goes through the normal launcher path for consistency.

- **Code health**
  - Removed dead placeholder demo apps from `main.cpp`.
  - Reordered resource initialization (`setAppResources`) so launcher methods can be used for initial windows.
  - All editor opens (from filesystem browser, start menu, or direct) now flow through the same coordinated path.

### Desktop Shell: Functional Start Menu + XP-style Taskbar

- **Start Menu is now fully functional**
  - Clicking items launches real apps (Terminal, Text Editor, Filesystem browser).
  - Multiple instances of the same app type can be opened.
  - Menu closes after selection or when clicking elsewhere.

- **Taskbar now shows all open windows** (classic Windows XP behavior)
  - Buttons for every window (minimized or not).
  - Visual distinction: highlighted for focused/active, darker for minimized.
  - Clicking a taskbar button brings the window forward or minimizes it if it was already active (toggle behavior).

- **Horizontal scrolling for taskbar buttons**
  - When more windows are open than fit, left/right scroll arrows appear.
  - Click arrows or use mouse wheel while hovering over the taskbar button area to scroll.
  - Scroll offset is preserved while windows are open.

### Text Editor App
- Added first real native text editor (`TextEditorApp`).
  - Basic multi-line editing with cursor (arrows, Home/End, Backspace, Delete, Enter).
  - Loads and saves files via the Monolith host-backed filesystem (Ctrl+S to save).
  - Opens `/home/monolith/welcome.txt` by default in the initial window.
  - Minimal status line showing filename + dirty indicator.
  - Uses the existing App hosting model and WindowManager integration.

### Desktop Shell Improvements

- **Taskbar is now always visible** (like a real desktop environment)
  - Left: "Start" button (placeholder menu with basic items)
  - Center: Buttons for currently minimized windows
  - Right: Live system clock (12-hour AM/PM format)

- **Click to restore minimized windows now works reliably**
  - Clicking a minimized window's entry in the taskbar now correctly restores it and brings it to the front.
  - Fixed by reordering mouse-down handling so global UI elements (taskbar + Start Menu) are checked *before* any internal window hit testing.

- **Start Menu**
  - Clicking the Start button toggles a popup menu.
  - Clicking outside the menu or on the taskbar closes it.
  - Contains items for launching the main applications.

- **Improved taskbar click handling**
  - Uses exact screen-space rectangles recorded during rendering for accurate hit testing.
  - Start Menu and taskbar interactions no longer conflict with internal window dragging/resizing.

### Notes
- Internal window system (8-direction resize, drag, z-order, title bar buttons, etc.) remains fully functional.
- All outer application window resizing logic continues to be disabled (fixed-size window mode).

## App Hosting Model (Architectural Foundation)

- Introduced a proper `App` abstraction (`src/app/App.hpp`).
  - `App` instances are now responsible for rendering and handling input inside a window's **client/content area** (everything below the title bar).
  - `WindowManager` handles frames, decorations, z-order, dragging, resizing, taskbar, and routes client-area events to the correct app.
- New `IWindowController` interface lets apps safely request operations on their host window (`close()`, `setTitle()`).
- `createWindow(...)` now accepts an optional `unique_ptr<App>`.
- Added focus gained/lost and resize notifications for apps.
- Mouse events forwarded to apps have coordinates translated so (0,0) is the top-left of the app's content area.
- Two early demo apps were used during development to validate the App hosting model.
- Zero regression for existing/placeholder windows (they continue to work with `nullptr` apps).
