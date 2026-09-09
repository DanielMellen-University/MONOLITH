// Headless regression test for retrying file opens after an initial load fails.

#include "../src/app/DrawingRaster.hpp"
#include "../src/fs/Filesystem.hpp"
#include "../src/window/WindowManager.hpp"

#include <SDL2/SDL_ttf.h>

#include <filesystem>
#include <iostream>
#include <string>
#include <unistd.h>
#include <vector>

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
        / ("monolith-window-file-open-" + std::to_string(getpid()));
    std::error_code ec;
    std::filesystem::remove_all(hostRoot, ec);

    monolith::fs::Filesystem fs(hostRoot.string());
    check(fs.initialize(), "file-open probe filesystem initialize");
    check(fs.createDirectory("/docs"), "create editor directory");
    check(fs.createDirectory("/drawings"), "create drawing directory");
    check(fs.writeFile("/drawings/retry.modr", "not a drawing"),
          "write corrupt drawing placeholder");

    check(TTF_Init() == 0, "SDL_ttf initialize");
    TTF_Font* font = TTF_OpenFont("assets/fonts/DejaVuSans.ttf", 14);
    check(font != nullptr, "load headless test font");
    if (!font) {
        TTF_Quit();
        std::filesystem::remove_all(hostRoot, ec);
        return 1;
    }

    {
        monolith::window::WindowManager wm;
        wm.setAppResources(font, &fs);

        wm.openPath("/docs/retry.txt");
        check(!wm.focusEditorForFile("/docs/retry.txt"),
              "failed editor open does not reserve a file singleton");

        check(fs.writeFile("/docs/retry.txt", "ready"), "create editor file after failed open");
        wm.openPath("/docs/retry.txt");
        check(wm.focusEditorForFile("/docs/retry.txt"),
              "editor can retry and bind after the file appears");

        wm.openPath("/drawings/retry.modr");
        check(!wm.focusDrawingForFile("/drawings/retry.modr"),
              "failed drawing open does not reserve a file singleton");

        std::vector<uint8_t> pixels(2 * 2 * 4, 255);
        const std::string validDrawing = monolith::drawing::encodeModr(2, 2, pixels);
        check(!validDrawing.empty() && fs.writeFile("/drawings/retry.modr", validDrawing),
              "replace corrupt drawing with valid file");
        wm.openPath("/drawings/retry.modr");
        check(wm.focusDrawingForFile("/drawings/retry.modr"),
              "drawing can retry and bind after the file is repaired");
    }

    TTF_CloseFont(font);
    TTF_Quit();

    std::filesystem::remove_all(hostRoot, ec);
    if (failures == 0) {
        std::cout << "ALL WINDOW FILE OPEN TESTS PASSED\n";
        return 0;
    }
    std::cerr << failures << " test(s) failed\n";
    return 1;
}
