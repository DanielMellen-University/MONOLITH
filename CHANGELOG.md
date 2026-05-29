# Changelog

## Latest Changes (Technical Debt Cleanup)

### Fixed / Removed
- **Major stability improvement: Title texture caching**
  - Window titles (and taskbar text) are no longer recreated from scratch every single frame.
  - Added a simple per-window title texture cache. Textures are only regenerated when the title text or focus state actually changes.
  - This directly eliminates the X11 `BadAlloc` crash that occurred after ~30 seconds of runtime due to excessive texture/surface allocation.

- **Memory leaks eliminated**
  - Removed all instances of `new SDL_Point` in hot paths (`getWindowAt`, `isInTitleBar`, `getResizeDirectionAt`).
  - Replaced with stack-allocated `SDL_Point` to prevent leaks on every mouse event and render frame.

- **Cleaned up compiler warnings**
  - Removed unused `btnHeight` variable.

### Improved
- `setFont()` now properly invalidates the title texture cache when the font changes.
- `closeWindow()` now correctly destroys any cached title texture for the closed window.

### Impact
- Significantly reduced CPU and GPU resource usage during normal operation.
- Long-running sessions are now stable (no more sudden X11 resource exhaustion).
- Better foundation for future performance work.

All outer window resize/min-size logic remains removed (as previously cleaned up). Internal window system (8-direction resize, buttons, taskbar, etc.) is unchanged.
