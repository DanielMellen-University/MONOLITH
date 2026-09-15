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

    for (const auto& icon : tall) {
        check(icon.rect.y + icon.rect.h <= 720 - 28,
              "each icon stays inside the usable desktop height");
        check(icon.tile.w == kDesktopIconTile && icon.tile.h == kDesktopIconTile,
              "icon tiles use the shared size");
    }

    check(hitTestDesktopIcon(tall, tall[0].rect.x + 4, tall[0].rect.y + 4) == 0,
          "hit testing finds the first icon");
    check(hitTestDesktopIcon(tall, 400, 40) == -1,
          "empty desktop misses icons");

    const auto shortDesk = layoutDesktopIcons(200, 90);
    check(static_cast<int>(shortDesk.size()) < kDesktopIconCount,
          "short desktops omit icons that would overlap the taskbar");
    for (const auto& icon : shortDesk) {
        check(icon.rect.y + icon.rect.h <= 90,
              "short-desktop icons never extend past usable height");
    }

    check(isDesktopIconDoubleClick(2, 1000, 2, 1300),
          "same-icon click within the window counts as a double-click");
    check(!isDesktopIconDoubleClick(2, 1000, 2, 1600),
          "late second click is not a double-click");
    check(!isDesktopIconDoubleClick(1, 1000, 2, 1100),
          "different icons are not a double-click");

    if (failures == 0) {
        std::cout << "ALL DESKTOP ICON TESTS PASSED\n";
        return 0;
    }
    return 1;
}
