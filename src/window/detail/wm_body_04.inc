    const SDL_Rect taskbarLogicalRect = getTaskbarRect();
    const SDL_Rect taskbarRect = logicalRectToScreen(taskbarLogicalRect);
    const int taskbarY = taskbarLogicalRect.y;

    // Draw taskbar background
    SDL_SetRenderDrawColor(renderer, 35, 35, 42, 255);
    SDL_RenderFillRect(renderer, &taskbarRect);

    // --- Left: Start button ---
    int x = logicalToScreenX(8);
    const int startBtnWidth = static_cast<int>(70 * m_contentScale);
    const int btnHeight = static_cast<int>(22 * m_contentScale);
    const int btnY = logicalToScreenY(taskbarY) + (taskbarRect.h - btnHeight) / 2;

    SDL_SetRenderDrawColor(renderer, 55, 85, 140, 255);
    SDL_Rect startBtn = {x, btnY, startBtnWidth, btnHeight};
    SDL_RenderFillRect(renderer, &startBtn);

    if (m_font) {
        SDL_Color textCol = {255, 255, 255, 255};
        SDL_Surface* s = TTF_RenderText_Blended(m_font, "Start", textCol);
        if (s) {
            SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
            if (t) {
                SDL_Rect dst = {x + 8, btnY + (btnHeight - s->h)/2, s->w, s->h};
                SDL_RenderCopy(renderer, t, nullptr, &dst);
                SDL_DestroyTexture(t);
            }
            SDL_FreeSurface(s);
        }
    }

    // Compute the button area from the actual logical width. Very narrow
    // desktops still need non-negative hit rectangles and a usable partial
    // button instead of a negative viewport.
    const int clockReserve = 100; // desired logical space for the clock tray
    const int taskbarButtonStart = 8 + 70 + 8;
    m_taskbarButtonAreaLeft = std::clamp(taskbarButtonStart, 0, m_logicalWidth);
    const int availableAfterStart = std::max(
        0, m_logicalWidth - m_taskbarButtonAreaLeft - 8);
    const int adaptiveClockReserve = std::min(
        clockReserve,
        std::max(0, availableAfterStart - kMinimumVisibleButtonSpace));
    m_taskbarButtonAreaWidth = std::max(
        0, availableAfterStart - adaptiveClockReserve);

    // Calculate total width + prepare drawing order (active/focused window always leftmost)
    std::vector<Window*> buttonOrder;
    if (m_focusedWindow) {
        buttonOrder.push_back(m_focusedWindow);
    }
    for (auto& w : m_windows) {
        if (w.get() != m_focusedWindow) {
            buttonOrder.push_back(w.get());
        }
    }

    int totalButtonsWidth = 0;
    std::vector<int> buttonWidths;
    for (Window* win : buttonOrder) {
        int bw = std::max(85, static_cast<int>(win->title.length() * 6.5f));
        buttonWidths.push_back(bw);
        totalButtonsWidth += bw + 6;
    }

    // Draw scroll arrows if needed
    const int arrowWidth = 16;
    const int arrowGap = 4;
    const int arrowReserve = (arrowWidth + arrowGap) * 2;
    m_taskbarNeedsScroll = totalButtonsWidth > m_taskbarButtonAreaWidth
        && m_taskbarButtonAreaWidth >= arrowReserve;
    int buttonsStartX = m_taskbarButtonAreaLeft;

    if (m_taskbarNeedsScroll) {
        // Left arrow
        SDL_SetRenderDrawColor(renderer, 80, 80, 90, 255);
        m_taskbarLeftArrowRect = {
            logicalToScreenX(m_taskbarButtonAreaLeft),
            btnY,
            static_cast<int>(arrowWidth * m_contentScale),
            btnHeight
        };
        SDL_RenderFillRect(renderer, &m_taskbarLeftArrowRect);
        // Simple left triangle using lines
        SDL_SetRenderDrawColor(renderer, 220, 220, 225, 255);
        int ax = logicalToScreenX(m_taskbarButtonAreaLeft + 4);
        int ay = btnY + btnHeight/2;
        SDL_RenderDrawLine(renderer, ax + 6, ay - 5, ax, ay);
        SDL_RenderDrawLine(renderer, ax, ay, ax + 6, ay + 5);

        buttonsStartX += arrowWidth + arrowGap;

        // Right arrow
        const int rightX = m_taskbarButtonAreaLeft + m_taskbarButtonAreaWidth - arrowWidth;
        SDL_SetRenderDrawColor(renderer, 80, 80, 90, 255);
        m_taskbarRightArrowRect = {
            logicalToScreenX(rightX),
            btnY,
            static_cast<int>(arrowWidth * m_contentScale),
            btnHeight
        };
        SDL_RenderFillRect(renderer, &m_taskbarRightArrowRect);
        SDL_SetRenderDrawColor(renderer, 220, 220, 225, 255);
        ax = logicalToScreenX(rightX + 4);
        SDL_RenderDrawLine(renderer, ax, ay - 5, ax + 6, ay);
        SDL_RenderDrawLine(renderer, ax + 6, ay, ax, ay + 5);

        m_taskbarButtonAreaWidth -= arrowReserve;
    } else {
        // A viewport too small for both arrows cannot be scrolled. Resetting
        // here prevents a prior wide-desktop scroll offset from hiding the
        // only partial button after the desktop shrinks.
        m_taskbarScrollOffset = 0;
    }

    // Keep scrolling bounded by the actual button viewport. Without this
    // clamp, repeated wheel/arrow input can move every window button away from
    // the visible strip and leave the taskbar looking empty until manually
    // scrolled back.
    const int maxTaskbarScroll = std::max(0, totalButtonsWidth - m_taskbarButtonAreaWidth);
    m_taskbarScrollOffset = std::clamp(m_taskbarScrollOffset, 0, maxTaskbarScroll);

    // --- Middle: Window buttons (ALL windows, XP style) ---
    // Active window is always drawn leftmost (per user request)
    const int areaLeftScreen = logicalToScreenX(buttonsStartX);
    const int areaRightScreen = logicalToScreenX(
        buttonsStartX + m_taskbarButtonAreaWidth);
    int currentX = logicalToScreenX(buttonsStartX) - static_cast<int>(m_taskbarScrollOffset * m_contentScale);

    for (size_t i = 0; i < buttonOrder.size(); ++i) {
        Window* win = buttonOrder[i];
        int bw = buttonWidths[i];

        SDL_Rect btnRect = {
            currentX,
            btnY,
            static_cast<int>(bw * m_contentScale),
            btnHeight
        };

        // Only consider for hit testing if it would be at least partially visible in the button area
        bool visible = (btnRect.x + btnRect.w > areaLeftScreen) && (btnRect.x < areaRightScreen);

        // Determine button appearance (Windows XP-ish)
        bool isFocused = (win == m_focusedWindow && !win->minimized);
        bool isMin = win->minimized;

        Uint8 r = 65, g = 65, b = 72;
        if (isFocused) {
            r = 85; g = 95; b = 130;   // highlighted for active
        } else if (isMin) {
            r = 55; g = 55; b = 62;    // darker for minimized
        }

        SDL_SetRenderDrawColor(renderer, r, g, b, 255);
        SDL_RenderFillRect(renderer, &btnRect);

        // Subtle border
        SDL_SetRenderDrawColor(renderer, 100, 100, 108, 255);
        SDL_RenderDrawRect(renderer, &btnRect);

        // Title text (clipped)
        if (m_font) {
            SDL_Color textCol = isMin ? SDL_Color{170,170,175,255} : SDL_Color{235,235,240,255};
            SDL_Surface* s = TTF_RenderText_Blended(m_font, win->title.c_str(), textCol);
            if (s) {
                SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
                if (t) {
                    // Keep the label at native size. The button owns the clip
                    // so long titles cannot bleed into neighboring buttons.
                    SDL_RenderSetClipRect(renderer, &btnRect);
                    SDL_Rect textDst = {
                        btnRect.x + 5,
                        btnY + (btnHeight - s->h) / 2,
                        s->w, s->h
                    };
                    SDL_RenderCopy(renderer, t, nullptr, &textDst);
                    SDL_RenderSetClipRect(renderer, nullptr);
                    SDL_DestroyTexture(t);
                }
                SDL_FreeSurface(s);
            }
        }

        if (visible) {
            // Match hit testing to the portion that is actually visible in
            // the taskbar viewport. A narrow desktop may show only part of a
            // button, but input must never extend outside that visible area.
            SDL_Rect hitRect = btnRect;
            const int hitRight = std::min(btnRect.x + btnRect.w, areaRightScreen);
            hitRect.x = std::max(hitRect.x, areaLeftScreen);
            hitRect.w = std::max(0, hitRight - hitRect.x);
            if (hitRect.w > 0) {
                m_taskbarEntries.push_back({hitRect, win});
            }
        }

        currentX += static_cast<int>((bw + 6) * m_contentScale);
    }

    // --- Right: System clock (local time; texture rebuilt when the minute changes) ---
    m_clockHitRect = {0, 0, 0, 0};
    if (m_font) {
        std::time_t now = std::time(nullptr);
        std::tm localTm{};
        localtime_r(&now, &localTm);
        char timeBuf[32];
        if (m_desktopSettings.clock24Hour()) {
            std::snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", localTm.tm_hour, localTm.tm_min);
        } else {
            int hour12 = localTm.tm_hour % 12;
            if (hour12 == 0) {
                hour12 = 12;
            }
            const char* ampm = (localTm.tm_hour < 12) ? "AM" : "PM";
            std::snprintf(timeBuf, sizeof(timeBuf), "%d:%02d %s", hour12, localTm.tm_min, ampm);
        }
        const std::string timeStr(timeBuf);

        if (timeStr != m_clockText || !m_clockTexture) {
            if (m_clockTexture) {
                SDL_DestroyTexture(m_clockTexture);
                m_clockTexture = nullptr;
            }
            SDL_Color textCol = {225, 225, 230, 255};
            SDL_Surface* s = TTF_RenderText_Blended(m_font, timeStr.c_str(), textCol);
            if (s) {
                m_clockTexture = SDL_CreateTextureFromSurface(renderer, s);
                m_clockTexW = s->w;
                m_clockTexH = s->h;
                SDL_FreeSurface(s);
                m_clockText = timeStr;
            }
        }

        char dateBuf[64];
        std::strftime(dateBuf, sizeof(dateBuf), "%a %b %d, %Y", &localTm);
        const std::string dateStr(dateBuf);
        if (dateStr != m_clockDateText || !m_clockDateTexture) {
            if (m_clockDateTexture) {
                SDL_DestroyTexture(m_clockDateTexture);
                m_clockDateTexture = nullptr;
            }
            SDL_Color dateCol = {235, 235, 240, 255};
            SDL_Surface* s = TTF_RenderText_Blended(m_font, dateStr.c_str(), dateCol);
            if (s) {
                m_clockDateTexture = SDL_CreateTextureFromSurface(renderer, s);
                m_clockDateTexW = s->w;
                m_clockDateTexH = s->h;
                SDL_FreeSurface(s);
                m_clockDateText = dateStr;
            }
        }

        if (m_clockTexture) {
            const int padX = static_cast<int>(8 * m_contentScale);
            const int trayW = m_clockTexW + padX * 2;
            const int trayH = btnHeight;
            const int trayX = logicalToScreenX(m_logicalWidth) - trayW - static_cast<int>(6 * m_contentScale);
            const int trayY = btnY;

            const int buttonAreaRight = logicalToScreenX(
                buttonsStartX + m_taskbarButtonAreaWidth);
            if (trayX >= buttonAreaRight) {
                SDL_SetRenderDrawColor(renderer, 45, 45, 52, 255);
                SDL_Rect tray = {trayX, trayY, trayW, trayH};
                SDL_RenderFillRect(renderer, &tray);
                SDL_SetRenderDrawColor(renderer, 100, 100, 108, 255);
                SDL_RenderDrawRect(renderer, &tray);
