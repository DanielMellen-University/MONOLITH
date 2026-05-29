#pragma once

#include "Window.hpp"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <memory>
#include <vector>

namespace monolith::window {

/**
 * The WindowManager is responsible for:
 * - Owning all windows
 * - Managing z-order (drawing order + focus)
 * - Handling window-level input (dragging, clicking title bar, etc.)
 * - Rendering window frames and title bars
 *
 * Individual applications will later render *into* the client area of a window.
 */
class WindowManager {
public:
    WindowManager();
    ~WindowManager();

    // Create a new window and return a pointer to it (for now)
    Window* createWindow(const std::string& title, int x, int y, int w, int h);

    // Basic input handling (called every frame with SDL events)
    void handleEvent(const SDL_Event& event);

    // Update logic (e.g. dragging)
    void update();

    // Render all windows (title bars + content areas)
    void render(SDL_Renderer* renderer);

    // Get the window currently under the mouse (if any)
    Window* getWindowAt(int mouseX, int mouseY);

    // Set the font used for rendering window titles
    void setFont(TTF_Font* font);

private:
    std::vector<std::unique_ptr<Window>> m_windows;
    int m_nextId = 0;

    // Currently focused window (top of z-order)
    Window* m_focusedWindow = nullptr;

    // Window being dragged (if any)
    Window* m_draggedWindow = nullptr;

    // Mouse state for dragging
    int m_mouseX = 0;
    int m_mouseY = 0;
    bool m_mouseDown = false;

    // Font used for window titles (not owned by WindowManager)
    TTF_Font* m_font = nullptr;

    // Bring a window to the front of the z-order
    void bringToFront(Window* window);

    // Helper to check if a point is inside a window's title bar
    bool isInTitleBar(const Window& window, int x, int y) const;
};

} // namespace monolith::window
