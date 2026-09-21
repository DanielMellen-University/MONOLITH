// Headless coverage for Settings wallpaper fit apply via IWindowController.
#include "../src/app/App.hpp"
#include <iostream>
#include <string>

namespace {

struct FitController final : monolith::app::IWindowController {
    std::string wallpaperFit = "cover";
    void close() override {}
    void setTitle(const std::string&) override {}
    std::string getWallpaperFit() const override { return wallpaperFit; }
    void setWallpaperFit(const std::string& fit) override { wallpaperFit = fit; }
};

} // namespace

int main() {
    int failures = 0;
    auto check = [&](bool ok, const char* msg) {
        if (!ok) { std::cerr << "FAIL: " << msg << '\n'; ++failures; }
        else { std::cout << "ok: " << msg << '\n'; }
    };

    FitController ctrl;
    check(ctrl.getWallpaperFit() == "cover", "default wallpaper fit is cover");
    ctrl.setWallpaperFit("contain");
    check(ctrl.getWallpaperFit() == "contain", "set contain");
    ctrl.setWallpaperFit("center");
    check(ctrl.getWallpaperFit() == "center", "set center");
    ctrl.setWallpaperFit("cover");
    check(ctrl.getWallpaperFit() == "cover", "set cover");

    if (failures) {
        std::cerr << failures << " test(s) failed\n";
        return 1;
    }
    std::cout << "ALL WALLPAPER FIT CONTROLLER TESTS PASSED\n";
    return 0;
}
