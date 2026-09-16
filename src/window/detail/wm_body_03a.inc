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
        // Layout: top-level apps, then a Games category (Snake / Minesweeper), then Shut Down.
        // action: >=0 launch/quit, -1 category header (not clickable), -2 separator (not clickable).
        struct MenuEntry {
            const char* label;
            int action;
            int indent;  // 0 top-level, 1 nested under a category
        };
        #include "wm_start_menu_entries.inc"
        const size_t entryCount = sizeof(entries) / sizeof(entries[0]);

        const int itemHLogical = 26;
        const int itemGapLogical = 2;
        const int categoryHLogical = 20;
        const int separatorHLogical = 10;
        const int headerHLogical = 22;
        const int topPadLogical = 6;
        const int bottomPadLogical = 8;

        int contentHLogical = topPadLogical;
        for (size_t i = 0; i < entryCount; ++i) {
            if (entries[i].action == -2) {
                contentHLogical += separatorHLogical;
            } else if (entries[i].action == -1) {
                contentHLogical += categoryHLogical + itemGapLogical;
            } else {
                contentHLogical += itemHLogical + itemGapLogical;
            }
        }
        contentHLogical += bottomPadLogical;
        const StartMenuLayout layout = computeStartMenuLayout(contentHLogical);
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
        }
