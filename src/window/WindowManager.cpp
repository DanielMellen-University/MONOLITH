#include "WindowManager.hpp"
#include <algorithm>

namespace monolith::window {

WindowManager::WindowManager() = default;

WindowManager::~WindowManager() = default;

Window* WindowManager::createWindow(const std::string& title, int x, int y, int w, int h) {
    auto window = std::make_unique<Window>();
    window->id = m_nextId++;
    window->title = title;
    window->rect.x = x;
    window->rect.y = y;
    window->rect.w = w;
    window->rect.h = h;

    Window* ptr = window.get();
    m_windows.push_back(std::move(window));

    bringToFront(ptr);
    return ptr;
}

void WindowManager::handleEvent(const SDL_Event& event) {
    if (event.type == SDL_MOUSEMOTION) {
        m_mouseX = event.motion.x;
        m_mouseY = event.motion.y;
    }

    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        m_mouseDown = true;
        m_mouseX = event.button.x;
        m_mouseY = event.button.y;

        Window* clicked = getWindowAt(m_mouseX, m_mouseY);
        if (!clicked) return;

        bringToFront(clicked);

        // === Priority 1: Title bar buttons ===
        if (isInTitleBar(*clicked, m_mouseX, m_mouseY)) {
            if (handleTitleBarButtons(clicked, m_mouseX, m_mouseY)) {
                return; // Button was clicked, don't start drag/resize
            }

            // Not on a button → start dragging the window
            m_draggedWindow = clicked;
            clicked->beingDragged = true;
            clicked->dragOffsetX = m_mouseX - clicked->rect.x;
            clicked->dragOffsetY = m_mouseY - clicked->rect.y;
            return;
        }

        // === Priority 2: Resize borders ===
        ResizeDirection dir = getResizeDirection(*clicked, m_mouseX, m_mouseY);
        if (dir != ResizeDirection::None && !clicked->maximized) {
            m_resizingWindow = clicked;
            m_resizeDirection = dir;
            return;
        }
    }

    if (event.type == SDL_MOUSEBUTTONUP && event.button.button == SDL_BUTTON_LEFT) {
        m_mouseDown = false;

        if (m_draggedWindow) {
            m_draggedWindow->beingDragged = false;
            m_draggedWindow = nullptr;
        }

        if (m_resizingWindow) {
            m_resizingWindow = nullptr;
            m_resizeDirection = ResizeDirection::None;
        }
    }
}

void WindowManager::update() {
    if (m_draggedWindow && m_mouseDown) {
        m_draggedWindow->rect.x = m_mouseX - m_draggedWindow->dragOffsetX;
        m_draggedWindow->rect.y = m_mouseY - m_draggedWindow->dragOffsetY;
    }

    if (m_resizingWindow && m_mouseDown) {
        applyResize(m_resizingWindow, m_mouseX, m_mouseY);
    }
}

void WindowManager::render(SDL_Renderer* renderer) {
    // Draw from back to front (so the focused window is drawn last)
    for (auto it = m_windows.rbegin(); it != m_windows.rend(); ++it) {
        Window& win = **it;

        if (win.minimized) continue;

        // === Window background (content area) ===
        SDL_SetRenderDrawColor(renderer, 45, 45, 50, 255);
        SDL_Rect contentRect = win.rect;
        contentRect.y += Window::TITLE_BAR_HEIGHT;
        contentRect.h -= Window::TITLE_BAR_HEIGHT;
        SDL_RenderFillRect(renderer, &contentRect);

        // === Title bar ===
        bool isFocused = (&win == m_focusedWindow);
        SDL_Color titleColor = isFocused ? SDL_Color{60, 60, 70, 255}
                                         : SDL_Color{40, 40, 48, 255};

        SDL_SetRenderDrawColor(renderer, titleColor.r, titleColor.g, titleColor.b, 255);
        SDL_Rect titleBar = {win.rect.x, win.rect.y, win.rect.w, Window::TITLE_BAR_HEIGHT};
        SDL_RenderFillRect(renderer, &titleBar);

        // === Window title text ===
        if (m_font && !win.title.empty()) {
            SDL_Color textColor = {220, 220, 225, 255};
            SDL_Surface* textSurface = TTF_RenderText_Blended(m_font, win.title.c_str(), textColor);
            if (textSurface) {
                SDL_Texture* textTexture = SDL_CreateTextureFromSurface(renderer, textSurface);

                int textWidth = textSurface->w;
                int textHeight = textSurface->h;

                // Position text on the left side of the title bar with padding
                int paddingLeft = 10;
                int availableWidth = win.rect.w - 120; // leave room for buttons

                int textX = win.rect.x + paddingLeft;
                int textY = win.rect.y + (Window::TITLE_BAR_HEIGHT - textHeight) / 2;

                // Simple truncation if title is too long
                SDL_Rect dstRect = {textX, textY, textWidth, textHeight};
                if (textWidth > availableWidth) {
                    dstRect.w = availableWidth;
                }

                SDL_RenderCopy(renderer, textTexture, nullptr, &dstRect);

                SDL_DestroyTexture(textTexture);
                SDL_FreeSurface(textSurface);
            }
        }

        // === Fake window buttons (close, minimize, maximize) ===
        int buttonSize = 16;
        int buttonY = win.rect.y + (Window::TITLE_BAR_HEIGHT - buttonSize) / 2;
        int buttonSpacing = 6;
        int rightEdge = win.rect.x + win.rect.w - 10;

        // Close button (red)
        SDL_SetRenderDrawColor(renderer, 200, 60, 60, 255);
        SDL_Rect closeBtn = {rightEdge - buttonSize, buttonY, buttonSize, buttonSize};
        SDL_RenderFillRect(renderer, &closeBtn);

        // Maximize (green)
        SDL_SetRenderDrawColor(renderer, 60, 170, 80, 255);
        SDL_Rect maxBtn = {rightEdge - buttonSize * 2 - buttonSpacing, buttonY, buttonSize, buttonSize};
        SDL_RenderFillRect(renderer, &maxBtn);

        // Minimize (yellow/orange)
        SDL_SetRenderDrawColor(renderer, 200, 170, 60, 255);
        SDL_Rect minBtn = {rightEdge - buttonSize * 3 - buttonSpacing * 2, buttonY, buttonSize, buttonSize};
        SDL_RenderFillRect(renderer, &minBtn);

        // === Window border ===
        SDL_SetRenderDrawColor(renderer, 30, 30, 35, 255);
        SDL_RenderDrawRect(renderer, &win.rect);
    }
}

Window* WindowManager::getWindowAt(int mouseX, int mouseY) {
    // Check from front to back (focused window first)
    for (auto it = m_windows.rbegin(); it != m_windows.rend(); ++it) {
        Window& win = **it;
        if (!win.minimized && SDL_PointInRect(
                new SDL_Point{mouseX, mouseY},
                &win.rect)) {
            return &win;
        }
    }
    return nullptr;
}

void WindowManager::bringToFront(Window* window) {
    if (!window) return;

    // Move the window to the end of the vector (drawn last = on top)
    auto it = std::find_if(m_windows.begin(), m_windows.end(),
        [window](const auto& w) { return w.get() == window; });

    if (it != m_windows.end()) {
        auto windowPtr = std::move(*it);
        m_windows.erase(it);
        m_windows.push_back(std::move(windowPtr));
        m_focusedWindow = m_windows.back().get();
    }
}

bool WindowManager::isInTitleBar(const Window& window, int x, int y) const {
    SDL_Rect titleBar = {
        window.rect.x,
        window.rect.y,
        window.rect.w,
        Window::TITLE_BAR_HEIGHT
    };
    return SDL_PointInRect(new SDL_Point{x, y}, &titleBar);
}

void WindowManager::setFont(TTF_Font* font) {
    m_font = font;
}

bool WindowManager::handleTitleBarButtons(Window* window, int mouseX, int mouseY) {
    if (!window) return false;

    const int buttonSize = 16;
    const int buttonSpacing = 6;
    const int rightEdge = window->rect.x + window->rect.w - 10;
    const int buttonY = window->rect.y + (Window::TITLE_BAR_HEIGHT - buttonSize) / 2;

    SDL_Rect closeBtn   = {rightEdge - buttonSize,                           buttonY, buttonSize, buttonSize};
    SDL_Rect maxBtn     = {rightEdge - buttonSize * 2 - buttonSpacing,       buttonY, buttonSize, buttonSize};
    SDL_Rect minBtn     = {rightEdge - buttonSize * 3 - buttonSpacing * 2,   buttonY, buttonSize, buttonSize};

    SDL_Point mouse = {mouseX, mouseY};

    if (SDL_PointInRect(&mouse, &closeBtn)) {
        closeWindow(window);
        return true;
    }

    if (SDL_PointInRect(&mouse, &maxBtn)) {
        if (window->maximized) {
            // Restore
            window->rect = window->previousRect;
            window->maximized = false;
        } else {
            // Maximize
            window->previousRect = window->rect;
            // For now, maximize to a large area inside the app window (we'll improve bounds later)
            window->rect.x = 20;
            window->rect.y = 20;
            window->rect.w = 1200;
            window->rect.h = 680;
            window->maximized = true;
        }
        return true;
    }

    if (SDL_PointInRect(&mouse, &minBtn)) {
        window->minimized = true;
        return true;
    }

    return false;
}

ResizeDirection WindowManager::getResizeDirection(const Window& window, int mouseX, int mouseY) const {
    const int border = 6; // Resize border thickness

    const int left   = window.rect.x;
    const int right  = window.rect.x + window.rect.w;
    const int top    = window.rect.y;
    const int bottom = window.rect.y + window.rect.h;

    bool onLeft   = mouseX >= left && mouseX <= left + border;
    bool onRight  = mouseX >= right - border && mouseX <= right;
    bool onTop    = mouseY >= top && mouseY <= top + border;
    bool onBottom = mouseY >= bottom - border && mouseY <= bottom;

    if (onLeft && onTop)     return ResizeDirection::TopLeft;
    if (onRight && onTop)    return ResizeDirection::TopRight;
    if (onLeft && onBottom)  return ResizeDirection::BottomLeft;
    if (onRight && onBottom) return ResizeDirection::BottomRight;

    if (onLeft)   return ResizeDirection::Left;
    if (onRight)  return ResizeDirection::Right;
    if (onTop)    return ResizeDirection::Top;
    if (onBottom) return ResizeDirection::Bottom;

    return ResizeDirection::None;
}

void WindowManager::applyResize(Window* window, int mouseX, int mouseY) {
    if (!window || m_resizeDirection == ResizeDirection::None) return;

    int minW = Window::MIN_WIDTH;
    int minH = Window::MIN_HEIGHT + Window::TITLE_BAR_HEIGHT;

    SDL_Rect& r = window->rect;

    switch (m_resizeDirection) {
        case ResizeDirection::Left:
            r.w = r.x + r.w - mouseX;
            r.x = mouseX;
            break;
        case ResizeDirection::Right:
            r.w = mouseX - r.x;
            break;
        case ResizeDirection::Top:
            r.h = r.y + r.h - mouseY;
            r.y = mouseY;
            break;
        case ResizeDirection::Bottom:
            r.h = mouseY - r.y;
            break;
        case ResizeDirection::TopLeft:
            r.w = r.x + r.w - mouseX;
            r.h = r.y + r.h - mouseY;
            r.x = mouseX;
            r.y = mouseY;
            break;
        case ResizeDirection::TopRight:
            r.w = mouseX - r.x;
            r.h = r.y + r.h - mouseY;
            r.y = mouseY;
            break;
        case ResizeDirection::BottomLeft:
            r.w = r.x + r.w - mouseX;
            r.h = mouseY - r.y;
            r.x = mouseX;
            break;
        case ResizeDirection::BottomRight:
            r.w = mouseX - r.x;
            r.h = mouseY - r.y;
            break;
        default:
            break;
    }

    // Enforce minimum size
    if (r.w < minW) r.w = minW;
    if (r.h < minH) r.h = minH;
}

void WindowManager::closeWindow(Window* window) {
    if (!window) return;

    auto it = std::find_if(m_windows.begin(), m_windows.end(),
        [window](const auto& w) { return w.get() == window; });

    if (it != m_windows.end()) {
        if (m_focusedWindow == window) m_focusedWindow = nullptr;
        if (m_draggedWindow == window) m_draggedWindow = nullptr;
        if (m_resizingWindow == window) {
            m_resizingWindow = nullptr;
            m_resizeDirection = ResizeDirection::None;
        }
        m_windows.erase(it);
    }
}

} // namespace monolith::window
