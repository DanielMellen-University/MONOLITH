// Headless regression test for Settings wallpaper path completion.

#include "../src/fs/Filesystem.hpp"

#include <SDL2/SDL.h>
#include <filesystem>
#include <iostream>
#include <string>
#include <unistd.h>

#define private public
#include "../src/app/SettingsApp.hpp"
#undef private

namespace {

struct TestController final : monolith::app::IWindowController {
    std::string wallpaperPath;
    int logicalWidth = 1280;
    int logicalHeight = 720;

    void close() override {}
    void setTitle(const std::string&) override {}

    std::string getWallpaperPath() const override {
        return wallpaperPath;
    }

    void setWallpaperPath(const std::string& path) override {
        wallpaperPath = path;
    }

    void getLogicalDesktopSize(int& width, int& height) const override {
        width = logicalWidth;
        height = logicalHeight;
    }
};

struct TestSettings final : monolith::app::SettingsApp {
    using monolith::app::App::setController;

    TestSettings(monolith::fs::Filesystem* fs)
        : SettingsApp(nullptr, fs) {}
};

void key(TestSettings& settings, SDL_Keycode sym) {
    SDL_Event event{};
    event.type = SDL_KEYDOWN;
    event.key.keysym.sym = sym;
    settings.handleEvent(event);
}

} // namespace

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

    const std::string hostRoot =
        "/tmp/monolith-settings-app-state-" + std::to_string(getpid());
    std::error_code ec;
    std::filesystem::remove_all(hostRoot, ec);
    monolith::fs::Filesystem fs(hostRoot);
    check(fs.initialize(), "settings state filesystem initialize");
    check(fs.createDirectory("/Wallpapers/art"), "create wallpaper completion directories");
    check(fs.writeFile("/Wallpapers/alpha.bmp", "a"), "create first BMP wallpaper");
    check(fs.writeFile("/Wallpapers/alpine.bmp", "b"), "create second BMP wallpaper");
    check(fs.writeFile("/Wallpapers/notes.txt", "not a wallpaper"),
          "create non-BMP completion distractor");

    TestSettings settings(&fs);
    TestController controller;
    settings.setController(&controller);
    controller.logicalWidth = 1024;
    controller.logicalHeight = 640;
    settings.onResize(400, 240);
    check(settings.m_lines.size() > 5
              && settings.m_lines[5].label == "Logical desktop"
              && settings.m_lines[5].value == "1024 x 640",
          "Settings reports the live logical desktop size");
    settings.m_wallpaperFieldFocused = true;

    settings.m_wallpaperEditBuffer = "/Wallpapers/al";
    settings.m_wallpaperCursorPos = settings.m_wallpaperEditBuffer.size();
    key(settings, SDLK_TAB);
    check(settings.m_wallpaperEditBuffer == "/Wallpapers/alp",
          "Tab extends multiple BMP matches to their shared prefix");

    settings.m_wallpaperEditBuffer = "/Wallpapers/alpha";
    settings.m_wallpaperCursorPos = settings.m_wallpaperEditBuffer.size();
    key(settings, SDLK_TAB);
    check(settings.m_wallpaperEditBuffer == "/Wallpapers/alpha.bmp",
          "Tab completes one BMP filename");

    settings.m_wallpaperEditBuffer = "/Wallpapers/ar";
    settings.m_wallpaperCursorPos = settings.m_wallpaperEditBuffer.size();
    key(settings, SDLK_TAB);
    check(settings.m_wallpaperEditBuffer == "/Wallpapers/art/",
          "Tab completes a directory with a trailing slash");

    settings.m_wallpaperEditBuffer = "/Wallpapers/n";
    settings.m_wallpaperCursorPos = settings.m_wallpaperEditBuffer.size();
    key(settings, SDLK_TAB);
    check(settings.m_wallpaperEditBuffer == "/Wallpapers/n",
          "Tab ignores non-BMP files");

    settings.m_wallpaperEditBuffer = "/Wallpapers/alpha.bmp";
    settings.m_wallpaperCursorPos = settings.m_wallpaperEditBuffer.size();
    key(settings, SDLK_RETURN);
    check(controller.wallpaperPath == "/Wallpapers/alpha.bmp"
              && !settings.m_wallpaperFieldFocused,
          "completed wallpaper path applies through the shell controller");

    settings.m_wallpaperFieldFocused = true;
    settings.m_wallpaperEditBuffer = "/Wallpapers/alpha.bmp/child.bmp";
    settings.m_wallpaperCursorPos = std::string("/Wallpapers/alpha.bmp/").size();
    settings.onVirtualPathMoved("/Wallpapers", "/Archive");
    check(settings.m_wallpaperEditBuffer == "/Archive/alpha.bmp/child.bmp"
              && settings.m_wallpaperCursorPos == std::string("/Archive/alpha.bmp/").size(),
          "focused wallpaper prompt follows a moved parent and preserves its caret");
    settings.m_wallpaperEditBuffer = "/Wallpapers/art/";
    settings.m_wallpaperCursorPos = settings.m_wallpaperEditBuffer.size();
    settings.onVirtualPathMoved("/Wallpapers", "/Archive");
    check(settings.m_wallpaperEditBuffer == "/Archive/art/",
          "focused wallpaper directory prompt preserves its trailing slash");
    settings.onVirtualPathRemoved("/Archive");
    check(settings.m_wallpaperEditBuffer.empty() && settings.m_wallpaperCursorPos == 0,
          "focused wallpaper prompt clears a deleted parent directory");

    settings.m_wallpaperFieldFocused = true;
    settings.m_wallpaperEditBuffer = "/Wallpapers/alpha.bmp";
    settings.m_wallpaperCursorPos = 2;
    settings.onVirtualPathMoved("/Wallpapers", "/\xC3\xA9");
    check(settings.m_wallpaperEditBuffer == "/\xC3\xA9/alpha.bmp"
              && settings.m_wallpaperCursorPos == 1,
          "moved wallpaper prompt caret stays on a UTF-8 boundary");

    settings.m_wallpaperScrollPx = 42;
    settings.onUiScaleChanged();
    check(settings.m_wallpaperScrollPx == 0,
          "settings resets pixel prompt scroll after UI scaling");

    std::filesystem::remove_all(hostRoot, ec);
    if (failures == 0) {
        std::cout << "ALL SETTINGS APP STATE TESTS PASSED\n";
        return 0;
    }
    std::cerr << failures << " test(s) failed\n";
    return 1;
}
