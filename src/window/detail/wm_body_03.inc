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
        const MenuEntry entries[] = {
            {"Terminal", 0, 0},
            {"Text Editor", 1, 0},
            {"Filesystem", 2, 0},
            {"Drawing", 4, 0},
            {"Settings", 3, 0},
            {"", -2, 0},                 // separator before Games
            {"Games", -1, 0},            // category header
            {"Snake", 5, 1},
            {"Minesweeper", 6, 1},
            {"", -2, 0},                 // separator before Shut Down
            {"Shut Down", 7, 0},
        };
        const size_t entryCount = sizeof(entries) / sizeof(entries[0]);

        const int menuWidth = 210;
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
        const int menuHeight = headerHLogical + contentHLogical;

        const int menuX = logicalToScreenX(8);
        const int menuY = logicalToScreenY(getTaskbarRect().y - menuHeight);

        // Menu background (slightly more XP-like dark panel)
        SDL_SetRenderDrawColor(renderer, 38, 38, 48, 255);
        SDL_Rect menuRect = {
            menuX, menuY,
            static_cast<int>(menuWidth * m_contentScale),
            static_cast<int>(menuHeight * m_contentScale)
        };
        m_startMenuRect = menuRect;   // save for click hit-testing (popup is above taskbar)
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
            SDL_Surface* hdr = TTF_RenderText_Blended(m_font, "Monolith", accentText);
            if (hdr) {
                SDL_Texture* ht = SDL_CreateTextureFromSurface(renderer, hdr);
                if (ht) {
                    SDL_Rect dst = {
                        menuX + static_cast<int>(10 * m_contentScale),
                        menuY + static_cast<int>(4 * m_contentScale),
                        hdr->w, hdr->h
                    };
                    SDL_RenderCopy(renderer, ht, nullptr, &dst);
                    SDL_DestroyTexture(ht);
                }
                SDL_FreeSurface(hdr);
            }
        }

        if (m_font) {
            SDL_Color textCol = {235, 235, 240, 255};
            SDL_Color hoverTextCol = {255, 255, 255, 255};
            SDL_Color hoverBgCol = {70, 95, 145, 255};
            SDL_Color categoryCol = {160, 175, 200, 255};
            SDL_Color separatorCol = {70, 70, 82, 255};

            int y = menuY + static_cast<int>((headerHLogical + topPadLogical) * m_contentScale);
            const int itemH = static_cast<int>(itemHLogical * m_contentScale);
            const int categoryH = static_cast<int>(categoryHLogical * m_contentScale);
            const int separatorH = static_cast<int>(separatorHLogical * m_contentScale);
            const int itemGap = static_cast<int>(itemGapLogical * m_contentScale);
            const int basePad = static_cast<int>(10 * m_contentScale);
            const int nestPad = static_cast<int>(22 * m_contentScale);

            for (size_t i = 0; i < entryCount; ++i) {
                const auto& e = entries[i];

                if (e.action == -2) {
                    // Horizontal rule centered in the separator band
                    const int lineY = y + separatorH / 2;
                    SDL_SetRenderDrawColor(renderer, separatorCol.r, separatorCol.g, separatorCol.b, 255);
                    SDL_RenderDrawLine(
                        renderer,
                        menuX + static_cast<int>(10 * m_contentScale),
                        lineY,
                        menuX + static_cast<int>((menuWidth - 10) * m_contentScale),
                        lineY);
                    y += separatorH;
                    continue;
                }

                if (e.action == -1) {
                    // Non-clickable category header
                    SDL_Surface* s = TTF_RenderText_Blended(m_font, e.label, categoryCol);
                    if (s) {
                        SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
                        if (t) {
                            SDL_Rect dst = {
                                menuX + basePad,
                                y + (categoryH - s->h) / 2,
                                s->w, s->h
                            };
                            SDL_RenderCopy(renderer, t, nullptr, &dst);
                            SDL_DestroyTexture(t);
                        }
                        SDL_FreeSurface(s);
                    }
                    y += categoryH + itemGap;
                    continue;
                }

                // Clickable app / system row
                SDL_Rect itemRect = {
                    menuX + static_cast<int>(4 * m_contentScale),
                    y,
                    static_cast<int>((menuWidth - 8) * m_contentScale),
                    itemH
                };
                m_startMenuItems.push_back({itemRect, e.action});

                bool hovered = false;
                SDL_Point mousePt = {m_mouseX, m_mouseY};
                if (SDL_PointInRect(&mousePt, &itemRect)) {
                    hovered = true;
                    m_startMenuHoverIndex = static_cast<int>(m_startMenuItems.size() - 1);
                }

                if (hovered) {
                    SDL_SetRenderDrawColor(renderer, hoverBgCol.r, hoverBgCol.g, hoverBgCol.b, 255);
                    SDL_RenderFillRect(renderer, &itemRect);
                }

                SDL_Color useTextCol = hovered ? hoverTextCol : textCol;
                const int textPad = (e.indent > 0) ? nestPad : basePad;

                SDL_Surface* s = TTF_RenderText_Blended(m_font, e.label, useTextCol);
                if (s) {
                    SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
                    if (t) {
                        SDL_Rect dst = {menuX + textPad, y + (itemH - s->h) / 2, s->w, s->h};
                        SDL_RenderCopy(renderer, t, nullptr, &dst);
                        SDL_DestroyTexture(t);
                    }
                    SDL_FreeSurface(s);
                }

                y += itemH + itemGap;
            }
        }
    }

    // === Taskbar (always visible) ===
    m_taskbarEntries.clear();
    m_taskbarLeftArrowRect = {0, 0, 0, 0};
    m_taskbarRightArrowRect = {0, 0, 0, 0};

