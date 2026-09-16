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
                    static_cast<int>(std::max(0, menuWidth - 8) * m_contentScale),
                    itemH
                };
                SDL_Rect visibleItemRect{};
                const bool visibleItem = SDL_IntersectRect(
                    &itemRect, &menuRect, &visibleItemRect);
                const int itemIndex = static_cast<int>(m_startMenuItems.size());
                if (visibleItem && visibleItemRect.w > 0 && visibleItemRect.h > 0) {
                    m_startMenuItems.push_back({visibleItemRect, e.action});
                }

                bool hovered = false;
                SDL_Point mousePt = {m_mouseX, m_mouseY};
                if (visibleItem && SDL_PointInRect(&mousePt, &visibleItemRect)) {
                    hovered = true;
                    m_startMenuHoverIndex = static_cast<int>(m_startMenuItems.size() - 1);
                }
                const bool keyboardSelected = visibleItem
                    && itemIndex == m_startMenuKeyboardIndex;

                if (hovered || keyboardSelected) {
                    SDL_SetRenderDrawColor(renderer, hoverBgCol.r, hoverBgCol.g, hoverBgCol.b, 255);
                    SDL_RenderFillRect(renderer, &itemRect);
                }

                SDL_Color useTextCol = (hovered || keyboardSelected) ? hoverTextCol : textCol;
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
        restoreRendererClip(renderer, callerClip);
        }
    }

    // === Taskbar (always visible) ===
    m_taskbarEntries.clear();
    m_taskbarLeftArrowRect = {0, 0, 0, 0};
    m_taskbarRightArrowRect = {0, 0, 0, 0};
