#pragma once

#include <SDL2/SDL.h>
#include <algorithm>
#include <cstddef>
#include <vector>

namespace monolith::window {

// Start-menu action ids reused by desktop icons (Games stay in Start only).
enum class DesktopIconAction : int {
    Terminal = 0,
    TextEditor = 1,
    Filesystem = 2,
    Settings = 3,
    Drawing = 4,
};

struct DesktopIconDef {
    const char* label;
    const char* glyph; // single letter drawn in the icon tile
    DesktopIconAction action;
    Uint8 r;
    Uint8 g;
    Uint8 b;
};

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

inline constexpr DesktopIconDef kDefaultDesktopIcons[] = {
    {"Terminal", "T", DesktopIconAction::Terminal, 55, 85, 140},
    {"Filesystem", "F", DesktopIconAction::Filesystem, 70, 120, 90},
    {"Editor", "E", DesktopIconAction::TextEditor, 120, 95, 70},
    {"Drawing", "D", DesktopIconAction::Drawing, 140, 80, 110},
    {"Settings", "S", DesktopIconAction::Settings, 90, 90, 110},
};

inline constexpr int kDesktopIconCount =
    static_cast<int>(sizeof(kDefaultDesktopIcons) / sizeof(kDefaultDesktopIcons[0]));

inline constexpr int kDesktopIconTile = 48;
inline constexpr int kDesktopIconLabelBand = 22;
inline constexpr int kDesktopIconGap = 14;
inline constexpr int kDesktopIconMargin = 16;
inline constexpr int kDesktopIconCellH =
    kDesktopIconTile + kDesktopIconLabelBand + kDesktopIconGap;
inline constexpr Uint32 kDesktopIconDoubleClickMs = 450;

// Lay out the default icon column inside the usable desktop (above the taskbar).
// Icons that cannot fit are omitted rather than overlapping the taskbar.
inline std::vector<DesktopIconPlacement> layoutDesktopIcons(
    int usableWidth, int usableHeight) {
    std::vector<DesktopIconPlacement> out;
    if (usableWidth <= 0 || usableHeight <= 0) return out;

    const int cellW = kDesktopIconTile + 8;
    const int iconH = kDesktopIconTile + kDesktopIconLabelBand;
    const int remainingAfterFirst = usableHeight - kDesktopIconMargin - iconH;
    const int maxRows = remainingAfterFirst < 0
        ? 0
        : 1 + remainingAfterFirst / kDesktopIconCellH;
    const int count = std::min(kDesktopIconCount, maxRows);
    out.reserve(static_cast<size_t>(count));

    for (int i = 0; i < count; ++i) {
        const DesktopIconDef& def = kDefaultDesktopIcons[i];
        const int x = kDesktopIconMargin;
        const int y = kDesktopIconMargin + i * kDesktopIconCellH;
        if (x + cellW > usableWidth) break;
        if (y + kDesktopIconTile + kDesktopIconLabelBand > usableHeight) break;

        DesktopIconPlacement p;
        p.tile = {x, y, kDesktopIconTile, kDesktopIconTile};
        p.rect = {x, y, cellW, kDesktopIconTile + kDesktopIconLabelBand};
        p.label = def.label;
        p.glyph = def.glyph;
        p.action = def.action;
        p.r = def.r;
        p.g = def.g;
        p.b = def.b;
        out.push_back(p);
    }
    return out;
}

inline int hitTestDesktopIcon(
    const std::vector<DesktopIconPlacement>& icons, int logicalX, int logicalY) {
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
