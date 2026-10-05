#pragma once

#include "AppRegistry.hpp"

#include <SDL2/SDL.h>

#include <algorithm>
#include <array>
#include <cstddef>

namespace monolith::window {

// Desktop icons reuse AppAction ids from the shared registry.
using DesktopIconAction = AppAction;

struct DesktopIconPlacement {
    SDL_Rect rect; // full hit target (tile + label band), logical pixels
    SDL_Rect tile; // colored square only
    const char* label;
    const char* glyph;
    DesktopIconAction action;
    Uint8 r;
    Uint8 g;
    Uint8 b;
};

struct DesktopIconPlacements {
    std::array<DesktopIconPlacement, kDesktopIconCount> entries{};
    std::size_t count = 0;

    std::size_t size() const { return count; }
    bool empty() const { return count == 0; }
    const DesktopIconPlacement& front() const { return entries.front(); }
    const DesktopIconPlacement& back() const { return entries[count - 1]; }
    const DesktopIconPlacement& operator[](std::size_t index) const {
        return entries[index];
    }
    const DesktopIconPlacement* begin() const { return entries.data(); }
    const DesktopIconPlacement* end() const { return entries.data() + count; }
};

inline constexpr int kDesktopIconTile = 48;
inline constexpr int kDesktopIconCellW = 112;
inline constexpr int kDesktopIconLabelBand = 22;
inline constexpr int kDesktopIconGap = 14;
inline constexpr int kDesktopIconMargin = 16;
inline constexpr int kDesktopIconCellH =
    kDesktopIconTile + kDesktopIconLabelBand + kDesktopIconGap;
inline constexpr int kDesktopIconColStep =
    kDesktopIconCellW + kDesktopIconGap;
inline constexpr Uint32 kDesktopIconDoubleClickMs = 450;

// Lay out registry desktop icons in columns left-to-right (top-to-bottom
// within each column) inside the usable desktop above the taskbar.
// Icons that cannot fit entirely are omitted rather than overlapping the
// taskbar or clipping past the right edge.
inline DesktopIconPlacements layoutDesktopIcons(
    int usableWidth, int usableHeight) {
    DesktopIconPlacements out;
    if (usableWidth <= 0 || usableHeight <= 0) return out;

    const auto defs = collectDesktopIconDefs();
    const int iconH = kDesktopIconTile + kDesktopIconLabelBand;
    const int remainingAfterFirstRow = usableHeight - kDesktopIconMargin - iconH;
    const int maxRows = remainingAfterFirstRow < 0
        ? 0
        : 1 + remainingAfterFirstRow / kDesktopIconCellH;
    if (maxRows <= 0) return out;

    const int remainingAfterFirstCol =
        usableWidth - kDesktopIconMargin - kDesktopIconCellW;
    const int maxCols = remainingAfterFirstCol < 0
        ? 0
        : 1 + remainingAfterFirstCol / kDesktopIconColStep;
    if (maxCols <= 0) return out;

    const int capacity = maxRows * maxCols;
    const int count = std::min(static_cast<int>(defs.size()), capacity);

    for (int i = 0; i < count; ++i) {
        const int col = i / maxRows;
        const int row = i % maxRows;
        const int x = kDesktopIconMargin + col * kDesktopIconColStep;
        const int y = kDesktopIconMargin + row * kDesktopIconCellH;
        if (x + kDesktopIconCellW > usableWidth) break;
        if (y + iconH > usableHeight) break;

        const DesktopIconDef& def = defs[static_cast<size_t>(i)];
        DesktopIconPlacement p;
        p.tile = {x, y, kDesktopIconTile, kDesktopIconTile};
        p.rect = {x, y, kDesktopIconCellW, iconH};
        p.label = def.label;
        p.glyph = def.glyph;
        p.action = def.action;
        p.r = def.r;
        p.g = def.g;
        p.b = def.b;
        out.entries[out.count++] = p;
    }
    return out;
}

inline int hitTestDesktopIcon(
    const DesktopIconPlacements& icons, int logicalX, int logicalY) {
    SDL_Point p{logicalX, logicalY};
    for (int i = 0; i < static_cast<int>(icons.size()); ++i) {
        if (SDL_PointInRect(&p, &icons[static_cast<size_t>(i)].rect)) {
            return i;
        }
    }
    return -1;
}

inline bool isDesktopIconDoubleClick(
    int previousIndex, Uint32 previousTicks,
    int currentIndex, Uint32 currentTicks) {
    if (previousIndex < 0 || currentIndex < 0) return false;
    if (previousIndex != currentIndex) return false;
    return static_cast<Uint32>(currentTicks - previousTicks)
        <= kDesktopIconDoubleClickMs;
}

} // namespace monolith::window
