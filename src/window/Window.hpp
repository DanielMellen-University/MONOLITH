#pragma once

#include <SDL2/SDL.h>
#include <memory>
#include <string>

// Forward declaration so Window can hold an App without pulling in the full definition.
namespace monolith::app {
class App;
}

namespace monolith::window {

/**
 * Represents a single window managed by the WindowManager.
 *
 * Contains geometry, window manager state (minimize/maximize, drag, etc.),
 * and optionally an App that owns the client-area content.
 *
 * Apps are responsible for rendering and handling input inside the content rect
 * (everything below the title bar). The WindowManager owns the frame and decorations.
 */
struct Window {
    int id = -1;
    std::string title;

    // If this window is a text editor associated with a specific virtual file path,
    // this holds the normalized path. Used by WindowManager for "singleton editor" behavior.
    std::string editedFilePath;

    // For dynamic per-type instance titling (see WindowManager::claimNextAppInstanceTitle
    // and closeWindow). If appBaseTitle is non-empty, this window holds a reserved
    // instance number for that base. 1 = bare name ("Terminal"), 2+ = "Terminal 2" etc.
    // 0 / empty = untracked (e.g. direct createWindow fallback or file-backed "Editor - foo").
    std::string appBaseTitle;
    int appInstanceNumber = 0;

    // Position and size of the entire window (including title bar)
    SDL_Rect rect{0, 0, 400, 300};

    bool minimized = false;
    bool maximized = false;

    // Stored rect for restoring after maximize
    SDL_Rect previousRect{0, 0, 0, 0};

    // For dragging
    bool beingDragged = false;
    int dragOffsetX = 0;
    int dragOffsetY = 0;

    // Title bar height (fixed for now)
    static constexpr int TITLE_BAR_HEIGHT = 32;

    // Minimum window size
    static constexpr int MIN_WIDTH = 200;
    static constexpr int MIN_HEIGHT = 120;

    // The application that renders and handles input for this window's client area.
    // Owned by the window (and transitively by WindowManager).
    // May be nullptr for placeholder / unimplemented windows.
    std::unique_ptr<monolith::app::App> app;
};

} // namespace monolith::window
