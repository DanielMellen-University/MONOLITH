#pragma once

#include "Window.hpp"
#include <SDL2/SDL.h>
#include <cstdint>
#include <SDL2/SDL_ttf.h>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

// App interface for client-area delegation
#include "../app/App.hpp"
#include "../settings/DesktopSettings.hpp"

namespace monolith::fs {
class Filesystem;
}

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

    // Create a new window (optionally with an app that renders its client area).
    // If app is nullptr, the window gets a solid background (existing placeholder behavior).
    //
    // The optional appBase + instanceNumber are for dynamic per-type titling.
    // Launchers should pass them (obtained from claimNextAppInstanceTitle) so that
    // the WM can track and release the slot on close. Direct/fallback creates can omit them.
    Window* createWindow(const std::string& title, int x, int y, int w, int h,
                         std::unique_ptr<monolith::app::App> app = nullptr,
                         const std::string& appBase = "",
                         int instanceNumber = 0);

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

    // Set the logical internal desktop size (runtime default: 1280x720 from main.cpp)
    void setLogicalDesktopSize(int width, int height);

    // Optional Y offset before logical coordinate mapping (0 in current main.cpp)
    void setHeaderOffset(int offsetY);

    // Scale factor to map logical desktop pixels into the SDL window (1.0 = 1:1)
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

    // Internal: create a controller that lets an app operate on a specific window
    monolith::app::IWindowController* createControllerFor(Window* window);

    // Client area helpers (logical coordinates)
    bool isInContentArea(const Window& window, int logicalX, int logicalY) const;

    // Translate mouse coordinates in a mouse event so (0,0) is top-left of the app's content area
    void translateMouseEventToClient(const Window& window, SDL_Event& event) const;

    // === Desktop shell resources & launchers (for Start Menu etc.) ===
    void setAppResources(TTF_Font* font, monolith::fs::Filesystem* fs);

    void launchTerminal();
    void launchTextEditor(const std::string& initialPath = "");
    void launchFilesystem();
    void launchDrawing(const std::string& initialPath = "");
    void launchSettings();
    void launchSnake();
    void launchMinesweeper();

    // Request that the desktop shell / main loop exit (used by Shut Down)
    void requestQuit();
    bool shouldQuit() const;

    // Desktop appearance settings (persisted on the host, applied immediately).
    void loadDesktopSettings(const std::string& hostPath);
    monolith::settings::RGB getDesktopBackground() const;
    void setDesktopBackground(uint8_t r, uint8_t g, uint8_t b);

    // Associate an editor window with a file path so the WM can avoid creating duplicates.
    // The path should be a normalized virtual path.
    void associateEditorWithFile(Window* window, const std::string& virtualPath);

    // Associate a Drawing window with a .modr path (singleton-per-file, like editors).
    void associateDrawingWithFile(Window* window, const std::string& virtualPath);
    void clearDrawingFileBinding(Window* window);

    bool focusEditorForFile(const std::string& virtualPath);
    bool focusDrawingForFile(const std::string& virtualPath);

    // Coordinate conversion helpers (screen <-> logical desktop space)
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

    // Logical internal desktop size (overridden at startup; matches main.cpp)
    int m_logicalWidth = 1280;
    int m_logicalHeight = 720;

    // Y offset applied before logical<->screen mapping (0 unless host chrome compensation is needed)
    int m_headerOffset = 0;

    // Scale to map logical content into the actual window size
    float m_contentScale = 1.0f;

    // Taskbar button hit testing (populated during render each frame)
    struct TaskbarEntry {
        SDL_Rect rect;
        Window* window;
    };
    std::vector<TaskbarEntry> m_taskbarEntries;

    // Start menu state
    bool m_showStartMenu = false;
    int m_startMenuHoverIndex = -1;
    SDL_Rect m_startMenuRect{0, 0, 0, 0};  // screen coords, updated during render when visible

    // Quit request (from Shut Down etc.)
    bool m_quitRequested = false;

    // Tracks which editor windows are responsible for which files (for singleton-per-file behavior)
    std::unordered_map<std::string, Window*> m_fileEditors;

    std::unordered_map<std::string, Window*> m_fileDrawings;

    // Tracks active instance numbers per app base type ("Terminal", "Filesystem", etc.)
    // for the dynamic titling system. Populated by claimNextAppInstanceTitle and
    // released in closeWindow (using the annotation stored on each Window).
    std::unordered_map<std::string, std::set<int>> m_activeAppInstances;

    // Resources for creating real apps from the shell (Start Menu launchers)
    TTF_Font* m_appFont = nullptr;
    monolith::fs::Filesystem* m_fs = nullptr;

    monolith::settings::DesktopSettings m_desktopSettings;
    std::string m_desktopSettingsHostPath;

    // Taskbar XP-style horizontal scrolling
    int m_taskbarScrollOffset = 0;        // logical pixels, can be negative
    int m_taskbarButtonAreaLeft = 0;      // logical left edge of button strip
    int m_taskbarButtonAreaWidth = 0;     // logical width for buttons
    bool m_taskbarNeedsScroll = false;

    struct StartMenuItem {
        SDL_Rect rect;  // in screen coordinates
        // 0=Terminal, 1=Text Editor, 2=Filesystem, 3=Settings, 4=Drawing,
        // 5=Snake, 6=Minesweeper, 7=Shut Down
        // Category headers / separators are not added to this list.
        int action;
    };
    std::vector<StartMenuItem> m_startMenuItems;

    struct TitleButtonRects {
        SDL_Rect close;
        SDL_Rect maximize;
        SDL_Rect minimize;
    };

    // Bring a window to the front of the z-order
    void bringToFront(Window* window);

    // Helper to check if a point is inside a window's title bar
    bool isInTitleBar(const Window& window, int x, int y) const;

    SDL_Rect getTaskbarRect() const;
    SDL_Rect getUsableDesktopRect() const;
    int getUsableDesktopBottom() const;
    SDL_Rect logicalRectToScreen(const SDL_Rect& rect) const;
    TitleButtonRects getTitleButtonRects(const Window& window) const;

    // Claims the lowest available positive instance number for the given app base
    // ("Terminal", "Filesystem", "Settings", "Editor" for bare editors, etc.).
    // Reserves it in m_activeAppInstances, computes the display title
    // (bare name for 1, "Base N" for N>1), and returns {title, instanceNumber}.
    // The caller (launchers) must pass the returned values + base to createWindow
    // so the Window is annotated and the slot can be released on close.
    std::pair<std::string, int> claimNextAppInstanceTitle(const std::string& base);

    // After a tracked window of `base` is closed, re-assign contiguous numbers
    // 1..N to all *remaining* live windows of that base (sorted by their previous
    // instance number to preserve relative ordering). Updates their titles and
    // instance numbers, invalidates title caches, and rebuilds the active set.
    // This makes numbers "adjust dynamically" so there are never gaps while
    // windows are open (e.g. closing "Settings" turns the old "Settings 2" into "Settings").
    void compactAppInstances(const std::string& base);
};

} // namespace monolith::window
