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

    const int logicalMouseY = screenToLogicalY(m_mouseY);

    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        m_mouseDown = true;
        m_mouseX = event.button.x;
        m_mouseY = event.button.y;

        Window* clicked = getWindowAt(m_mouseX, m_mouseY);
        if (!clicked) return;

        bringToFront(clicked);

        // === Priority 1: Title bar buttons (highest priority) ===
        if (isInTitleBar(*clicked, m_mouseX, logicalMouseY)) {
            if (handleTitleBarButtons(clicked, m_mouseX, logicalMouseY)) {
                return; // Button was clicked
            }
        }

        // === Priority 2: Resize borders (all 8 directions, including top edge/corners) ===
        ResizeDirection dir = getResizeDirection(*clicked, m_mouseX, logicalMouseY);
        if (dir != ResizeDirection::None && !clicked->maximized) {
            m_resizingWindow = clicked;
            m_resizeDirection = dir;
            return;
        }

        // === Priority 3: Title bar drag (only if not on buttons and not on resize zone) ===
        if (isInTitleBar(*clicked, m_mouseX, logicalMouseY)) {
            m_draggedWindow = clicked;
            clicked->beingDragged = true;
            clicked->dragOffsetX = m_mouseX - clicked->rect.x;
            clicked->dragOffsetY = logicalMouseY - clicked->rect.y;
            return;
        }
    }

    // === Taskbar clicks (restore minimized windows) ===
    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        const int taskbarHeight = 28;
        const int taskbarY = m_logicalHeight - taskbarHeight;

        if (logicalMouseY >= taskbarY && logicalMouseY < m_logicalHeight) {
            // Rebuild the list of minimized windows in the same order as render
            std::vector<Window*> minimizedWindows;
            for (auto& w : m_windows) {
                if (w->minimized) minimizedWindows.push_back(w.get());
            }

            int x = 8;
            const int spacing = 6;

            for (Window* mw : minimizedWindows) {
                int btnWidth = std::max(80, static_cast<int>(mw->title.length() * 7));
                if (m_mouseX >= x && m_mouseX < x + btnWidth &&
                    logicalMouseY >= taskbarY && logicalMouseY < taskbarY + taskbarHeight) {
                    // Restore this window
                    mw->minimized = false;
                    bringToFront(mw);
                    return;
                }
                x += btnWidth + spacing;
            }
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
    const int logicalMouseY = screenToLogicalY(m_mouseY);

    if (m_draggedWindow && m_mouseDown) {
        m_draggedWindow->rect.x = m_mouseX - m_draggedWindow->dragOffsetX;
        m_draggedWindow->rect.y = logicalMouseY - m_draggedWindow->dragOffsetY;

        // Prevent user from dragging the window so far that buttons become inaccessible
        clampSingleWindow(*m_draggedWindow);
    }

    if (m_resizingWindow && m_mouseDown) {
        applyResize(m_resizingWindow, m_mouseX, logicalMouseY);
        clampSingleWindow(*m_resizingWindow);
    }

    // Enforce maximized windows fill the full logical area (minus taskbar if present)
    bool hasMinimized = false;
    for (auto& w : m_windows) {
        if (w->minimized) hasMinimized = true;
    }

    const int bottomReserved = hasMinimized ? 28 : 0;  // leave space for taskbar

    for (auto& w : m_windows) {
        if (w->maximized && !w->minimized) {
            w->rect.x = 0;
            w->rect.y = 0;
            w->rect.w = m_logicalWidth;
            w->rect.h = m_logicalHeight - bottomReserved;
        }
    }
}

void WindowManager::render(SDL_Renderer* renderer) {
    // Draw from back to front so the focused window (last in vector) is drawn last and appears on top
    for (auto it = m_windows.begin(); it != m_windows.end(); ++it) {
        Window& win = **it;

        if (win.minimized) continue;

        // Convert logical window rect to screen coordinates (shifted down by GNOME header + scaled)
        const int screenX = logicalToScreenX(win.rect.x);
        const int screenY = logicalToScreenY(win.rect.y);

        const int scaledW = static_cast<int>(win.rect.w * m_contentScale);
        const int scaledH = static_cast<int>(win.rect.h * m_contentScale);
        const int scaledTitleH = static_cast<int>(Window::TITLE_BAR_HEIGHT * m_contentScale);

        // === Window background (content area) ===
        SDL_SetRenderDrawColor(renderer, 45, 45, 50, 255);
        SDL_Rect contentRect = {screenX, screenY + scaledTitleH, scaledW, scaledH - scaledTitleH};
        SDL_RenderFillRect(renderer, &contentRect);

        // === Title bar ===
        bool isFocused = (&win == m_focusedWindow);
        SDL_Color titleColor = isFocused ? SDL_Color{60, 60, 70, 255}
                                         : SDL_Color{40, 40, 48, 255};

        SDL_SetRenderDrawColor(renderer, titleColor.r, titleColor.g, titleColor.b, 255);
        SDL_Rect titleBar = {screenX, screenY, scaledW, scaledTitleH};
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
                int availableWidth = scaledW - 80;

                int textX = screenX + paddingLeft;
                int textY = screenY + (scaledTitleH - textHeight) / 2;

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

        // === Window buttons (close, minimize, maximize) with simple symbols ===
        int buttonSize = static_cast<int>(16 * m_contentScale);
        if (buttonSize < 10) buttonSize = 10; // minimum usable size
        int buttonY = screenY + (scaledTitleH - buttonSize) / 2;
        int buttonSpacing = static_cast<int>(6 * m_contentScale);
        int rightEdge = screenX + scaledW - static_cast<int>(10 * m_contentScale);

        // Close button (red) - draw white X
        SDL_SetRenderDrawColor(renderer, 200, 60, 60, 255);
        SDL_Rect closeBtn = {rightEdge - buttonSize, buttonY, buttonSize, buttonSize};
        SDL_RenderFillRect(renderer, &closeBtn);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderDrawLine(renderer, closeBtn.x + 3, closeBtn.y + 3, closeBtn.x + buttonSize - 4, closeBtn.y + buttonSize - 4);
        SDL_RenderDrawLine(renderer, closeBtn.x + buttonSize - 4, closeBtn.y + 3, closeBtn.x + 3, closeBtn.y + buttonSize - 4);

        // Maximize (green) - draw white square outline
        SDL_SetRenderDrawColor(renderer, 60, 170, 80, 255);
        SDL_Rect maxBtn = {rightEdge - buttonSize * 2 - buttonSpacing, buttonY, buttonSize, buttonSize};
        SDL_RenderFillRect(renderer, &maxBtn);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_Rect maxSymbol = {maxBtn.x + 3, maxBtn.y + 3, buttonSize - 7, buttonSize - 7};
        SDL_RenderDrawRect(renderer, &maxSymbol);

        // Minimize (yellow/orange) - draw white horizontal line
        SDL_SetRenderDrawColor(renderer, 200, 170, 60, 255);
        SDL_Rect minBtn = {rightEdge - buttonSize * 3 - buttonSpacing * 2, buttonY, buttonSize, buttonSize};
        SDL_RenderFillRect(renderer, &minBtn);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        int minLineY = minBtn.y + buttonSize / 2;
        SDL_RenderDrawLine(renderer, minBtn.x + 3, minLineY, minBtn.x + buttonSize - 4, minLineY);

        // === Window border (drawn in screen coordinates) ===
        SDL_Rect screenRect = {screenX, screenY, scaledW, scaledH};
        SDL_SetRenderDrawColor(renderer, 30, 30, 35, 255);
        SDL_RenderDrawRect(renderer, &screenRect);
    }

    // === Very basic taskbar for minimized windows (temporary until proper taskbar exists) ===
    std::vector<Window*> minimizedWindows;
    for (auto& w : m_windows) {
        if (w->minimized) minimizedWindows.push_back(w.get());
    }

    if (!minimizedWindows.empty()) {
        const int taskbarHeight = 28;
        const int taskbarY = m_logicalHeight - taskbarHeight; // bottom of logical area

        // Draw taskbar background
        SDL_SetRenderDrawColor(renderer, 35, 35, 40, 255);
        SDL_Rect taskbarRect = {
            logicalToScreenX(0),
            logicalToScreenY(taskbarY),
            static_cast<int>(m_logicalWidth * m_contentScale),
            static_cast<int>(taskbarHeight * m_contentScale)
        };
        SDL_RenderFillRect(renderer, &taskbarRect);

        // Draw minimized window buttons
        int x = logicalToScreenX(8);
        const int btnHeight = static_cast<int>(22 * m_contentScale);
        const int btnY = logicalToScreenY(taskbarY) + (taskbarRect.h - btnHeight) / 2;

        for (Window* mw : minimizedWindows) {
            int btnWidth = std::max(80, static_cast<int>(mw->title.length() * 7 * m_contentScale));
            SDL_SetRenderDrawColor(renderer, 70, 70, 80, 255);
            SDL_Rect btnRect = {x, btnY, btnWidth, btnHeight};
            SDL_RenderFillRect(renderer, &btnRect);

            // Simple text for now (using the same font if available)
            if (m_font) {
                SDL_Color textCol = {230, 230, 235, 255};
                SDL_Surface* s = TTF_RenderText_Blended(m_font, mw->title.c_str(), textCol);
                if (s) {
                    SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
                    if (t) {
                        SDL_Rect textDst = {x + 6, btnY + (btnHeight - s->h) / 2, s->w, s->h};
                        if (textDst.w > btnWidth - 12) textDst.w = btnWidth - 12;
                        SDL_RenderCopy(renderer, t, nullptr, &textDst);
                        SDL_DestroyTexture(t);
                    }
                    SDL_FreeSurface(s);
                }
            }

            // Store clickable area for later hit testing (simple approach: store in a temp vector)
            // For now we'll handle clicks in handleEvent by checking y range and x ranges.
            x += btnWidth + static_cast<int>(6 * m_contentScale);
        }
    }
}

Window* WindowManager::getWindowAt(int mouseX, int mouseY) {
    const int logicalMouseY = screenToLogicalY(mouseY);

    // Check from front to back (focused window first)
    for (auto it = m_windows.rbegin(); it != m_windows.rend(); ++it) {
        Window& win = **it;
        if (!win.minimized) {
            // Build a logical-space rect for hit testing
            SDL_Rect logicalRect = win.rect;
            // Note: win.rect is already stored in logical coordinates
            if (SDL_PointInRect(new SDL_Point{mouseX, logicalMouseY}, &logicalRect)) {
                return &win;
            }
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

void WindowManager::setLogicalDesktopSize(int width, int height) {
    m_logicalWidth = width;
    m_logicalHeight = height;

    // Re-clamp when logical size changes
    clampWindowsToDesktop();
}

void WindowManager::setHeaderOffset(int offsetY) {
    m_headerOffset = offsetY;
}

void WindowManager::setContentScale(float scale) {
    m_contentScale = scale;
}

ResizeDirection WindowManager::getResizeDirectionAt(int mouseX, int mouseY) const {
    // Check from front to back
    for (auto it = m_windows.rbegin(); it != m_windows.rend(); ++it) {
        Window& win = **it;
        if (win.minimized || win.maximized) continue;

        if (SDL_PointInRect(new SDL_Point{mouseX, mouseY}, &win.rect)) {
            return getResizeDirection(win, mouseX, mouseY);
        }
    }
    return ResizeDirection::None;
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
            // Restore previous size/position
            window->rect = window->previousRect;
            window->maximized = false;
        } else {
            // Maximize to fill the entire current logical desktop
            // (the title bar will sit at the top of the logical area, content fills the rest)
            window->previousRect = window->rect;

            window->rect.x = 0;
            window->rect.y = 0;
            window->rect.w = m_logicalWidth;
            window->rect.h = m_logicalHeight;

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
    const int border = 8; // Resize border thickness (slightly generous for usability)

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

    const int minW = Window::MIN_WIDTH;
    const int minH = Window::MIN_HEIGHT + Window::TITLE_BAR_HEIGHT;

    SDL_Rect& r = window->rect;

    // Compute new dimensions while respecting minimums.
    // This prevents the window edge from ever moving past the min size
    // (the edge "stops" even if the user keeps dragging the mouse).
    int newX = r.x;
    int newY = r.y;
    int newW = r.w;
    int newH = r.h;

    switch (m_resizeDirection) {
        case ResizeDirection::Left: {
            int tentativeW = r.x + r.w - mouseX;
            if (tentativeW < minW) {
                newX = r.x + r.w - minW;
                newW = minW;
            } else {
                newX = mouseX;
                newW = tentativeW;
            }
            break;
        }
        case ResizeDirection::Right: {
            int tentativeW = mouseX - r.x;
            newW = std::max(minW, tentativeW);
            break;
        }
        case ResizeDirection::Top: {
            int tentativeH = r.y + r.h - mouseY;
            if (tentativeH < minH) {
                newY = r.y + r.h - minH;
                newH = minH;
            } else {
                newY = mouseY;
                newH = tentativeH;
            }
            break;
        }
        case ResizeDirection::Bottom: {
            int tentativeH = mouseY - r.y;
            newH = std::max(minH, tentativeH);
            break;
        }
        case ResizeDirection::TopLeft: {
            // Horizontal
            int tentativeW = r.x + r.w - mouseX;
            if (tentativeW < minW) {
                newX = r.x + r.w - minW;
                newW = minW;
            } else {
                newX = mouseX;
                newW = tentativeW;
            }
            // Vertical
            int tentativeH = r.y + r.h - mouseY;
            if (tentativeH < minH) {
                newY = r.y + r.h - minH;
                newH = minH;
            } else {
                newY = mouseY;
                newH = tentativeH;
            }
            break;
        }
        case ResizeDirection::TopRight: {
            newW = std::max(minW, mouseX - r.x);
            int tentativeH = r.y + r.h - mouseY;
            if (tentativeH < minH) {
                newY = r.y + r.h - minH;
                newH = minH;
            } else {
                newY = mouseY;
                newH = tentativeH;
            }
            break;
        }
        case ResizeDirection::BottomLeft: {
            int tentativeW = r.x + r.w - mouseX;
            if (tentativeW < minW) {
                newX = r.x + r.w - minW;
                newW = minW;
            } else {
                newX = mouseX;
                newW = tentativeW;
            }
            newH = std::max(minH, mouseY - r.y);
            break;
        }
        case ResizeDirection::BottomRight: {
            newW = std::max(minW, mouseX - r.x);
            newH = std::max(minH, mouseY - r.y);
            break;
        }
        default:
            break;
    }

    r.x = newX;
    r.y = newY;
    r.w = newW;
    r.h = newH;

    // Additional safety to keep title bar + buttons accessible
    if (r.y < 0) r.y = 0;
    if (r.y + Window::TITLE_BAR_HEIGHT > m_logicalHeight) {
        r.y = m_logicalHeight - Window::TITLE_BAR_HEIGHT;
    }
    if (r.x + r.w < 60) {
        r.x = 60 - r.w;
    }
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

void WindowManager::clampWindowsToDesktop() {
    for (auto& winPtr : m_windows) {
        if (winPtr) {
            clampSingleWindow(*winPtr);
        }
    }
}

void WindowManager::clampSingleWindow(Window& w) {
    if (w.minimized) return;

    const int titleH = Window::TITLE_BAR_HEIGHT;
    const int minButtonSpace = 80;

    SDL_Rect& r = w.rect;

    // Vertical - title bar fully visible
    if (r.y < 0) r.y = 0;
    if (r.y + titleH > m_logicalHeight) {
        r.y = m_logicalHeight - titleH;
    }

    // Horizontal - buttons must stay visible
    if (r.x + r.w < minButtonSpace) {
        r.x = minButtonSpace - r.w;
    }
    if (r.x > m_logicalWidth - minButtonSpace) {
        r.x = m_logicalWidth - minButtonSpace;
    }

    // Shrink if bigger than current desktop
    if (r.w > m_logicalWidth)  r.w = std::max(Window::MIN_WIDTH, m_logicalWidth);
    if (r.h > m_logicalHeight) r.h = std::max(Window::MIN_HEIGHT + titleH, m_logicalHeight);

    // Final bounds correction
    if (r.x < 0) r.x = 0;
    if (r.y < 0) r.y = 0;
    if (r.x + r.w > m_logicalWidth)  r.x = m_logicalWidth - r.w;
    if (r.y + titleH > m_logicalHeight) r.y = m_logicalHeight - titleH;
}

} // namespace monolith::window
