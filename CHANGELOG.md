# Changelog

## Latest Changes

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
