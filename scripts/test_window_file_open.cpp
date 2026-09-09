// Headless regression test for retrying file opens after an initial load fails.

#include "../src/app/DrawingRaster.hpp"
#include "../src/fs/Filesystem.hpp"
#define private public
#include "../src/window/WindowManager.hpp"
#undef private

#include <SDL2/SDL_ttf.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
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

        const auto settingsPath = hostRoot / "desktop-settings.txt";
        wm.loadDesktopSettings(settingsPath.string());
        check(fs.createDirectory("/Wallpapers"), "create wallpaper directory");
        check(fs.createDirectory("/archive"), "create directory move destination");
        check(fs.writeFile("/Wallpapers/old.bmp", "placeholder"),
              "write wallpaper move source");
        wm.setWallpaperPath("/Wallpapers/old.bmp");
        check(fs.rename("/Wallpapers/old.bmp", "/archive/moved.bmp"),
              "move configured wallpaper");
        wm.notifyVirtualPathMoved("/Wallpapers/old.bmp", "/archive/moved.bmp");
        check(wm.getWallpaperPath() == "/archive/moved.bmp",
              "wallpaper setting follows a moved file");
        std::ifstream settingsFile(settingsPath);
        const std::string settingsText(
            std::istreambuf_iterator<char>(settingsFile), {});
        check(settingsText.find("wallpaper_path=/archive/moved.bmp") != std::string::npos,
              "moved wallpaper path is persisted");

        wm.openPath("/docs/retry.txt");
        check(!wm.focusEditorForFile("/docs/retry.txt"),
              "failed editor open does not reserve a file singleton");
        check(wm.m_windows.size() == 1 && wm.m_windows.front()->title == "Editor"
                  && wm.m_windows.front()->appBaseTitle == "Editor"
                  && wm.m_windows.front()->appInstanceNumber == 1,
              "failed editor open becomes a tracked untitled window");

        const auto failedSessionPath = hostRoot / "failed-session.txt";
        check(wm.saveSession(failedSessionPath.string()),
              "save session with failed editor window");
        std::ifstream failedSession(failedSessionPath);
        const std::string failedSessionText(
            std::istreambuf_iterator<char>(failedSession), {});
        check(failedSessionText.find("\neditor ") != std::string::npos,
              "failed editor window remains eligible for session restore");

        check(fs.writeFile("/docs/retry.txt", "ready"), "create editor file after failed open");
        wm.openPath("/docs/retry.txt");
        check(wm.focusEditorForFile("/docs/retry.txt"),
              "editor can retry and bind after the file appears");

        check(fs.rename("/docs/retry.txt", "/docs/renamed.txt"),
              "rename bound editor file");
        wm.notifyVirtualPathMoved("/docs/retry.txt", "/docs/renamed.txt");
        check(!wm.focusEditorForFile("/docs/retry.txt")
                  && wm.focusEditorForFile("/docs/renamed.txt"),
              "renamed editor file keeps its singleton binding");
        auto renamedEditor = std::find_if(
            wm.m_windows.begin(), wm.m_windows.end(),
            [](const auto& window) {
                return window && window->editedFilePath == "/docs/renamed.txt";
            });
        check(renamedEditor != wm.m_windows.end()
                  && (*renamedEditor)->title == "Editor - renamed.txt",
              "renamed editor file updates its title and path");

        check(fs.isDirectory("/archive"), "directory move destination remains available");
        check(fs.createDirectory("/docs/nested"), "create nested editor directory");
        check(fs.writeFile("/docs/nested/child.txt", "nested"),
              "write nested editor file");
        wm.openPath("/docs/nested/child.txt");
        check(wm.focusEditorForFile("/docs/nested/child.txt"),
              "open nested editor file");
        check(fs.rename("/docs/nested", "/archive/nested"),
              "move bound editor directory");
        wm.notifyVirtualPathMoved("/docs/nested", "/archive/nested");
        check(wm.focusEditorForFile("/archive/nested/child.txt"),
              "moved directory remaps nested editor binding");

        wm.openPath("/drawings/retry.modr");
        check(!wm.focusDrawingForFile("/drawings/retry.modr"),
              "failed drawing open does not reserve a file singleton");
        auto failedDrawing = std::find_if(
            wm.m_windows.begin(), wm.m_windows.end(),
            [](const auto& window) {
                return window && window->title == "Drawing"
                    && window->appBaseTitle == "Drawing";
            });
        check(failedDrawing != wm.m_windows.end()
                  && (*failedDrawing)->appInstanceNumber > 0,
              "failed drawing open becomes a tracked untitled window");

        std::vector<uint8_t> pixels(2 * 2 * 4, 255);
        const std::string validDrawing = monolith::drawing::encodeModr(2, 2, pixels);
        check(!validDrawing.empty() && fs.writeFile("/drawings/retry.modr", validDrawing),
              "replace corrupt drawing with valid file");
        wm.openPath("/drawings/retry.modr");
        check(wm.focusDrawingForFile("/drawings/retry.modr"),
              "drawing can retry and bind after the file is repaired");

        check(fs.rename("/drawings/retry.modr", "/archive/retry.modr"),
              "rename bound drawing file");
        wm.notifyVirtualPathMoved("/drawings/retry.modr", "/archive/retry.modr");
        check(!wm.focusDrawingForFile("/drawings/retry.modr")
                  && wm.focusDrawingForFile("/archive/retry.modr"),
              "renamed drawing file keeps its singleton binding");
        auto renamedDrawing = std::find_if(
            wm.m_windows.begin(), wm.m_windows.end(),
            [](const auto& window) {
                return window && window->drawingFilePath == "/archive/retry.modr";
            });
        check(renamedDrawing != wm.m_windows.end()
                  && (*renamedDrawing)->title == "Drawing - retry.modr",
              "renamed drawing file updates its title and path");
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
