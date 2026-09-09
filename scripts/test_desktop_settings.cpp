// Headless test of DesktopSettings persistence, including the optional UI scale key.

#include "../src/settings/DesktopSettings.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>

using monolith::settings::DesktopSettings;

int main() {
    int failures = 0;
    auto check = [&](bool ok, const char* msg) {
        if (!ok) {
            std::cerr << "FAIL: " << msg << '\n';
            ++failures;
        } else {
            std::cout << "ok: " << msg << '\n';
        }
    };

    const std::filesystem::path path =
        std::filesystem::temp_directory_path() / "monolith-desktop-settings-roadmap.txt";
    std::error_code ec;
    std::filesystem::remove(path, ec);

    DesktopSettings saved;
    saved.setDesktopBackground({18, 24, 42});
    saved.setWallpaperPath("/Wallpapers/sample.bmp");
    saved.setClock24Hour(true);
    saved.setUiScalePercent(115);
    check(saved.saveToHostPath(path.string()), "save settings with UI scale");

    DesktopSettings loaded;
    check(loaded.loadFromHostPath(path.string()), "load settings file");
    const auto background = loaded.desktopBackground();
    check(background.r == 18 && background.g == 24 && background.b == 42,
          "background round-trip");
    check(loaded.wallpaperPath() == "/Wallpapers/sample.bmp", "wallpaper round-trip");
    check(loaded.clock24Hour(), "clock format round-trip");
    check(loaded.uiScalePercent() == 115, "UI scale round-trip");

    {
        std::ofstream legacy(path, std::ios::trunc);
        legacy << "desktop_background=25,25,30\n"
               << "wallpaper_path=\n"
               << "clock_24_hour=0\n";
    }
    DesktopSettings older;
    check(older.loadFromHostPath(path.string()), "load legacy settings file");
    check(older.uiScalePercent() == 100, "legacy settings default to 100 percent");

    {
        std::ofstream invalid(path, std::ios::trunc);
        invalid << "ui_scale_percent=200\n";
    }
    DesktopSettings bounded;
    check(!bounded.loadFromHostPath(path.string()), "ignore invalid-only scale file");
    check(bounded.uiScalePercent() == 100, "out-of-range UI scale is ignored");

    std::filesystem::remove(path, ec);
    if (failures == 0) {
        std::cout << "ALL DESKTOP SETTINGS TESTS PASSED\n";
        return 0;
    }
    std::cerr << failures << " test(s) failed\n";
    return 1;
}
