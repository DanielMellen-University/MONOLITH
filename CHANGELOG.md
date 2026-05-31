# Changelog

## Latest Changes

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

- **Start Menu placeholder**
  - Clicking the Start button toggles a simple popup menu.
  - Clicking outside the menu or on the taskbar closes it.
  - Contains placeholder items for future functionality.

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
- Two demo apps added in main.cpp:
  - `PlaceholderApp` — proves delegation + rendering.
  - `ClickDemoApp` — interactive (click to place dots) proving input routing works.
- Zero regression for existing/placeholder windows (they continue to work with `nullptr` apps).
