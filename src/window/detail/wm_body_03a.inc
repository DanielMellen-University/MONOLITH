        // Maximize (green) - draw white square outline
        SDL_SetRenderDrawColor(renderer, 60, 170, 80, 255);
        SDL_RenderFillRect(renderer, &maxBtn);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        SDL_Rect maxSymbol = {maxBtn.x + 3, maxBtn.y + 3, buttonSize - 7, buttonSize - 7};
        SDL_RenderDrawRect(renderer, &maxSymbol);

        // Minimize (yellow/orange) - draw white horizontal line
        SDL_SetRenderDrawColor(renderer, 200, 170, 60, 255);
        SDL_RenderFillRect(renderer, &minBtn);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        int minLineY = minBtn.y + buttonSize / 2;
        SDL_RenderDrawLine(renderer, minBtn.x + 3, minLineY, minBtn.x + buttonSize - 4, minLineY);

        // === Window border (drawn in screen coordinates) ===
        SDL_Rect screenRect = {screenX, screenY, scaledW, scaledH};
        SDL_SetRenderDrawColor(renderer, 30, 30, 35, 255);
        SDL_RenderDrawRect(renderer, &screenRect);
    }

    // === Start Menu popup ===
    m_startMenuItems.clear();
    m_startMenuHoverIndex = -1;
    m_startMenuRect = {0, 0, 0, 0};

    if (m_showStartMenu) {
        // Layout comes from AppRegistry (apps, Games category, Shut Down).
        // action: >=0 launch/quit, -1 category header (not clickable), -2 separator.
        // Filtered by m_startMenuFilter when type-ahead is active.
        const int headerHLogical = m_startMenuFilter.empty() ? 22 : 36;
        #include "wm_start_menu_rows.inc"
        StartMenuLayout layout = computeStartMenuLayout(contentHLogical);
        // Keep layout chrome height in sync with the drawn filter header.
        if (layout.headerHLogical != headerHLogical) {
            const int delta = headerHLogical - layout.headerHLogical;
            layout.headerHLogical = headerHLogical;
            layout.menuRect.y -= static_cast<int>(delta * m_contentScale);
            layout.menuRect.h += static_cast<int>(delta * m_contentScale);
            layout.itemStartY = layout.menuRect.y + static_cast<int>(
                (layout.headerHLogical + layout.topPadLogical) * m_contentScale);
        }
        const int menuWidth = layout.menuWidthLogical;
        const int menuX = layout.menuRect.x;
        const int menuY = layout.menuRect.y;

        // Menu background (slightly more XP-like dark panel)
        const SDL_Rect menuRect = layout.menuRect;
        m_startMenuRect = menuRect;   // save for click hit-testing (popup is above taskbar)
        const SDL_Rect effectiveMenuClip = intersectRendererClip(menuRect, callerClip);
        const bool menuHasRenderArea = menuRect.w > 0 && menuRect.h > 0
            && effectiveMenuClip.w > 0 && effectiveMenuClip.h > 0;
        if (menuHasRenderArea) {
            SDL_RenderSetClipRect(renderer, &effectiveMenuClip);
            SDL_SetRenderDrawColor(renderer, 38, 38, 48, 255);
        SDL_RenderFillRect(renderer, &menuRect);
        SDL_SetRenderDrawColor(renderer, 85, 85, 95, 255);
        SDL_RenderDrawRect(renderer, &menuRect);

        // Top accent bar (subtle header area)
        SDL_SetRenderDrawColor(renderer, 55, 85, 140, 255);
        SDL_Rect headerAccent = {
            menuX, menuY,
            static_cast<int>(menuWidth * m_contentScale),
            static_cast<int>(headerHLogical * m_contentScale)
        };
        SDL_RenderFillRect(renderer, &headerAccent);

        if (m_font) {
            // Small "Monolith" label in the accent area
            SDL_Color accentText = {255, 255, 255, 255};
            int headerW = 0;
            int headerH = 0;
            SDL_Texture* headerTexture = getShellTextTexture(
                renderer, "Monolith", accentText, headerW, headerH);
            if (headerTexture) {
                SDL_Rect dst = {
                    menuX + static_cast<int>(10 * m_contentScale),
                    menuY + static_cast<int>(4 * m_contentScale),
                    headerW, headerH
                };
                SDL_RenderCopy(renderer, headerTexture, nullptr, &dst);
            }
            // Active type-ahead filter (muted) so the user sees what they typed.
            if (!m_startMenuFilter.empty()) {
                SDL_Color filterCol = {190, 200, 220, 255};
                int filterW = 0;
                int filterH = 0;
                SDL_Texture* filterTexture = getShellTextTexture(
                    renderer, m_startMenuFilter.c_str(), filterCol, filterW, filterH);
                if (filterTexture) {
                    SDL_Rect dst = {
                        menuX + static_cast<int>(10 * m_contentScale),
                        menuY + static_cast<int>(20 * m_contentScale),
                        filterW, filterH
                    };
                    SDL_RenderCopy(renderer, filterTexture, nullptr, &dst);
                }
            }
        }
