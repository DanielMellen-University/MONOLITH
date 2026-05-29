#pragma once

#include <SDL2/SDL.h>
#include <string>

namespace monolith::window {

/**
 * Represents a single window managed by the WindowManager.
 * This is intentionally simple for the early stages.
 */
struct Window {
    int id = -1;
    std::string title;

    // Position and size of the entire window (including title bar)
    SDL_Rect rect{0, 0, 400, 300};

    bool minimized = false;

    // For dragging
    bool beingDragged = false;
    int dragOffsetX = 0;
    int dragOffsetY = 0;

    // Title bar height (fixed for now)
    static constexpr int TITLE_BAR_HEIGHT = 32;
};

} // namespace monolith::window
