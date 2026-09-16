            // Check for clicks directly on menu items (the actionable rows)
            for (const auto& item : m_startMenuItems) {
                SDL_Point p = {m_mouseX, m_mouseY};
                if (SDL_PointInRect(&p, &item.rect)) {
                    m_shellMouseCapture = true;
                    activateStartMenuItem(item.action);
                    return;
                }
            }

            // Click inside the menu rect (header or background, not on an item) → consume, keep menu open
            SDL_Point p = {m_mouseX, m_mouseY};
            if (SDL_PointInRect(&p, &m_startMenuRect)) {
                m_shellMouseCapture = true;
                return;
            }

            // Click was completely outside the menu. If it targets another
            // window, let the normal focus path consume the suspension marker
            // instead of briefly resuming the old app first.
            const Window* clickedWindow = getWindowAt(m_mouseX, m_mouseY);
            const bool clickChangesFocus = clickedWindow
                && clickedWindow != m_focusedWindow;
            const bool clickIsOnTaskbar = m_mouseY >= taskbarScreenY
                && m_mouseY < taskbarScreenBottom;
            if (!clickIsOnTaskbar) {
                setStartMenuVisible(false, !clickChangesFocus);
            }
        }

        if (m_mouseY >= taskbarScreenY && m_mouseY < taskbarScreenBottom) {
            clearDesktopIconSelection();
            m_shellMouseCapture = true;
            ensureTaskbarHitTargets();
            // Start button (when menu was not open, or after we handled the open case above)
            SDL_Point startPoint{m_mouseX, m_mouseY};
            if (SDL_PointInRect(&startPoint, &taskbarLayout.startButtonRect)) {
                setStartMenuVisible(!m_showStartMenu);
                return;
            }

            // Scroll arrows (rects populated during render). Must run before the
            // unconditional taskbar return — a separate post-return handler never fired.
            if (m_taskbarNeedsScroll) {
                SDL_Point p = {m_mouseX, m_mouseY};
                if (SDL_PointInRect(&p, &taskbarLayout.leftArrowRect)) {
                    m_taskbarScrollOffset = std::max(0, m_taskbarScrollOffset - 120);
                    invalidateShellHitTargets();
                    setStartMenuVisible(false);
                    return;
                }
                if (SDL_PointInRect(&p, &taskbarLayout.rightArrowRect)) {
                    // Soft upper bound; drawing clamps which buttons stay visible.
                    m_taskbarScrollOffset += 120;
                    invalidateShellHitTargets();
                    setStartMenuVisible(false);
                    return;
                }
            }

            // Taskbar window buttons (populated during render)
            for (const auto& entry : m_taskbarEntries) {
                SDL_Point p = {m_mouseX, m_mouseY};
                if (SDL_PointInRect(&p, &entry.rect)) {
                    // Close the modal boundary before the taskbar action so
                    // bringToFront can deliver the new focus callback.
                    setStartMenuVisible(false, false);
                    if (entry.window) {
                        if (entry.window->minimized) {
                            entry.window->minimized = false;
                            bringToFront(entry.window);
                        } else if (entry.window == m_focusedWindow) {
                            // Clicking the active window's taskbar button → minimize it (XP-like toggle)
                            entry.window->minimized = true;
                            focusTopmostVisibleWindow();
                        } else {
                            bringToFront(entry.window);
                        }
                    }
                    return;
                }
            }

            setStartMenuVisible(false);
            return;
        }
    }

    if (event.type == SDL_MOUSEBUTTONUP
        && event.button.button == SDL_BUTTON_LEFT
        && m_shellMouseCapture) {
        m_mouseX = event.button.x;
        m_mouseY = event.button.y;
        m_mouseDown = false;
        m_shellMouseCapture = false;
        return;
    }

    // Mouse wheel over taskbar scrolls the window buttons
    if (event.type == SDL_MOUSEWHEEL) {
        const TaskbarLayout taskbarLayout = computeTaskbarLayout();
        const SDL_Rect taskbarScreenRect = taskbarLayout.taskbarRect;
        const int taskbarScreenY = taskbarScreenRect.y;
        const int taskbarScreenBottom = taskbarScreenRect.y + taskbarScreenRect.h;
        ensureTaskbarHitTargets();
        if (m_mouseY >= taskbarScreenY && m_mouseY < taskbarScreenBottom && m_taskbarNeedsScroll) {
            m_taskbarScrollOffset += (event.wheel.y > 0 ? -80 : 80);
            if (m_taskbarScrollOffset < 0) m_taskbarScrollOffset = 0;
            // Upper bound will be soft-limited by drawing (buttons just stop appearing)
            invalidateShellHitTargets();
            return;
        }
