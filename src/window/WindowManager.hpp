#pragma once

#include "Window.hpp"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace monolith::window {

enum class ResizeDirection {
    None,
    Left, Right, Top, Bottom,
    TopLeft, TopRight, BottomLeft, BottomRight
};

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

    // Set the logical internal desktop size (e.g. 1920x1080)
    void setLogicalDesktopSize(int width, int height);

    // Set how many pixels from the top of the SDL window are "eaten" by GNOME's title bar
    void setHeaderOffset(int offsetY);

    // Set the scale factor to draw the logical 1920x1080 content into the actual window
    void setContentScale(float scale);

    // Returns the resize direction at the given screen point (for cursor feedback)
    ResizeDirection getResizeDirectionAt(int mouseX, int mouseY) const;

    // Close a specific window
    void closeWindow(Window* window);

    // Handle button clicks in title bar
    bool handleTitleBarButtons(Window* window, int mouseX, int mouseY);

    // Get which resize direction the mouse is over (if any)
    ResizeDirection getResizeDirection(const Window& window, int mouseX, int mouseY) const;

    // Apply resize based on current direction
    void applyResize(Window* window, int mouseX, int mouseY);

    // Ensure all windows have their title bars (and thus buttons) at least partially visible
    void clampWindowsToDesktop();
    void clampSingleWindow(Window& w);

    // Coordinate conversion helpers (screen <-> logical internal 1920x1080 space)
    int screenToLogicalX(int screenX) const { return static_cast<int>(screenX / m_contentScale); }
    int screenToLogicalY(int screenY) const { return static_cast<int>((screenY - m_headerOffset) / m_contentScale); }

    int logicalToScreenX(int logicalX) const { return static_cast<int>(logicalX * m_contentScale); }
    int logicalToScreenY(int logicalY) const { return static_cast<int>(logicalY * m_contentScale + m_headerOffset); }

private:
    std::vector<std::unique_ptr<Window>> m_windows;
    int m_nextId = 0;

    // Currently focused window (top of z-order)
    Window* m_focusedWindow = nullptr;

    // Window being dragged (if any)
    Window* m_draggedWindow = nullptr;

    // Window being resized + direction
    Window* m_resizingWindow = nullptr;
    ResizeDirection m_resizeDirection = ResizeDirection::None;

    // Mouse state
    int m_mouseX = 0;
    int m_mouseY = 0;
    bool m_mouseDown = false;

    // Font used for window titles (not owned by WindowManager)
    TTF_Font* m_font = nullptr;

    // Simple cache for title textures: key = window id, value = {texture, last title, was focused}
    struct TitleCacheEntry {
        SDL_Texture* texture = nullptr;
        std::string lastTitle;
        bool wasFocused = false;
    };
    std::unordered_map<int, TitleCacheEntry> m_titleCache;

    // Logical internal desktop size (e.g. 1920x1080)
    int m_logicalWidth = 1920;
    int m_logicalHeight = 1080;

    // Pixels from top of SDL window that are occupied by GNOME's client-side title bar
    int m_headerOffset = 36;

    // Scale to map logical content into the actual window size
    float m_contentScale = 1.0f;

    // Taskbar button hit testing (populated during render each frame)
    struct TaskbarEntry {
        SDL_Rect rect;
        Window* window;
    };
    std::vector<TaskbarEntry> m_taskbarEntries;

    // Start menu state (placeholder)
    bool m_showStartMenu = false;

    // Bring a window to the front of the z-order
    void bringToFront(Window* window);

    // Helper to check if a point is inside a window's title bar
    bool isInTitleBar(const Window& window, int x, int y) const;
};

} // namespace monolith::window
