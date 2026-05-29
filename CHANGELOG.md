# Changelog

All notable changes to Monolith will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Changed
- **Outer application window is now strictly fixed-size with no resizing logic.**
  - Completely removed all dynamic resizing, minimum size enforcement, and resize event handling for the main Monolith SDL window.
  - The window is created with only `SDL_WINDOW_SHOWN` and is intentionally non-resizable in the current mode.
  - This was done after multiple iterations because previous approaches (dynamic adaptation, min-size snapping, etc.) produced buggy behavior on Linux (especially GNOME).

- **Fixed comfortable default window size with GNOME title bar compensation.**
  - Outer window is now created at 1280×756.
  - Internal logical desktop is 1280×720.
  - 36px vertical offset added at the top to account for GNOME's client-side title bar (Adwaita), so internal content and window buttons sit cleanly below the OS header.

- **Internal window system remains fully functional.**
  - All 8-direction resizing, dragging, z-order, title bar buttons (close/minimize/maximize), and clamping logic for internal windows is preserved and working.
  - Cursor feedback for internal window edges/corners is active.

### Removed
- All code related to outer window resizing and dynamic desktop size adaptation (for now).
- Minimum size constraints and "snap back" behavior for the main application window.
- Content scaling between logical and physical window size (logical size now matches the window content area 1:1).

### Notes
- Higher internal resolutions (1440p, 4K, etc.) and proper resolution switching will be re-introduced later via the planned Settings app.
- The outer window may become resizable again in the future once the desired behavior is better defined.

## Previous Work (prior to this cleanup)
- Full internal window manager foundation (z-order, 8-direction resizing, functional title bar buttons, live clamping, cursor feedback).
- Dynamic outer window support and clamping experiments (later removed per direction above).

[Unreleased]: https://github.com/DanielMellen-University/MONOLITH/compare/8841239...HEAD
