    // Rebuild hit targets from the same filtered registry rows used by rendering.
    #include "wm_start_menu_rows.inc"
    const int headerHLogical = m_startMenuFilter.empty() ? 22 : 36;
    StartMenuLayout layout = computeStartMenuLayout(contentHLogical);
    if (layout.headerHLogical != headerHLogical) {
        const int delta = headerHLogical - layout.headerHLogical;
        layout.headerHLogical = headerHLogical;
        layout.menuRect.y -= static_cast<int>(delta * m_contentScale);
        layout.menuRect.h += static_cast<int>(delta * m_contentScale);
        layout.itemStartY = layout.menuRect.y + static_cast<int>(
            (layout.headerHLogical + layout.topPadLogical) * m_contentScale);
    }
    m_startMenuRect = layout.menuRect;
    m_startMenuItems.clear();
    m_startMenuHoverIndex = -1;

    int y = layout.itemStartY;
    for (std::size_t i = 0; i < entryCount; ++i) {
        const auto& entry = entries[i];
        if (entry.action == -2) {
            y += layout.separatorH;
            continue;
        }
        if (entry.action == -1) {
            y += layout.categoryH + layout.itemGap;
            continue;
        }

        const SDL_Rect itemRect = {
            layout.menuRect.x + static_cast<int>(4 * m_contentScale),
            y,
            static_cast<int>(std::max(0, layout.menuWidthLogical - 8) * m_contentScale),
            layout.itemH
        };
        SDL_Rect visibleItemRect{};
        if (SDL_IntersectRect(&itemRect, &m_startMenuRect, &visibleItemRect)
            && visibleItemRect.w > 0 && visibleItemRect.h > 0) {
            m_startMenuItems.push_back({visibleItemRect, entry.action});
        }
        y += layout.itemH + layout.itemGap;
    }
