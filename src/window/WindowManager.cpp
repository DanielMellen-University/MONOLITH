#include "WindowManager.hpp"
#include "../app/App.hpp"
#include <algorithm>

namespace monolith::window {

WindowManager::WindowManager() = default;

WindowManager::~WindowManager() = default;

Window* WindowManager::createWindow(const std::string& title, int x, int y, int w, int h,
                                        std::unique_ptr<monolith::app::App> app) {
    auto window = std::make_unique<Window>();
    window->id = m_nextId++;
    window->title = title;
    window->rect.x = x;
    window->rect.y = y;
    window->rect.w = w;
    window->rect.h = h;
    window->app = std::move(app);

    Window* ptr = window.get();
    m_windows.push_back(std::move(window));

    // Attach controller to the app (if any) so it can request window operations
    if (ptr->app) {
        ptr->app->setController(createControllerFor(ptr));

        // Initial size notification
        const int clientH = ptr->rect.h - Window::TITLE_BAR_HEIGHT;
        ptr->app->onResize(ptr->rect.w, clientH > 0 ? clientH : 0);
    }

    bringToFront(ptr);
    return ptr;
}

void WindowManager::handleEvent(const SDL_Event& event) {
    if (event.type == SDL_MOUSEMOTION) {
        m_mouseX = event.motion.x;
        m_mouseY = event.motion.y;

        // Forward motion to the focused window's app if the pointer is in its content area
        if (m_focusedWindow && m_focusedWindow->app && !m_focusedWindow->minimized) {
            const int logicalY = screenToLogicalY(m_mouseY);
            if (isInContentArea(*m_focusedWindow, m_mouseX, logicalY)) {
                SDL_Event clientEvent = event;
                translateMouseEventToClient(*m_focusedWindow, clientEvent);
                m_focusedWindow->app->handleEvent(clientEvent);
            }
        }
    }

    const int logicalMouseY = screenToLogicalY(m_mouseY);

    // Forward keyboard events (text input, key down/up) to the focused window's app
    if (m_focusedWindow && m_focusedWindow->app &&
        (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP || event.type == SDL_TEXTINPUT)) {
        m_focusedWindow->app->handleEvent(event);
        // Do not return — WM doesn't consume keyboard yet, but apps get first crack
    }

    // === Global UI first: Taskbar + Start Menu (must be checked even if click misses all windows) ===
    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        m_mouseDown = true;
        m_mouseX = event.button.x;
        m_mouseY = event.button.y;

        const int taskbarHeight = 28;
        const int taskbarScreenY = logicalToScreenY(m_logicalHeight - taskbarHeight);
        const int taskbarScreenBottom = logicalToScreenY(m_logicalHeight);

        if (m_mouseY >= taskbarScreenY && m_mouseY < taskbarScreenBottom) {
            // Start button
            int startBtnScreenWidth = static_cast<int>(70 * m_contentScale);
            int startBtnScreenX = logicalToScreenX(8);

            if (m_mouseX >= startBtnScreenX && m_mouseX < startBtnScreenX + startBtnScreenWidth) {
                m_showStartMenu = !m_showStartMenu;
                return;
            }

            // Minimized windows in taskbar
            for (const auto& entry : m_taskbarEntries) {
                SDL_Point p = {m_mouseX, m_mouseY};
                if (SDL_PointInRect(&p, &entry.rect)) {
                    if (entry.window) {
                        entry.window->minimized = false;
                        bringToFront(entry.window);
                        m_showStartMenu = false;
                    }
                    return;
                }
            }

            m_showStartMenu = false;
            return;
        } else {
            m_showStartMenu = false;
        }
    }

    // === Normal internal window interaction ===
    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        m_mouseDown = true;
        m_mouseX = event.button.x;
        m_mouseY = event.button.y;

        Window* clicked = getWindowAt(m_mouseX, m_mouseY);
        if (!clicked) return;

        bringToFront(clicked);

        // === Priority 1: Title bar buttons ===
        if (isInTitleBar(*clicked, m_mouseX, logicalMouseY)) {
            if (handleTitleBarButtons(clicked, m_mouseX, logicalMouseY)) {
                return;
            }
        }

        // === Priority 2: Resize borders ===
        ResizeDirection dir = getResizeDirection(*clicked, m_mouseX, logicalMouseY);
        if (dir != ResizeDirection::None && !clicked->maximized) {
            m_resizingWindow = clicked;
            m_resizeDirection = dir;
            return;
        }

        // === Priority 3: Title bar drag ===
        if (isInTitleBar(*clicked, m_mouseX, logicalMouseY)) {
            m_draggedWindow = clicked;
            clicked->beingDragged = true;
            clicked->dragOffsetX = m_mouseX - clicked->rect.x;
            clicked->dragOffsetY = logicalMouseY - clicked->rect.y;
            return;
        }

        // === Priority 4: Client area of the window ===
        // Forward to the app (if any), with coordinates translated to the app's local space.
        if (isInContentArea(*clicked, m_mouseX, logicalMouseY)) {
            if (clicked->app) {
                SDL_Event clientEvent = event;
                translateMouseEventToClient(*clicked, clientEvent);
                clicked->app->handleEvent(clientEvent);
            }
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

        // Delegate to the app (if present) so it can draw its own content
        if (win.app) {
            win.app->render(renderer, contentRect);
        }

        // === Title bar ===
        bool isFocused = (&win == m_focusedWindow);
        SDL_Color titleColor = isFocused ? SDL_Color{60, 60, 70, 255}
                                         : SDL_Color{40, 40, 48, 255};

        SDL_SetRenderDrawColor(renderer, titleColor.r, titleColor.g, titleColor.b, 255);
        SDL_Rect titleBar = {screenX, screenY, scaledW, scaledTitleH};
        SDL_RenderFillRect(renderer, &titleBar);

        // === Window title text (cached) ===
        if (m_font && !win.title.empty()) {
            auto& cache = m_titleCache[win.id];
            bool needsUpdate = (cache.texture == nullptr) ||
                               (cache.lastTitle != win.title) ||
                               (cache.wasFocused != isFocused);

            if (needsUpdate) {
                if (cache.texture) {
                    SDL_DestroyTexture(cache.texture);
                }

                SDL_Color textColor = isFocused ? SDL_Color{230, 230, 235, 255}
                                                : SDL_Color{180, 180, 185, 255};

                SDL_Surface* surface = TTF_RenderText_Blended(m_font, win.title.c_str(), textColor);
                if (surface) {
                    cache.texture = SDL_CreateTextureFromSurface(renderer, surface);
                    SDL_FreeSurface(surface);
                }
                cache.lastTitle = win.title;
                cache.wasFocused = isFocused;
            }

            if (cache.texture) {
                int texW, texH;
                SDL_QueryTexture(cache.texture, nullptr, nullptr, &texW, &texH);

                int paddingLeft = 10;
                int availableWidth = scaledW - 80;

                SDL_Rect dstRect = {
                    screenX + paddingLeft,
                    screenY + (scaledTitleH - texH) / 2,
                    texW,
                    texH
                };

                if (texW > availableWidth) {
                    dstRect.w = availableWidth;
                }

                SDL_RenderCopy(renderer, cache.texture, nullptr, &dstRect);
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

    // === Start Menu popup (very basic placeholder) ===
    if (m_showStartMenu) {
        const int menuWidth = 200;
        const int menuHeight = 280;
        const int menuX = logicalToScreenX(8);
        const int menuY = logicalToScreenY(m_logicalHeight - 28 - menuHeight); // above taskbar

        SDL_SetRenderDrawColor(renderer, 45, 45, 55, 255);
        SDL_Rect menuRect = {
            menuX,
            menuY,
            static_cast<int>(menuWidth * m_contentScale),
            static_cast<int>(menuHeight * m_contentScale)
        };
        SDL_RenderFillRect(renderer, &menuRect);

        SDL_SetRenderDrawColor(renderer, 80, 80, 90, 255);
        SDL_RenderDrawRect(renderer, &menuRect);

        if (m_font) {
            SDL_Color textCol = {230, 230, 235, 255};
            const char* items[] = {"Terminal", "Filesystem", "Settings", "About Monolith", "Shut Down"};
            int y = menuY + static_cast<int>(10 * m_contentScale);
            for (const char* item : items) {
                SDL_Surface* s = TTF_RenderText_Blended(m_font, item, textCol);
                if (s) {
                    SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
                    if (t) {
                        SDL_Rect dst = {menuX + static_cast<int>(12 * m_contentScale), y, s->w, s->h};
                        SDL_RenderCopy(renderer, t, nullptr, &dst);
                        SDL_DestroyTexture(t);
                    }
                    y += static_cast<int>(28 * m_contentScale);
                    SDL_FreeSurface(s);
                }
            }
        }
    }

    // === Taskbar (always visible, like a real desktop) ===
    m_taskbarEntries.clear();

    const int taskbarHeight = 28;
    const int taskbarY = m_logicalHeight - taskbarHeight;

    // Draw taskbar background
    SDL_SetRenderDrawColor(renderer, 35, 35, 40, 255);
    SDL_Rect taskbarRect = {
        logicalToScreenX(0),
        logicalToScreenY(taskbarY),
        static_cast<int>(m_logicalWidth * m_contentScale),
        static_cast<int>(taskbarHeight * m_contentScale)
    };
    SDL_RenderFillRect(renderer, &taskbarRect);

    // --- Left: Start Menu button (placeholder) ---
    int x = logicalToScreenX(8);
    const int startBtnWidth = static_cast<int>(70 * m_contentScale);
    const int btnHeight = static_cast<int>(22 * m_contentScale);
    const int btnY = logicalToScreenY(taskbarY) + (taskbarRect.h - btnHeight) / 2;

    SDL_SetRenderDrawColor(renderer, 60, 90, 150, 255); // blue-ish start button
    SDL_Rect startBtn = {x, btnY, startBtnWidth, btnHeight};
    SDL_RenderFillRect(renderer, &startBtn);

    if (m_font) {
        SDL_Color textCol = {255, 255, 255, 255};
        SDL_Surface* s = TTF_RenderText_Blended(m_font, "Start", textCol);
        if (s) {
            SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
            if (t) {
                SDL_Rect dst = {x + 8, btnY + (btnHeight - s->h) / 2, s->w, s->h};
                SDL_RenderCopy(renderer, t, nullptr, &dst);
                SDL_DestroyTexture(t);
            }
            SDL_FreeSurface(s);
        }
    }

    // Store start button for clicking (we'll treat it specially)
    // For simplicity, we'll handle it with a magic x range in handleEvent for now.

    x += startBtnWidth + static_cast<int>(8 * m_contentScale);

    // --- Middle: Minimized windows ---
    for (auto& w : m_windows) {
        if (!w->minimized) continue;

        int btnWidth = std::max(90, static_cast<int>(w->title.length() * 7 * m_contentScale));
        SDL_SetRenderDrawColor(renderer, 70, 70, 80, 255);
        SDL_Rect btnRect = {x, btnY, btnWidth, btnHeight};
        SDL_RenderFillRect(renderer, &btnRect);

        if (m_font) {
            SDL_Color textCol = {230, 230, 235, 255};
            SDL_Surface* s = TTF_RenderText_Blended(m_font, w->title.c_str(), textCol);
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

        // Record for click handling
        m_taskbarEntries.push_back({btnRect, w.get()});
        x += btnWidth + static_cast<int>(6 * m_contentScale);
    }

    // --- Right: System clock (12-hour AM/PM) ---
    {
        time_t now = time(nullptr);
        struct tm* tm_info = localtime(&now);
        char timeStr[32];
        strftime(timeStr, sizeof(timeStr), "%I:%M %p", tm_info);

        if (m_font) {
            SDL_Color textCol = {230, 230, 235, 255};
            SDL_Surface* s = TTF_RenderText_Blended(m_font, timeStr, textCol);
            if (s) {
                SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
                if (t) {
                    int clockX = logicalToScreenX(m_logicalWidth) - s->w - static_cast<int>(12 * m_contentScale);
                    int clockY = btnY + (btnHeight - s->h) / 2;
                    SDL_Rect dst = {clockX, clockY, s->w, s->h};
                    SDL_RenderCopy(renderer, t, nullptr, &dst);
                    SDL_DestroyTexture(t);
                }
                SDL_FreeSurface(s);
            }
        }
    }
}

Window* WindowManager::getWindowAt(int mouseX, int mouseY) {
    const int logicalMouseY = screenToLogicalY(mouseY);

    // Check from front to back (focused window first)
    for (auto it = m_windows.rbegin(); it != m_windows.rend(); ++it) {
        Window& win = **it;
        if (!win.minimized) {
            SDL_Rect logicalRect = win.rect;
            SDL_Point p = {mouseX, logicalMouseY};
            if (SDL_PointInRect(&p, &logicalRect)) {
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

        Window* newFocused = m_windows.back().get();

        // Notify focus change
        if (m_focusedWindow && m_focusedWindow != newFocused && m_focusedWindow->app) {
            m_focusedWindow->app->onFocusLost();
        }
        if (newFocused && newFocused->app && m_focusedWindow != newFocused) {
            newFocused->app->onFocusGained();
        }

        m_focusedWindow = newFocused;
    }
}

bool WindowManager::isInTitleBar(const Window& window, int x, int y) const {
    SDL_Rect titleBar = {
        window.rect.x,
        window.rect.y,
        window.rect.w,
        Window::TITLE_BAR_HEIGHT
    };
    SDL_Point p = {x, y};
    return SDL_PointInRect(&p, &titleBar);
}

void WindowManager::setFont(TTF_Font* font) {
    if (m_font != font) {
        // Invalidate all title caches when font changes
        for (auto& [id, entry] : m_titleCache) {
            if (entry.texture) {
                SDL_DestroyTexture(entry.texture);
            }
        }
        m_titleCache.clear();
    }
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

        SDL_Point p = {mouseX, mouseY};
        if (SDL_PointInRect(&p, &win.rect)) {
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

        // Notify app of size change
        if (window->app) {
            const int clientH = window->rect.h - Window::TITLE_BAR_HEIGHT;
            window->app->onResize(window->rect.w, clientH > 0 ? clientH : 0);
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

    // Notify the app of the new client size (if any)
    if (window->app) {
        const int clientW = r.w;
        const int clientH = r.h - Window::TITLE_BAR_HEIGHT;
        window->app->onResize(clientW, clientH > 0 ? clientH : 0);
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

        // Clean up cached title texture
        auto cacheIt = m_titleCache.find(window->id);
        if (cacheIt != m_titleCache.end()) {
            if (cacheIt->second.texture) {
                SDL_DestroyTexture(cacheIt->second.texture);
            }
            m_titleCache.erase(cacheIt);
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

// =============================================================================
// Client area helpers
// =============================================================================

bool WindowManager::isInContentArea(const Window& window, int logicalX, int logicalY) const {
    const int titleH = Window::TITLE_BAR_HEIGHT;
    SDL_Rect contentRect = {
        window.rect.x,
        window.rect.y + titleH,
        window.rect.w,
        window.rect.h - titleH
    };
    SDL_Point p = {logicalX, logicalY};
    return SDL_PointInRect(&p, &contentRect);
}

void WindowManager::translateMouseEventToClient(const Window& window, SDL_Event& event) const {
    const int titleH = Window::TITLE_BAR_HEIGHT;
    const int clientOriginX = window.rect.x;
    const int clientOriginY = window.rect.y + titleH;

    if (event.type == SDL_MOUSEMOTION) {
        event.motion.x -= clientOriginX;
        event.motion.y -= clientOriginY;
    } else if (event.type == SDL_MOUSEBUTTONDOWN || event.type == SDL_MOUSEBUTTONUP) {
        event.button.x -= clientOriginX;
        event.button.y -= clientOriginY;
    } else if (event.type == SDL_MOUSEWHEEL) {
        // Wheel events don't have x/y in the same way; apps can use mouse position separately if needed
    }
}

// =============================================================================
// Per-window controller for apps (IWindowController implementation)
// =============================================================================

namespace {

// Small concrete controller that lets an App affect its host window.
struct WindowController : public monolith::app::IWindowController {
    monolith::window::WindowManager* wm;
    monolith::window::Window* targetWindow;

    void close() override {
        if (wm && targetWindow) wm->closeWindow(targetWindow);
    }

    void setTitle(const std::string& title) override {
        if (targetWindow) {
            targetWindow->title = title;
        }
    }
};

} // anonymous namespace

monolith::app::IWindowController* WindowManager::createControllerFor(Window* window) {
    // We allocate a small controller per window that has an app.
    // Lifetime is tied to the app (destroyed when the window/app is closed).
    // For simplicity we leak them for now (or we could store them in a map).
    // A better long-term solution is to store them alongside the window.
    static std::vector<std::unique_ptr<WindowController>> s_controllers;

    auto ctrl = std::make_unique<WindowController>();
    ctrl->wm = this;
    ctrl->targetWindow = window;

    monolith::app::IWindowController* raw = ctrl.get();
    s_controllers.push_back(std::move(ctrl));
    return raw;
}

} // namespace monolith::window
