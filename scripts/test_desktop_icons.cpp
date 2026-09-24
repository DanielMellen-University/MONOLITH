// Headless layout/hit tests for desktop icons (no WindowManager / display).
#include "../src/window/DesktopIcons.hpp"

#include <iostream>

int main() {
    using namespace monolith::window;
    int failures = 0;
    auto check = [&](bool ok, const char* message) {
        if (!ok) {
            std::cerr << "FAIL: " << message << '\n';
            ++failures;
        } else {
            std::cout << "ok: " << message << '\n';
        }
    };

    const auto tall = layoutDesktopIcons(1280, 720 - 28);
    check(static_cast<int>(tall.size()) == kDesktopIconCount,
          "default desktop fits all five icons above the taskbar");
    check(tall.front().action == DesktopIconAction::Terminal,
          "first icon is Terminal");
    check(tall.back().action == DesktopIconAction::Settings,
          "last icon is Settings");
    check(tall.front().rect.w == kDesktopIconCellW,
          "desktop icon cells reserve a stable label-width hit area");
    check(tall.front().rect.x == kDesktopIconMargin,
          "first column starts at the left margin");

    for (const auto& icon : tall) {
        check(icon.rect.y + icon.rect.h <= 720 - 28,
              "each icon stays inside the usable desktop height");
        check(icon.rect.x + icon.rect.w <= 1280,
              "each icon stays inside the usable desktop width");
        check(icon.tile.w == kDesktopIconTile && icon.tile.h == kDesktopIconTile,
              "icon tiles use the shared size");
    }

    check(hitTestDesktopIcon(tall, tall[0].rect.x + 4, tall[0].rect.y + 4) == 0,
          "hit testing finds the first icon");
    check(hitTestDesktopIcon(tall, 400, 40) == -1,
          "empty desktop misses icons");

    // Height that fits exactly two rows (margin + row0 + gap + row1).
    // Three columns are needed so five registry icons remain reachable.
    const int twoRowHeight = kDesktopIconMargin
        + kDesktopIconTile + kDesktopIconLabelBand
        + kDesktopIconGap
        + kDesktopIconTile + kDesktopIconLabelBand;
    const int threeColWidth = kDesktopIconMargin
        + kDesktopIconCellW
        + 2 * kDesktopIconColStep;
    const auto shortTall = layoutDesktopIcons(threeColWidth, twoRowHeight);
    check(static_cast<int>(shortTall.size()) == kDesktopIconCount,
          "short usable height places later icons in further columns");
    check(shortTall.size() >= 3, "short desk still places at least three icons");
    if (shortTall.size() >= 3) {
        check(shortTall[0].rect.x == kDesktopIconMargin
                  && shortTall[1].rect.x == kDesktopIconMargin,
              "first column fills top-to-bottom before advancing");
        check(shortTall[2].rect.x
                  == kDesktopIconMargin + kDesktopIconColStep,
              "third icon starts column 2 when only two rows fit");
        check(shortTall[2].rect.y == kDesktopIconMargin,
              "column 2 starts at the top row");
    }
    if (shortTall.size() >= 5) {
        check(shortTall[4].rect.x
                  == kDesktopIconMargin + 2 * kDesktopIconColStep,
              "fifth icon reaches column 3 on a short desk");
    }
    for (size_t i = 1; i < shortTall.size(); ++i) {
        check(shortTall[i].rect.x >= shortTall[i - 1].rect.x,
              "columns advance left-to-right without going backwards");
    }
    for (size_t a = 0; a < shortTall.size(); ++a) {
        for (size_t b = a + 1; b < shortTall.size(); ++b) {
            const SDL_Rect& ra = shortTall[a].rect;
            const SDL_Rect& rb = shortTall[b].rect;
            const bool overlap = !(ra.x + ra.w <= rb.x || rb.x + rb.w <= ra.x
                || ra.y + ra.h <= rb.y || rb.y + rb.h <= ra.y);
            check(!overlap, "multi-column placements do not overlap");
        }
    }

    const auto shortDesk = layoutDesktopIcons(200, 90);
    check(static_cast<int>(shortDesk.size()) < kDesktopIconCount,
          "short desktops omit icons that would overlap the taskbar");
    for (const auto& icon : shortDesk) {
        check(icon.rect.y + icon.rect.h <= 90,
              "short-desktop icons never extend past usable height");
        check(icon.rect.x + icon.rect.w <= 200,
              "short-desktop icons never extend past usable width");
    }

    const int oneIconHeight = kDesktopIconMargin
        + kDesktopIconTile + kDesktopIconLabelBand;
    check(layoutDesktopIcons(200, oneIconHeight).size() == 1,
          "desktop renders one icon when the first row exactly fits");
    check(layoutDesktopIcons(200, oneIconHeight - 1).empty(),
          "desktop omits the first icon when its row is one pixel too tall");
    check(layoutDesktopIcons(kDesktopIconCellW - 1, oneIconHeight).empty(),
          "narrow desktops omit icons whose labels cannot fit horizontally");

    // Narrow width: one column only; remaining icons omitted, not overlapped.
    const int oneColWidth = kDesktopIconMargin + kDesktopIconCellW;
    const auto narrow = layoutDesktopIcons(oneColWidth, twoRowHeight);
    check(static_cast<int>(narrow.size()) == 2,
          "narrow width keeps a single column and omits overflow icons");
    for (const auto& icon : narrow) {
        check(icon.rect.x == kDesktopIconMargin,
              "narrow desk keeps every placed icon in column 1");
        check(icon.rect.x + icon.rect.w <= oneColWidth,
              "narrow desk never clips past the right edge");
    }
    check(layoutDesktopIcons(oneColWidth + kDesktopIconGap, twoRowHeight).size() == 2,
          "partial second column width still omits rather than overlapping");

    check(isDesktopIconDoubleClick(2, 1000, 2, 1300),
          "same-icon click within the window counts as a double-click");
    check(!isDesktopIconDoubleClick(2, 1000, 2, 1600),
          "late second click is not a double-click");
    check(!isDesktopIconDoubleClick(1, 1000, 2, 1100),
          "different icons are not a double-click");
    check(isDesktopIconDoubleClick(2, 0xfffffff0u, 2, 0x00000050u),
          "double-click timing survives SDL tick wraparound");

    if (failures == 0) {
        std::cout << "ALL DESKTOP ICON TESTS PASSED\n";
        return 0;
    }
    return 1;
}
