#pragma once

#include "Window.hpp"
#include "DesktopIcons.hpp"
#include <SDL2/SDL.h>
#include <cstdint>
#include <SDL2/SDL_ttf.h>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
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
 * - Drawing optional desktop icons behind windows
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

    // Get the window currently under a screen-space mouse point (if any).
    Window* getWindowAt(int mouseX, int mouseY);

    // Set the font used for rendering window titles
    void setFont(TTF_Font* font);

    // Set the logical internal desktop size (runtime default: 1280x720 from main.cpp)
    void setLogicalDesktopSize(int width, int height);
    void getLogicalDesktopSize(int& width, int& height) const;

    // Optional Y offset before logical coordinate mapping (0 in current main.cpp)
    void setHeaderOffset(int offsetY);

    // Scale factor to map logical desktop pixels into the SDL window (1.0 = 1:1)
    void setContentScale(float scale);

    // Returns the resize direction at the given screen-space point (for cursor feedback).
    ResizeDirection getResizeDirectionAt(int mouseX, int mouseY) const;

    // Close a specific window
    void closeWindow(Window* window);

    // Update a window title through the shell bridge and invalidate cached taskbar geometry.
    void setWindowTitle(Window* window, const std::string& title);

    // Handle button clicks in title bar
    bool handleTitleBarButtons(Window* window, int mouseX, int mouseY);

    // Get which resize direction the mouse is over (if any)
    ResizeDirection getResizeDirection(const Window& window, int mouseX, int mouseY) const;

    // Apply resize based on current direction
    void applyResize(Window* window, int mouseX, int mouseY);

    // Ensure all windows have their title bars (and thus buttons) at least partially visible
    void clampWindowsToDesktop();
    void clampSingleWindow(Window& w);
    void notifyAppResizeIfGeometryChanged(Window& w, const SDL_Rect& before);

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
    void launchPong();
    void launchBreakout();

    // Open a virtual path with the default app for its type (.modr → Drawing, else Editor).
    void openPath(const std::string& virtualPath);

    // Request that the desktop shell / main loop exit (used by Shut Down)
    void requestQuit();
    bool shouldQuit() const;

    // Desktop appearance settings (persisted on the host, applied immediately).
    void loadDesktopSettings(const std::string& hostPath);
    monolith::settings::RGB getDesktopBackground() const;
    void setDesktopBackground(uint8_t r, uint8_t g, uint8_t b);
    std::string getWallpaperPath() const;
    void setWallpaperPath(const std::string& virtualPath);
    bool getClock24Hour() const;
    void setClock24Hour(bool enabled);
    int getUiScalePercent() const;
    void setUiScalePercent(int percent);

    // Session: restore open windows from a host-side file; save current layout on exit.
    // Format is line-based (see saveSession). Returns true after a valid
    // session header is accepted, even when no entries can be restored.
    bool loadSession(const std::string& hostPath);
    bool saveSession(const std::string& hostPath) const;

    // Associate an editor window with a file path so the WM can avoid creating duplicates.
    // The path should be a normalized virtual path.
    void associateEditorWithFile(Window* window, const std::string& virtualPath);
    void clearEditorFileBinding(Window* window);

    // Associate a Drawing window with a .modr path (singleton-per-file, like editors).
    void associateDrawingWithFile(Window* window, const std::string& virtualPath);
    void clearDrawingFileBinding(Window* window);
    void notifyVirtualPathMoved(const std::string& oldPath, const std::string& newPath);
    void notifyVirtualPathCreated(const std::string& virtualPath);
    void notifyVirtualPathChanged(const std::string& virtualPath);
    void notifyVirtualPathRemoved(const std::string& virtualPath);

    bool focusEditorForFile(const std::string& virtualPath);
    bool focusDrawingForFile(const std::string& virtualPath);

    // Shared virtual filesystem clipboard used by all Filesystem windows.
    bool getFilesystemClipboard(std::vector<std::string>& paths, bool& isCut) const;
    void setFilesystemClipboard(const std::vector<std::string>& paths, bool isCut);
    void clearFilesystemClipboard();

    // Coordinate conversion helpers (screen <-> logical desktop space)
    int screenToLogicalX(int screenX) const { return static_cast<int>(screenX / m_contentScale); }
    int screenToLogicalY(int screenY) const { return static_cast<int>((screenY - m_headerOffset) / m_contentScale); }

    int logicalToScreenX(int logicalX) const { return static_cast<int>(logicalX * m_contentScale); }
    int logicalToScreenY(int logicalY) const { return static_cast<int>(logicalY * m_contentScale + m_headerOffset); }

#include "WindowManager_private.inc"
