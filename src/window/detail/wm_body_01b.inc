            // Check for clicks directly on menu items (the actionable rows)
            for (const auto& item : m_startMenuItems) {
                SDL_Point p = {m_mouseX, m_mouseY};
                if (SDL_PointInRect(&p, &item.rect)) {
                    m_shellMouseCapture = true;
                    setStartMenuVisible(false);
#include "wm_start_menu_actions.inc"
                    return;
                }
            }

            // Click inside the menu rect (header or background, not on an item) → consume, keep menu open
            SDL_Point p = {m_mouseX, m_mouseY};
            if (SDL_PointInRect(&p, &m_startMenuRect)) {
                m_shellMouseCapture = true;
                return;
            }

            // Click was completely outside the menu → close it (but do not return yet,
            // so the click can still be processed by windows or other UI below).
            setStartMenuVisible(false);
        }

        if (m_mouseY >= taskbarScreenY && m_mouseY < taskbarScreenBottom) {
            clearDesktopIconSelection();
            m_shellMouseCapture = true;
            // Start button (when menu was not open, or after we handled the open case above)
            int startBtnScreenWidth = static_cast<int>(70 * m_contentScale);
            int startBtnScreenX = logicalToScreenX(8);
            const int startBtnScreenRight = std::min(
                startBtnScreenX + startBtnScreenWidth,
                taskbarScreenRect.x + taskbarScreenRect.w);

            if (m_mouseX >= startBtnScreenX && m_mouseX < startBtnScreenRight) {
                setStartMenuVisible(!m_showStartMenu);
                return;
            }

            // Scroll arrows (rects populated during render). Must run before the
            // unconditional taskbar return — a separate post-return handler never fired.
            if (m_taskbarNeedsScroll) {
                SDL_Point p = {m_mouseX, m_mouseY};
                if (SDL_PointInRect(&p, &m_taskbarLeftArrowRect)) {
                    m_taskbarScrollOffset = std::max(0, m_taskbarScrollOffset - 120);
                    setStartMenuVisible(false);
                    return;
                }
                if (SDL_PointInRect(&p, &m_taskbarRightArrowRect)) {
                    // Soft upper bound; drawing clamps which buttons stay visible.
                    m_taskbarScrollOffset += 120;
                    setStartMenuVisible(false);
                    return;
                }
            }

            // Taskbar window buttons (populated during render)
            for (const auto& entry : m_taskbarEntries) {
                SDL_Point p = {m_mouseX, m_mouseY};
                if (SDL_PointInRect(&p, &entry.rect)) {
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
                    setStartMenuVisible(false);
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
        const SDL_Rect taskbarScreenRect = logicalRectToScreen(getTaskbarRect());
        const int taskbarScreenY = taskbarScreenRect.y;
        const int taskbarScreenBottom = taskbarScreenRect.y + taskbarScreenRect.h;
        if (m_mouseY >= taskbarScreenY && m_mouseY < taskbarScreenBottom && m_taskbarNeedsScroll) {
            m_taskbarScrollOffset += (event.wheel.y > 0 ? -80 : 80);
            if (m_taskbarScrollOffset < 0) m_taskbarScrollOffset = 0;
            // Upper bound will be soft-limited by drawing (buttons just stop appearing)
            return;
        }
