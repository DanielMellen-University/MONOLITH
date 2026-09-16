// Headless regression test for Settings wallpaper path completion.

#include "../src/fs/Filesystem.hpp"

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
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
    uint8_t backgroundR = 25;
    uint8_t backgroundG = 25;
    uint8_t backgroundB = 30;
    bool clock24Hour = false;
    int uiScalePercent = 100;

    void close() override {}
    void setTitle(const std::string&) override {}

    std::string getWallpaperPath() const override {
        return wallpaperPath;
    }

    void setWallpaperPath(const std::string& path) override {
        wallpaperPath = path;
    }

    void setDesktopBackgroundColor(uint8_t r, uint8_t g, uint8_t b) override {
        backgroundR = r;
        backgroundG = g;
        backgroundB = b;
    }

    void setClock24Hour(bool enabled) override {
        clock24Hour = enabled;
    }

    int getUiScalePercent() const override {
        return uiScalePercent;
    }

    void setUiScalePercent(int percent) override {
        uiScalePercent = percent;
    }

    void getLogicalDesktopSize(int& width, int& height) const override {
        width = logicalWidth;
        height = logicalHeight;
    }
};

struct TestSettings final : monolith::app::SettingsApp {
    using monolith::app::App::setController;

    TestSettings(TTF_Font* font, monolith::fs::Filesystem* fs)
        : SettingsApp(font, fs) {}
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
    check(fs.createDirectory("/Wallpapers/unicode"),
          "create Unicode wallpaper completion directory");
    check(fs.writeFile("/Wallpapers/alpha.bmp", "a"), "create first BMP wallpaper");
    check(fs.writeFile("/Wallpapers/alpine.bmp", "b"), "create second BMP wallpaper");
    check(fs.writeFile("/Wallpapers/unicode/\xC3\xA9" "clair.png", "a"),
          "write first Unicode wallpaper completion candidate");
    check(fs.writeFile("/Wallpapers/unicode/\xC3\xAA" "cole.png", "b"),
          "write second Unicode wallpaper completion candidate");
    check(fs.writeFile("/Wallpapers/notes.txt", "not a wallpaper"),
          "create non-BMP completion distractor");

    check(SDL_Init(SDL_INIT_VIDEO) == 0, "settings state SDL initialize");
    check(TTF_Init() == 0, "settings state SDL_ttf initialize");
    TTF_Font* font = TTF_OpenFont("assets/fonts/DejaVuSans.ttf", 14);
    check(font != nullptr, "settings state loads test font");
    if (!font) {
        TTF_Quit();
        SDL_Quit();
        std::filesystem::remove_all(hostRoot, ec);
        return 1;
    }

    TestSettings settings(font, &fs);
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

    settings.m_wallpaperEditBuffer = "/Wallpapers/unicode/";
    settings.m_wallpaperCursorPos = settings.m_wallpaperEditBuffer.size();
    key(settings, SDLK_TAB);
    check(settings.m_wallpaperEditBuffer == "/Wallpapers/unicode/",
          "Settings completion does not insert a partial UTF-8 codepoint");

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
    const int baseLineHeight = settings.getLineHeight();
    const int baseControlSize = settings.getControlSize();
    const int baseFieldHeight = settings.getFieldHeight();
    const int baseFooterHeight = settings.getFooterHeight();
    const int baseContentHeight = settings.m_contentHeight;
    settings.m_backgroundSwatches[0] = {10, 10, 20, 20};
    settings.m_clockFormatHitRects[0] = {10, 40, 20, 20};
    settings.m_uiScaleHitRects[0] = {10, 70, 20, 20};
    settings.m_wallpaperFieldRect = {10, 100, 80, 20};
    settings.m_wallpaperSetRect = {95, 100, 30, 20};
    settings.m_wallpaperClearRect = {130, 100, 40, 20};
    check(TTF_SetFontSize(font, 22) == 0, "settings state applies larger test font");
    settings.onUiScaleChanged();
    check(settings.m_wallpaperScrollPx == 0,
          "settings resets pixel prompt scroll after UI scaling");
    check(settings.getLineHeight() > baseLineHeight
              && settings.getControlSize() > baseControlSize
              && settings.getFieldHeight() > baseFieldHeight
              && settings.getFooterHeight() > baseFooterHeight
              && settings.m_contentHeight > baseContentHeight,
          "Settings layout bands grow with the shared interface font");
    check(settings.m_backgroundSwatches[0].w == 0
              && settings.m_clockFormatHitRects[0].w == 0
              && settings.m_uiScaleHitRects[0].w == 0
              && settings.m_wallpaperFieldRect.w == 0
              && settings.m_wallpaperSetRect.w == 0
              && settings.m_wallpaperClearRect.w == 0,
          "Settings scaling invalidates stale hit targets");
    settings.m_backgroundSwatches[1] = {10, 10, 20, 20};
    settings.m_clockFormatHitRects[1] = {10, 40, 20, 20};
    settings.m_uiScaleHitRects[1] = {10, 70, 20, 20};
    settings.m_wallpaperFieldRect = {10, 100, 80, 20};
    settings.m_wallpaperSetRect = {95, 100, 30, 20};
    settings.m_wallpaperClearRect = {130, 100, 40, 20};
    settings.onResize(420, 240);
    check(settings.m_backgroundSwatches[1].w == 0
              && settings.m_clockFormatHitRects[1].w == 0
              && settings.m_uiScaleHitRects[1].w == 0
              && settings.m_wallpaperFieldRect.w == 0
              && settings.m_wallpaperSetRect.w == 0
              && settings.m_wallpaperClearRect.w == 0,
          "Settings resize invalidates stale hit targets");
    settings.ensureHitTargets();
    const SDL_Rect preRenderPreset = settings.m_backgroundSwatches[2];
    const SDL_Rect preRenderField = settings.m_wallpaperFieldRect;
    const SDL_Rect preRenderClock = settings.m_clockFormatHitRects[1];
    const SDL_Rect preRenderScale = settings.m_uiScaleHitRects[2];
    check(preRenderPreset.w > 0 && preRenderField.w > 0
              && preRenderField.h > 0 && settings.m_wallpaperSetRect.w > 0
              && settings.m_wallpaperClearRect.w > 0
              && preRenderClock.w > 0 && preRenderScale.w > 0,
          "Settings rebuilds every control target on demand");
    auto clickRect = [&](const SDL_Rect& rect) {
        SDL_Event click{};
        click.type = SDL_MOUSEBUTTONDOWN;
        click.button.button = SDL_BUTTON_LEFT;
        click.button.x = rect.x + rect.w / 2;
        click.button.y = rect.y + rect.h / 2;
        settings.handleEvent(click);
    };
    settings.invalidateHitTargets();
    clickRect(preRenderPreset);
    check(controller.backgroundR == 32 && controller.backgroundG == 36
              && controller.backgroundB == 48,
          "Settings accepts a queued scaled preset click before render");
    settings.invalidateHitTargets();
    clickRect(preRenderField);
    check(settings.m_wallpaperFieldFocused,
          "Settings accepts a queued scaled wallpaper-field click before render");
    settings.m_wallpaperFieldFocused = false;
    settings.invalidateHitTargets();
    clickRect(preRenderClock);
    check(controller.clock24Hour,
          "Settings accepts a queued scaled clock click before render");
    settings.invalidateHitTargets();
    clickRect(preRenderScale);
    check(controller.uiScalePercent == 115,
          "Settings accepts a queued scaled UI-size click before render");
    settings.m_scrollOffset = 20;
    settings.invalidateHitTargets();
    settings.ensureHitTargets();
    const SDL_Rect scrolledPreset = settings.m_backgroundSwatches[3];
    check(scrolledPreset.y > 0,
          "Settings keeps a scrolled control inside the client viewport");
    settings.invalidateHitTargets();
    clickRect(scrolledPreset);
    check(controller.backgroundR == 20 && controller.backgroundG == 32
              && controller.backgroundB == 24,
          "Settings clicks scrolled controls in client coordinates");
    settings.m_wallpaperFieldFocused = false;
    settings.m_backgroundSwatches[2] = {10, 10, 20, 20};
    SDL_Event wheel{};
    wheel.type = SDL_MOUSEWHEEL;
    wheel.wheel.y = -1;
    settings.handleEvent(wheel);
    check(settings.m_backgroundSwatches[2].w == 0,
          "Settings wheel scrolling invalidates stale hit targets");
    settings.m_clockFormatHitRects[0] = {10, 40, 20, 20};
    key(settings, SDLK_HOME);
    check(settings.m_clockFormatHitRects[0].w == 0,
          "Settings Home scrolling invalidates stale hit targets");
    settings.m_uiScaleHitRects[0] = {10, 70, 20, 20};
    key(settings, SDLK_END);
    check(settings.m_uiScaleHitRects[0].w == 0,
          "Settings End scrolling invalidates stale hit targets");
    key(settings, SDLK_HOME);
    const SDL_Rect tinyContent{10, 20, 200, 8};
    const SDL_Rect tinyFooter = settings.getFooterRect(tinyContent);
    check(tinyFooter.y >= tinyContent.y
              && tinyFooter.y + tinyFooter.h <= tinyContent.y + tinyContent.h,
          "Settings footer stays inside an undersized client area");

    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(
        0, 240, 260, 32, SDL_PIXELFORMAT_RGBA32);
    SDL_Renderer* renderer = surface ? SDL_CreateSoftwareRenderer(surface) : nullptr;
    check(renderer != nullptr, "settings state creates a software renderer");
    if (renderer) {
        const SDL_Rect expectedClip{5, 6, 140, 120};
        SDL_RenderSetClipRect(renderer, &expectedClip);
        settings.render(renderer, {0, 0, 200, 240});
        auto insideClient = [](const SDL_Rect& rect, int width) {
            return rect.x >= 0 && rect.y >= 0 && rect.w >= 0 && rect.h >= 0
                && rect.x + rect.w <= width;
        };
        check(insideClient(settings.m_wallpaperFieldRect, 200)
                  && insideClient(settings.m_wallpaperSetRect, 200)
                  && insideClient(settings.m_wallpaperClearRect, 200),
              "Settings wallpaper controls stay inside a narrow client width");
        SDL_Rect restoredClip{};
        SDL_RenderGetClipRect(renderer, &restoredClip);
        check(restoredClip.x == expectedClip.x
                  && restoredClip.y == expectedClip.y
                  && restoredClip.w == expectedClip.w
                  && restoredClip.h == expectedClip.h,
              "Settings restores the caller renderer clip after rendering");
        const size_t cachedSurfaceCount = settings.m_textSurfaceCache.m_entries.size();
        settings.render(renderer, {0, 0, 200, 240});
        check(cachedSurfaceCount > 0
                  && settings.m_textSurfaceCache.m_entries.size() == cachedSurfaceCount,
              "Settings reuses cached text surfaces between frames");
        settings.onUiScaleChanged();
        check(settings.m_textSurfaceCache.m_entries.empty(),
              "Settings clears cached text surfaces when UI scale changes");
        SDL_DestroyRenderer(renderer);
    }
    if (surface) SDL_FreeSurface(surface);

    std::filesystem::remove_all(hostRoot, ec);
    TTF_CloseFont(font);
    TTF_Quit();
    SDL_Quit();
    if (failures == 0) {
        std::cout << "ALL SETTINGS APP STATE TESTS PASSED\n";
        return 0;
    }
    std::cerr << failures << " test(s) failed\n";
    return 1;
}
