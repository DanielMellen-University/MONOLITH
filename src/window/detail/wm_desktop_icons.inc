void WindowManager::clearDesktopIconSelection() {
    m_desktopIconSelected = -1;
    m_desktopIconLastClickIndex = -1;
    m_desktopIconLastClickTicks = 0;
}

void WindowManager::destroyDesktopIconTextCache() {
    for (auto& entry : m_desktopIconTextCache) {
        if (entry.glyphTexture) SDL_DestroyTexture(entry.glyphTexture);
        if (entry.labelTexture) SDL_DestroyTexture(entry.labelTexture);
        if (entry.selectedLabelTexture) SDL_DestroyTexture(entry.selectedLabelTexture);
        entry = {};
    }
}

void WindowManager::activateDesktopIcon(DesktopIconAction action) {
    switch (action) {
        case DesktopIconAction::Terminal: launchTerminal(); break;
        case DesktopIconAction::TextEditor: launchTextEditor(); break;
        case DesktopIconAction::Filesystem: launchFilesystem(); break;
        case DesktopIconAction::Settings: launchSettings(); break;
        case DesktopIconAction::Drawing: launchDrawing(); break;
        default: break;
    }
    clearDesktopIconSelection();
}

bool WindowManager::tryHandleDesktopIconClick(int logicalX, int logicalY) {
    if (m_showStartMenu) return false;

    const SDL_Rect usable = getUsableDesktopRect();
    const auto icons = layoutDesktopIcons(usable.w, usable.h);
    const int hit = hitTestDesktopIcon(icons, logicalX, logicalY);
    const Uint32 now = SDL_GetTicks();

    if (hit < 0) {
        clearDesktopIconSelection();
        return false;
    }

    const bool dbl = isDesktopIconDoubleClick(
        m_desktopIconLastClickIndex, m_desktopIconLastClickTicks, hit, now);
    m_desktopIconSelected = hit;
    m_desktopIconLastClickIndex = hit;
    m_desktopIconLastClickTicks = now;

    if (dbl) {
        activateDesktopIcon(icons[static_cast<size_t>(hit)].action);
    }
    return true;
}

void WindowManager::renderDesktopIcons(SDL_Renderer* renderer) {
    if (!renderer) return;

    const SDL_Rect usable = getUsableDesktopRect();
    const auto icons = layoutDesktopIcons(usable.w, usable.h);
    if (icons.empty()) return;

    for (int i = 0; i < static_cast<int>(icons.size()); ++i) {
        const DesktopIconPlacement& icon = icons[static_cast<size_t>(i)];
        const bool selected = (i == m_desktopIconSelected);

        const SDL_Rect tileScreen = logicalRectToScreen(icon.tile);
        if (selected) {
            SDL_Rect sel = logicalRectToScreen(icon.rect);
            sel.x -= static_cast<int>(2 * m_contentScale);
            sel.y -= static_cast<int>(2 * m_contentScale);
            sel.w += static_cast<int>(4 * m_contentScale);
            sel.h += static_cast<int>(4 * m_contentScale);
            SDL_SetRenderDrawColor(renderer, 70, 100, 160, 180);
            SDL_RenderFillRect(renderer, &sel);
        }

        SDL_SetRenderDrawColor(renderer, icon.r, icon.g, icon.b, 255);
        SDL_RenderFillRect(renderer, &tileScreen);
        SDL_SetRenderDrawColor(renderer, 220, 220, 230, 255);
        SDL_RenderDrawRect(renderer, &tileScreen);

        if (!m_font || i >= kDesktopIconCount) continue;

        auto& cache = m_desktopIconTextCache[static_cast<size_t>(i)];
        if (!cache.glyphTexture && icon.glyph && icon.glyph[0] != '\0') {
            SDL_Color glyphCol = {245, 245, 250, 255};
            SDL_Surface* gs = TTF_RenderText_Blended(m_font, icon.glyph, glyphCol);
            if (gs) {
                cache.glyphTexture = SDL_CreateTextureFromSurface(renderer, gs);
                if (cache.glyphTexture) {
                    cache.glyphW = gs->w;
                    cache.glyphH = gs->h;
                }
                SDL_FreeSurface(gs);
            }
        }
        if (cache.glyphTexture) {
            SDL_Rect dst = {
                tileScreen.x + (tileScreen.w - cache.glyphW) / 2,
                tileScreen.y + (tileScreen.h - cache.glyphH) / 2,
                cache.glyphW,
                cache.glyphH
            };
            SDL_RenderCopy(renderer, cache.glyphTexture, nullptr, &dst);
        }

        if (icon.label) {
            if (!cache.labelTexture) {
                SDL_Color labelCol = {220, 220, 230, 255};
                SDL_Surface* ls = TTF_RenderText_Blended(m_font, icon.label, labelCol);
                if (ls) {
                    cache.labelTexture = SDL_CreateTextureFromSurface(renderer, ls);
                    if (cache.labelTexture) {
                        cache.labelW = ls->w;
                        cache.labelH = ls->h;
                    }
                    SDL_FreeSurface(ls);
                }
            }
            if (!cache.selectedLabelTexture) {
                SDL_Color selectedLabelCol = {255, 255, 255, 255};
                SDL_Surface* ls = TTF_RenderText_Blended(
                    m_font, icon.label, selectedLabelCol);
                if (ls) {
                    cache.selectedLabelTexture = SDL_CreateTextureFromSurface(renderer, ls);
                    if (cache.selectedLabelTexture) {
                        cache.selectedLabelW = ls->w;
                        cache.selectedLabelH = ls->h;
                    }
                    SDL_FreeSurface(ls);
                }
            }

            SDL_Texture* labelTexture = selected
                ? cache.selectedLabelTexture : cache.labelTexture;
            const int labelW = selected ? cache.selectedLabelW : cache.labelW;
            const int labelH = selected ? cache.selectedLabelH : cache.labelH;
            if (labelTexture) {
                const int labelY = logicalToScreenY(
                    icon.tile.y + icon.tile.h + 2);
                SDL_Rect dst = {
                    tileScreen.x + (tileScreen.w - labelW) / 2,
                    labelY,
                    labelW,
                    labelH
                };
                SDL_RenderCopy(renderer, labelTexture, nullptr, &dst);
            }
        }
    }
}
