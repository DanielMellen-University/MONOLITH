# Changelog

## Latest Changes

### Real Filesystem Browser + Editor File Coordination

- **New real graphical Filesystem browser** (`FilesystemApp`)
  - Scrollable list view with directories shown first (▶ indicator) and files second.
  - Full navigation: double-click to enter folders or open files, "Up" button, keyboard arrows + Enter/Backspace.
  - Basic actions: "New Folder" (auto-generates unique names) and "Delete selected".
  - Double-clicking any file requests the shell to open it in a Text Editor.
  - Added `Filesystem::listEntries()` helper (returns sorted `DirEntry` with type info) for clean UI code.
  - Proper UTF-8 rendering fix (switched to `TTF_RenderUTF8_Blended` for all text in the browser).

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
