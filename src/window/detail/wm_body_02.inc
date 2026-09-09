        // Forward wheel events to the focused app's client area (Settings scroll, etc.).
        if (m_focusedWindow && m_focusedWindow->app && !m_focusedWindow->minimized && !m_showStartMenu) {
            if (isInContentArea(*m_focusedWindow, logicalMouseX, logicalMouseY)) {
                m_focusedWindow->app->handleEvent(event);
                return;
            }
        }
    }

    // === Normal internal window interaction ===
    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        m_mouseDown = true;
        m_mouseX = event.button.x;
        m_mouseY = event.button.y;
        const int eventLogicalX = screenToLogicalX(m_mouseX);
        const int eventLogicalY = screenToLogicalY(m_mouseY);

        Window* clicked = getWindowAt(m_mouseX, m_mouseY);
        if (!clicked) return;

        bringToFront(clicked);

        // === Priority 1: Title bar buttons ===
        if (isInTitleBar(*clicked, eventLogicalX, eventLogicalY)) {
            if (handleTitleBarButtons(clicked, eventLogicalX, eventLogicalY)) {
                return;
            }
        }

        // === Priority 2: Resize borders ===
        ResizeDirection dir = getResizeDirection(*clicked, eventLogicalX, eventLogicalY);
        if (dir != ResizeDirection::None && !clicked->maximized) {
            m_resizingWindow = clicked;
            m_resizeDirection = dir;
            return;
        }

        // === Priority 3: Title bar drag ===
        if (isInTitleBar(*clicked, eventLogicalX, eventLogicalY)) {
            m_draggedWindow = clicked;
            clicked->beingDragged = true;
            clicked->dragOffsetX = eventLogicalX - clicked->rect.x;
            clicked->dragOffsetY = eventLogicalY - clicked->rect.y;
            return;
        }

        // === Priority 4: Client area of the window ===
        // Forward to the app (if any), with coordinates translated to the app's local space.
        if (isInContentArea(*clicked, eventLogicalX, eventLogicalY)) {
            if (clicked->app) {
                if (!m_mouseCaptureWindow) {
                    m_mouseCaptureWindow = clicked;
                }
                SDL_Event clientEvent = event;
                translateMouseEventToClient(*clicked, clientEvent);
                clicked->app->handleEvent(clientEvent);
            }
            return;
        }
    }

    // === Non-left mouse buttons (right-click for context menus, etc.) ===
    // These still need to reach the app's client area, but skip all the drag/resize/titlebar logic.
    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button != SDL_BUTTON_LEFT) {
        m_mouseX = event.button.x;
        m_mouseY = event.button.y;
        const int eventLogicalX = screenToLogicalX(m_mouseX);
        const int eventLogicalY = screenToLogicalY(m_mouseY);

        Window* clicked = getWindowAt(m_mouseX, m_mouseY);
        if (clicked) {
            bringToFront(clicked);

            if (isInContentArea(*clicked, eventLogicalX, eventLogicalY)) {
                if (clicked->app) {
                    SDL_Event clientEvent = event;
                    translateMouseEventToClient(*clicked, clientEvent);
                    clicked->app->handleEvent(clientEvent);
                }
                return;
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

        // Forward release to the client that received the press. This is a small
        // pointer-capture rule: focus and pointer location may have changed since down.
        Window* releaseWindow = m_mouseCaptureWindow;
        m_mouseCaptureWindow = nullptr;
        if (releaseWindow && releaseWindow->app) {
            SDL_Event clientEvent = event;
            translateMouseEventToClient(*releaseWindow, clientEvent);
            releaseWindow->app->handleEvent(clientEvent);
        } else if (m_focusedWindow && m_focusedWindow->app && !m_focusedWindow->minimized && !m_showStartMenu) {
            SDL_Event clientEvent = event;
            translateMouseEventToClient(*m_focusedWindow, clientEvent);
            m_focusedWindow->app->handleEvent(clientEvent);
        }
    }
}

void WindowManager::update() {
    const int logicalMouseX = screenToLogicalX(m_mouseX);
    const int logicalMouseY = screenToLogicalY(m_mouseY);

    if (m_draggedWindow && m_mouseDown) {
        m_draggedWindow->rect.x = logicalMouseX - m_draggedWindow->dragOffsetX;
        m_draggedWindow->rect.y = logicalMouseY - m_draggedWindow->dragOffsetY;

        // Prevent user from dragging the window so far that buttons become inaccessible
        clampSingleWindow(*m_draggedWindow);
    }

    if (m_resizingWindow && m_mouseDown) {
        applyResize(m_resizingWindow, logicalMouseX, logicalMouseY);
        clampSingleWindow(*m_resizingWindow);
    }

    // Enforce maximized windows fill the full logical area (minus the always-visible taskbar)
    // The taskbar is permanently at the bottom, so all maximized windows must
    // always reserve space for it. This is a general rule for the entire desktop shell,
    // not something individual apps should have to handle.
    for (auto& w : m_windows) {
        if (w->maximized && !w->minimized) {
            SDL_Rect usable = getUsableDesktopRect();
            w->rect = usable;
        }
    }

    // Per-frame app updates (games ticks, timers, etc.). Skip minimized windows.
    for (auto& w : m_windows) {
        if (w && w->app && !w->minimized) {
            w->app->update();
        }
    }
}

void WindowManager::render(SDL_Renderer* renderer) {
    // Solid color is cleared by main; paint wallpaper image (if any) under windows.
    renderWallpaper(renderer);

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
        const TitleButtonRects logicalButtons = getTitleButtonRects(win);
        SDL_Rect closeBtn = logicalRectToScreen(logicalButtons.close);
        SDL_Rect maxBtn = logicalRectToScreen(logicalButtons.maximize);
        SDL_Rect minBtn = logicalRectToScreen(logicalButtons.minimize);
        const int buttonSize = closeBtn.w;

        // Close button (red) - draw white X
        SDL_SetRenderDrawColor(renderer, 200, 60, 60, 255);
        SDL_RenderFillRect(renderer, &closeBtn);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_RenderDrawLine(renderer, closeBtn.x + 3, closeBtn.y + 3, closeBtn.x + buttonSize - 4, closeBtn.y + buttonSize - 4);
        SDL_RenderDrawLine(renderer, closeBtn.x + buttonSize - 4, closeBtn.y + 3, closeBtn.x + 3, closeBtn.y + buttonSize - 4);
