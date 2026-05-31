#pragma once

#include <SDL2/SDL.h>
#include <string>

namespace monolith::window {
class WindowManager;
}

namespace monolith::app {

/**
 * Interface that apps can use to request operations on the window that contains them.
 * This is the primary (and safe) way for an app to affect its host window.
 */
struct IWindowController {
    virtual ~IWindowController() = default;

    virtual void close() = 0;
    virtual void setTitle(const std::string& title) = 0;

    // Request that the desktop shell open the given virtual path in a text editor.
    // Safe to call even if not supported (default is no-op).
    virtual void openInTextEditor(const std::string& /*virtualPath*/) {}

    // Future extensions:
    // virtual void minimize() = 0;
    // virtual void maximize() = 0;
    // virtual void restore() = 0;
};

/**
 * Base class for all native applications that live inside Monolith windows.
 *
 * Responsibilities of an App:
 * - Render its own content area (the region inside the window frame, below the title bar).
 * - Handle input events delivered to its client area.
 * - Manage its own internal state and logic.
 *
 * The WindowManager owns the window frame, decorations, z-order, dragging, resizing,
 * taskbar, and input routing. Apps never draw title bars or handle frame interactions.
 */
class App {
public:
    virtual ~App() = default;

    // === Rendering ===
    // The WindowManager has already drawn the window frame (title bar + buttons)
    // and a solid background for the content area.
    //
    // contentRect is in *screen* coordinates (already scaled and adjusted for any
    // host window manager chrome). The app must restrict all drawing to this rectangle.
    virtual void render(SDL_Renderer* renderer, const SDL_Rect& contentRect) = 0;

    // === Input ===
    // Called only for events that hit this app's client area (after WM hit-testing).
    //
    // For mouse events, the coordinates have been translated so that (0,0) is the
    // top-left corner of the content area in logical pixels (i.e. relative to the
    // app's own space, excluding the title bar).
    virtual void handleEvent(const SDL_Event& /*event*/) {}

    // === Per-frame update ===
    virtual void update() {}

    // === Optional lifecycle notifications ===
    virtual void onFocusGained() {}
    virtual void onFocusLost() {}

    // Called whenever the client area size changes (resize, maximize, etc.).
    // Dimensions are in logical pixels (excluding title bar height).
    virtual void onResize(int /*clientWidth*/, int /*clientHeight*/) {}

    // === Controller access (provided by WindowManager) ===
    IWindowController* getController() const { return m_controller; }

protected:
    // Called by WindowManager after the app is attached to a window.
    void setController(IWindowController* controller) { m_controller = controller; }

private:
    IWindowController* m_controller = nullptr;

    // Allow WindowManager to call the protected setter.
    friend class ::monolith::window::WindowManager;
};

} // namespace monolith::app
