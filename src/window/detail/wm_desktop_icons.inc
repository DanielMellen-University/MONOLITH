void WindowManager::clearDesktopIconSelection() {
    m_desktopIconSelected = -1;
    m_desktopIconLastClickIndex = -1;
    m_desktopIconLastClickTicks = 0;
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

        if (m_font && icon.glyph && icon.glyph[0] != '\0') {
            SDL_Color glyphCol = {245, 245, 250, 255};
            SDL_Surface* gs = TTF_RenderText_Blended(m_font, icon.glyph, glyphCol);
            if (gs) {
                SDL_Texture* gt = SDL_CreateTextureFromSurface(renderer, gs);
                if (gt) {
                    SDL_Rect dst = {
                        tileScreen.x + (tileScreen.w - gs->w) / 2,
                        tileScreen.y + (tileScreen.h - gs->h) / 2,
                        gs->w,
                        gs->h
                    };
                    SDL_RenderCopy(renderer, gt, nullptr, &dst);
                    SDL_DestroyTexture(gt);
                }
                SDL_FreeSurface(gs);
            }
        }

        if (m_font && icon.label) {
            SDL_Color labelCol = selected
                ? SDL_Color{255, 255, 255, 255}
                : SDL_Color{220, 220, 230, 255};
            SDL_Surface* ls = TTF_RenderText_Blended(m_font, icon.label, labelCol);
            if (ls) {
                SDL_Texture* lt = SDL_CreateTextureFromSurface(renderer, ls);
                if (lt) {
                    const int labelY = logicalToScreenY(
                        icon.tile.y + icon.tile.h + 2);
                    SDL_Rect dst = {
                        tileScreen.x + (tileScreen.w - ls->w) / 2,
                        labelY,
                        ls->w,
                        ls->h
                    };
                    SDL_RenderCopy(renderer, lt, nullptr, &dst);
                    SDL_DestroyTexture(lt);
                }
                SDL_FreeSurface(ls);
            }
        }
    }
}
