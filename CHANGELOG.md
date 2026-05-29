# Changelog

## Latest Changes

### Fixed
- **Fonts now render reliably**
  - Font loading now tries multiple common system locations automatically (`assets/fonts/`, `/usr/share/fonts/truetype/dejavu/`, etc.).
  - Removed the single hard-coded path that frequently failed.

- **No more unwanted gap under GNOME title bar**
  - Removed the artificial 36px GNOME header compensation that was making the outer window taller and shifting all content downward.
  - Set header offset to 0 so the internal 1280×720 desktop now starts directly at the top of the SDL client area (right under the GNOME title bar).

- **Outer application window is strictly fixed-size (no resizing logic)**
  - All dynamic resize handling, minimum-size enforcement, and resize event logic for the main Monolith window have been removed.
  - Window is created as a clean fixed-size window at 1280×756 (1280×720 content + 36px GNOME header compensation).

### Changed
- Internal logical desktop size is now 1280×720 (matches the comfortable window size, no scaling applied).
- All internal windows, dragging, resizing (8 directions), buttons, and taskbar logic continue to work at full size with no unwanted downscaling.

### Notes
- Higher internal resolutions (1440p, 4K) can be re-introduced later via the Settings system when ready.
- Recommended build/run command now includes more robust font discovery.
