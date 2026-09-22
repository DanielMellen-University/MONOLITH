// AppRegistry-backed Start menu hit rebuild.
// Replaces the former hardcoded MenuEntry table. Included from
// ensureStartMenuHitTargets after a local MenuEntry struct declaration.
// Does the real rebuild from filtered AppRegistry rows, then returns. A tiny
// dummy entries array keeps the unreachable caller-side sizeof/loop valid.
{
    #include "wm_start_menu_rows.inc"
    const StartMenuLayout layout = computeStartMenuLayout(contentHLogical);
    m_startMenuRect = layout.menuRect;
    m_startMenuItems.clear();
    m_startMenuHoverIndex = -1;

    int y = layout.itemStartY;
    for (size_t i = 0; i < entryCount; ++i) {
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
}
return;
const MenuEntry entries[] = {
    {"", -2, 0},
};
