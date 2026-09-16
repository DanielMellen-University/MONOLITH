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
                const SDL_Rect usableScreen = logicalRectToScreen(getUsableDesktopRect());
                int tipW = m_clockDateTexW + tipPadX * 2;
                int tipH = m_clockDateTexH + tipPadY * 2;
                tipW = std::min(tipW, std::max(0, usableScreen.w));
                tipH = std::min(tipH, std::max(0, usableScreen.h));
                if (tipW > 0 && tipH > 0) {
                    const int usableRight = usableScreen.x + usableScreen.w;
                    const int usableBottom = usableScreen.y + usableScreen.h;
                    int tipX = trayX + trayW - tipW;
                    int tipY = trayY - tipH - static_cast<int>(4 * m_contentScale);
                    tipX = std::clamp(tipX, usableScreen.x, usableRight - tipW);
                    tipY = std::clamp(tipY, usableScreen.y, usableBottom - tipH);

                    const SDL_Rect tipBg = {tipX, tipY, tipW, tipH};
                    m_clockTooltipRect = tipBg;
                    const SDL_Rect tooltipClip = intersectRendererClip(tipBg, callerClip);
                    if (tooltipClip.w > 0 && tooltipClip.h > 0) {
                        SDL_RenderSetClipRect(renderer, &tooltipClip);
                        SDL_SetRenderDrawColor(renderer, 50, 50, 58, 255);
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
                        restoreRendererClip(renderer, callerClip);
                    }
                }
            }
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
                const int logicalTextW = static_cast<int>(std::ceil(
                    static_cast<float>(s->w) / m_contentScale));
                const int maxBoxW = std::max(1, m_logicalWidth - 80);
                const int boxW = std::min(maxBoxW, std::max(220, logicalTextW + 32));
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
                const SDL_Rect effectiveBoxClip = intersectRendererClip(box, callerClip);
                SDL_RenderSetClipRect(renderer, &effectiveBoxClip);
                SDL_RenderCopy(renderer, t, nullptr, &dst);
                restoreRendererClip(renderer, callerClip);
                SDL_DestroyTexture(t);
            }
            SDL_FreeSurface(s);
        }
    }

    restoreRendererClip(renderer, callerClip);
}

Window* WindowManager::getWindowAt(int mouseX, int mouseY) {
    const int logicalMouseX = screenToLogicalX(mouseX);
    const int logicalMouseY = screenToLogicalY(mouseY);

    // Check from front to back (focused window first)
    for (auto it = m_windows.rbegin(); it != m_windows.rend(); ++it) {
        Window& win = **it;
        if (!win.minimized) {
            SDL_Rect logicalRect = win.rect;
            SDL_Point p = {logicalMouseX, logicalMouseY};
            if (SDL_PointInRect(&p, &logicalRect)) {
                return &win;
            }
        }
    }
    return nullptr;
}

bool WindowManager::isLiveWindow(const Window* window) const {
    if (!window) return false;
    return std::find_if(
        m_windows.begin(),
        m_windows.end(),
        [window](const auto& candidate) {
            return candidate.get() == window;
        }) != m_windows.end();
}

bool WindowManager::isLiveWindow(const Window* window, int expectedId) const {
    if (!window) return false;
    return std::find_if(
        m_windows.begin(),
        m_windows.end(),
        [window, expectedId](const auto& candidate) {
            return candidate.get() == window && candidate->id == expectedId;
        }) != m_windows.end();
}

void WindowManager::bringToFront(Window* window) {
    if (!isLiveWindow(window)) return;

    const SDL_Rect before = window->rect;
    const int windowId = window->id;

    // Any operation that explicitly focuses a window, including the
    // file-singleton bridges, must restore it if it was minimized.
    window->minimized = false;
    // A minimized window may have come from an old session or a changed
    // desktop size. Re-apply the shell geometry invariant before it becomes
    // visible so its title bar stays above the taskbar.
    clampSingleWindow(*window);
    notifyAppResizeIfGeometryChanged(*window, before);

    // Resize callbacks may close the target or replace its storage before the
    // z-order operation resumes.
    if (!isLiveWindow(window, windowId)) return;

    // Move the window to the end of the vector (drawn last = on top)
    auto it = std::find_if(m_windows.begin(), m_windows.end(),
        [window](const auto& w) { return w.get() == window; });

    if (it != m_windows.end()) {
        auto windowPtr = std::move(*it);
        m_windows.erase(it);
        m_windows.push_back(std::move(windowPtr));

        Window* newFocused = m_windows.back().get();
        Window* previousFocused = m_focusedWindow;
        const int newFocusedId = newFocused->id;
        const bool focusChanged = previousFocused != newFocused;

        // Publish the new focus before callbacks. A focus callback can close
        // its own window; closeWindow must then perform the handoff without
        // this stack frame restoring a destroyed pointer afterward.
        m_focusedWindow = newFocused;
        if (focusChanged && m_hostFocused && !m_showStartMenu
            && previousFocused && previousFocused->app) {
            invokeAppCallback([&]() { previousFocused->app->onFocusLost(); });
        }

        if (focusChanged && m_hostFocused && !m_showStartMenu
            && m_focusedWindow == newFocused
            && isLiveWindow(newFocused, newFocusedId) && newFocused->app) {
            invokeAppCallback([&]() { newFocused->app->onFocusGained(); });
        }

        // When active window changes, reset taskbar scroll so the new leftmost (active) button is visible
        m_taskbarScrollOffset = 0;
    }
}

void WindowManager::focusTopmostVisibleWindow() {
    Window* successor = nullptr;
    for (auto it = m_windows.rbegin(); it != m_windows.rend(); ++it) {
        if (*it && !(*it)->minimized) {
            successor = it->get();
            break;
        }
    }

    if (successor) {
        bringToFront(successor);
        return;
    }

    Window* previousFocused = m_focusedWindow;
    m_focusedWindow = nullptr;
    if (m_hostFocused && !m_showStartMenu
        && previousFocused && previousFocused->app) {
        invokeAppCallback([&]() { previousFocused->app->onFocusLost(); });
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
    applyUiScaleFont();
}

void WindowManager::invalidateShellHitTargets() {
    m_taskbarEntries.clear();
    m_taskbarNeedsScroll = false;
    m_taskbarLeftArrowRect = {0, 0, 0, 0};
    m_taskbarRightArrowRect = {0, 0, 0, 0};
    m_clockHitRect = {0, 0, 0, 0};
    m_clockTooltipRect = {0, 0, 0, 0};
    m_startMenuItems.clear();
    m_startMenuHoverIndex = -1;
    m_startMenuRect = {0, 0, 0, 0};
}

void WindowManager::setLogicalDesktopSize(int width, int height) {
    m_logicalWidth = std::max(1, width);
    m_logicalHeight = std::max(1, height);
    invalidateShellHitTargets();

    // Re-clamp when logical size changes and keep each app's client geometry
    // synchronized when the shell had to move or resize its window. The stable
    // snapshot keeps an onResize callback from invalidating this pass.
    forEachLiveWindow([this](Window& window) {
        if (window.minimized) return;

        const SDL_Rect before = window.rect;
        if (window.maximized) {
            window.rect = getUsableDesktopRect();
        } else {
            clampSingleWindow(window);
        }
        if (window.rect.x == before.x
            && window.rect.y == before.y
            && window.rect.w == before.w
            && window.rect.h == before.h) {
            return;
        }
        notifyAppResizeIfGeometryChanged(window, before);
    });
}

void WindowManager::getLogicalDesktopSize(int& width, int& height) const {
    width = m_logicalWidth;
    height = m_logicalHeight;
}

void WindowManager::setHeaderOffset(int offsetY) {
    m_headerOffset = offsetY;
    invalidateShellHitTargets();
}

void WindowManager::setContentScale(float scale) {
    m_contentScale = scale > 0.0f ? scale : 1.0f;
    invalidateShellHitTargets();
}

void WindowManager::applyUiScaleFont() {
    if (!m_font) {
        invalidateShellHitTargets();
        return;
    }

    const int percent = m_desktopSettings.uiScalePercent();
    const int pointSize = std::clamp((kBaseFontPointSize * percent + 50) / 100, 10, 22);
    if (TTF_SetFontSize(m_font, pointSize) != 0) return;

    for (auto& [id, entry] : m_titleCache) {
        (void)id;
        if (entry.texture) {
            SDL_DestroyTexture(entry.texture);
            entry.texture = nullptr;
        }
    }
    m_titleCache.clear();
    destroyClockTextures();
    invalidateShellHitTargets();
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
    const int logicalMouseX = screenToLogicalX(mouseX);
    const int logicalMouseY = screenToLogicalY(mouseY);
    // Check from front to back
    for (auto it = m_windows.rbegin(); it != m_windows.rend(); ++it) {
        Window& win = **it;
        if (win.minimized || win.maximized) continue;

        SDL_Point p = {logicalMouseX, logicalMouseY};
        if (SDL_PointInRect(&p, &win.rect)) {
            return getResizeDirection(win, logicalMouseX, logicalMouseY);
        }
    }
    return ResizeDirection::None;
}

bool WindowManager::handleTitleBarButtons(Window* window, int mouseX, int mouseY) {
    if (!window) return false;

    const SDL_Rect before = window->rect;
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

        clampSingleWindow(*window);
        notifyAppResizeIfGeometryChanged(*window, before);
        return true;
    }

    if (SDL_PointInRect(&mouse, &buttons.minimize)) {
        window->minimized = true;
        if (m_focusedWindow == window) {
            focusTopmostVisibleWindow();
        }
        return true;
    }

    return false;
}

ResizeDirection WindowManager::getResizeDirection(const Window& window, int mouseX, int mouseY) const {
    const int border = 8; // Resize border thickness (slightly generous for usability)
