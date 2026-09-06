            SDL_Rect dst = {
                trayX + padX,
                trayY + (trayH - m_clockTexH) / 2,
                m_clockTexW,
                m_clockTexH
            };
            SDL_RenderCopy(renderer, m_clockTexture, nullptr, &dst);
            m_clockHitRect = tray;

            SDL_Point mousePt = {m_mouseX, m_mouseY};
            if (m_clockDateTexture && SDL_PointInRect(&mousePt, &m_clockHitRect)) {
                const int tipPadX = static_cast<int>(6 * m_contentScale);
                const int tipPadY = static_cast<int>(3 * m_contentScale);
                const int tipW = m_clockDateTexW + tipPadX * 2;
                const int tipH = m_clockDateTexH + tipPadY * 2;
                int tipX = trayX + trayW - tipW;
                if (tipX < 0) {
                    tipX = 0;
                }
                const int tipY = trayY - tipH - static_cast<int>(4 * m_contentScale);

                SDL_SetRenderDrawColor(renderer, 50, 50, 58, 255);
                SDL_Rect tipBg = {tipX, tipY, tipW, tipH};
                SDL_RenderFillRect(renderer, &tipBg);
                SDL_SetRenderDrawColor(renderer, 110, 110, 120, 255);
                SDL_RenderDrawRect(renderer, &tipBg);

                SDL_Rect tipDst = {
                    tipX + tipPadX,
                    tipY + tipPadY,
                    m_clockDateTexW,
                    m_clockDateTexH
                };
                SDL_RenderCopy(renderer, m_clockDateTexture, nullptr, &tipDst);
            }
        }
    }

    // Alt+Tab switcher overlay
    if (m_altTabCycling && !m_altTabOrder.empty() && m_font) {
        Window* current = nullptr;
        if (m_altTabIndex >= 0 && m_altTabIndex < static_cast<int>(m_altTabOrder.size())) {
            current = m_altTabOrder[static_cast<size_t>(m_altTabIndex)];
        }
        const std::string label = current ? current->title : "(no window)";
        SDL_Color textCol = {235, 235, 240, 255};
        SDL_Surface* s = TTF_RenderUTF8_Blended(m_font, label.c_str(), textCol);
        if (s) {
            SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
            if (t) {
                const int boxW = std::min(m_logicalWidth - 80, std::max(220, s->w + 32));
                const int boxH = 52;
                SDL_Rect box = {
                    logicalToScreenX((m_logicalWidth - boxW) / 2),
                    logicalToScreenY((m_logicalHeight - boxH) / 2 - 20),
                    static_cast<int>(boxW * m_contentScale),
                    static_cast<int>(boxH * m_contentScale)
                };
                SDL_SetRenderDrawColor(renderer, 28, 30, 40, 240);
                SDL_RenderFillRect(renderer, &box);
                SDL_SetRenderDrawColor(renderer, 90, 120, 180, 255);
                SDL_RenderDrawRect(renderer, &box);
                SDL_Rect dst = {
                    box.x + (box.w - s->w) / 2,
                    box.y + (box.h - s->h) / 2,
                    s->w,
                    s->h
                };
                SDL_RenderCopy(renderer, t, nullptr, &dst);
                SDL_DestroyTexture(t);
            }
            SDL_FreeSurface(s);
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

        // When active window changes, reset taskbar scroll so the new leftmost (active) button is visible
        m_taskbarScrollOffset = 0;
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
        destroyClockTextures();
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

SDL_Rect WindowManager::getTaskbarRect() const {
    return {0, m_logicalHeight - kTaskbarHeight, m_logicalWidth, kTaskbarHeight};
}

SDL_Rect WindowManager::getUsableDesktopRect() const {
    SDL_Rect taskbar = getTaskbarRect();
    return {0, 0, m_logicalWidth, std::max(0, taskbar.y)};
}

int WindowManager::getUsableDesktopBottom() const {
    SDL_Rect usable = getUsableDesktopRect();
    return usable.y + usable.h;
}

SDL_Rect WindowManager::logicalRectToScreen(const SDL_Rect& rect) const {
    return {
        logicalToScreenX(rect.x),
        logicalToScreenY(rect.y),
        static_cast<int>(rect.w * m_contentScale),
        static_cast<int>(rect.h * m_contentScale)
    };
}

WindowManager::TitleButtonRects WindowManager::getTitleButtonRects(const Window& window) const {
    const int buttonSize = kWindowButtonSize;
    const int buttonY = window.rect.y + (Window::TITLE_BAR_HEIGHT - buttonSize) / 2;
    const int rightEdge = window.rect.x + window.rect.w - kWindowButtonRightPadding;

    TitleButtonRects rects;
    rects.close = {rightEdge - buttonSize, buttonY, buttonSize, buttonSize};
    rects.maximize = {
        rightEdge - buttonSize * 2 - kWindowButtonSpacing,
        buttonY,
        buttonSize,
        buttonSize
    };
    rects.minimize = {
        rightEdge - buttonSize * 3 - kWindowButtonSpacing * 2,
        buttonY,
        buttonSize,
        buttonSize
    };
    return rects;
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

    SDL_Point mouse = {mouseX, mouseY};
    const TitleButtonRects buttons = getTitleButtonRects(*window);

    if (SDL_PointInRect(&mouse, &buttons.close)) {
        closeWindow(window);
        return true;
    }

    if (SDL_PointInRect(&mouse, &buttons.maximize)) {
        if (window->maximized) {
            // Restore previous size/position
            window->rect = window->previousRect;
            window->maximized = false;
        } else {
            // Maximize to fill the logical desktop, leaving space for the taskbar at the bottom.
            // (the title bar sits at the top of the logical area, content fills the rest)
            window->previousRect = window->rect;

            window->rect = getUsableDesktopRect();

            window->maximized = true;
        }

        // Notify app of size change
        if (window->app) {
            const int clientH = window->rect.h - Window::TITLE_BAR_HEIGHT;
            window->app->onResize(window->rect.w, clientH > 0 ? clientH : 0);
        }
        return true;
    }

    if (SDL_PointInRect(&mouse, &buttons.minimize)) {
        window->minimized = true;
        return true;
    }

    return false;
}

ResizeDirection WindowManager::getResizeDirection(const Window& window, int mouseX, int mouseY) const {
    const int border = 8; // Resize border thickness (slightly generous for usability)

