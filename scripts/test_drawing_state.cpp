// Headless regression test for Drawing file state across window resizes.

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include <filesystem>
#include <iostream>
#include <string>
#include <unistd.h>
#include <vector>

#include "../src/app/App.hpp"
#include "../src/app/DrawingRaster.hpp"
#include "../src/fs/Filesystem.hpp"

#define private public
#include "../src/app/DrawingApp.hpp"
#undef private

int main() {
    int failures = 0;
    auto check = [&](bool ok, const char* message) {
        if (!ok) {
            std::cerr << "FAIL: " << message << '\n';
            ++failures;
        } else {
            std::cout << "ok: " << message << '\n';
        }
    };

    const std::filesystem::path hostRoot = std::filesystem::temp_directory_path()
        / ("monolith-drawing-state-" + std::to_string(getpid()));
    std::error_code ec;
    std::filesystem::remove_all(hostRoot, ec);

    monolith::fs::Filesystem fs(hostRoot.string());
    check(fs.initialize(), "drawing state filesystem initialize");
    std::vector<uint8_t> pixels(2 * 2 * 4, 255);
    check(fs.writeFile("/drawings/resize.modr",
                       monolith::drawing::encodeModr(2, 2, pixels)),
          "write resize drawing");

    monolith::app::DrawingApp drawing(nullptr, &fs);
    drawing.onResize(300, 300);
    check(!drawing.m_dirty, "initial blank resize stays clean");
    check(drawing.loadFromPath("/drawings/resize.modr"), "load resize drawing");
    check(!drawing.m_dirty, "loaded drawing starts clean");

    drawing.onResize(320, 300);
    check(drawing.m_dirty, "resizing a loaded drawing marks it modified");
    check(drawing.m_undoStack.empty() && drawing.m_redoStack.empty(),
          "resizing a loaded drawing clears incompatible history");

    std::filesystem::remove_all(hostRoot, ec);
    if (failures == 0) {
        std::cout << "ALL DRAWING STATE TESTS PASSED\n";
        return 0;
    }
    std::cerr << failures << " test(s) failed\n";
    return 1;
}
