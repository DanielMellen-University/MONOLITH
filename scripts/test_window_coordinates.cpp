#include <SDL2/SDL.h>

#define private public
#include "../src/window/WindowManager.hpp"
#undef private

#include <cstdlib>
#include <iostream>
#include <memory>

namespace {

class ProbeApp final : public monolith::app::App {
public:
    void render(SDL_Renderer*, const SDL_Rect&) override {}

    void handleEvent(const SDL_Event& event) override {
        if (event.type == SDL_MOUSEBUTTONDOWN) {
            lastDownX = event.button.x;
            lastDownY = event.button.y;
            ++buttonDowns;
        }
    }

    void onResize(int clientWidth, int clientHeight) override {
        lastResizeWidth = clientWidth;
        lastResizeHeight = clientHeight;
        ++resizeCalls;
    }

    int lastDownX = -1;
    int lastDownY = -1;
    int buttonDowns = 0;
    int lastResizeWidth = 0;
    int lastResizeHeight = 0;
    int resizeCalls = 0;
};

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

    if (!std::getenv("SDL_VIDEODRIVER")) {
        setenv("SDL_VIDEODRIVER", "dummy", 1);
    }
    check(SDL_Init(SDL_INIT_VIDEO) == 0, "SDL initializes for taskbar geometry checks");
    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(
        0, 1280, 720, 32, SDL_PIXELFORMAT_RGBA32);
    SDL_Renderer* renderer = surface ? SDL_CreateSoftwareRenderer(surface) : nullptr;
    SDL_PixelFormat* rgbaFormat = SDL_AllocFormat(SDL_PIXELFORMAT_RGBA32);
    check(renderer != nullptr, "software renderer initializes for taskbar geometry checks");

    monolith::window::WindowManager wm;
    wm.setLogicalDesktopSize(1000, 700);
    wm.setContentScale(2.0f);

    auto probe = std::make_unique<ProbeApp>();
    ProbeApp* probePtr = probe.get();
    monolith::window::Window* window = wm.createWindow(
        "Probe", 100, 100, 300, 240, std::move(probe));

    check(wm.getWindowAt(220, 280) == window,
          "scaled hit testing finds a window without prior motion");
    check(wm.getWindowAt(800, 280) == nullptr,
          "scaled hit testing rejects the logical right edge");
    check(wm.getResizeDirectionAt(798, 280) == monolith::window::ResizeDirection::Right,
          "scaled resize hit testing finds the right edge");

    SDL_Event click{};
    click.type = SDL_MOUSEBUTTONDOWN;
    click.button.button = SDL_BUTTON_LEFT;
    click.button.x = 220;
    click.button.y = 280;
    wm.handleEvent(click);
    check(probePtr->buttonDowns == 1, "direct scaled click reaches the app");
    check(probePtr->lastDownX == 10 && probePtr->lastDownY == 8,
          "client event coordinates are logical and title-bar relative");

    SDL_Event dragStart{};
    dragStart.type = SDL_MOUSEBUTTONDOWN;
    dragStart.button.button = SDL_BUTTON_LEFT;
    dragStart.button.x = 220;
    dragStart.button.y = 220;
    wm.handleEvent(dragStart);

    SDL_Event dragMotion{};
    dragMotion.type = SDL_MOUSEMOTION;
    dragMotion.motion.x = 400;
    dragMotion.motion.y = 400;
    wm.handleEvent(dragMotion);
    wm.update();
    check(window->rect.x == 190 && window->rect.y == 190,
          "scaled dragging uses logical pointer coordinates");

    wm.setLogicalDesktopSize(120, 120);
    check(window->rect.x == 0 && window->rect.w == 120,
          "narrow logical desktops keep clamped windows inside the left edge");
    const SDL_Rect narrowUsable = wm.getUsableDesktopRect();
    check(window->rect.y >= narrowUsable.y
              && window->rect.y + window->rect.h <= narrowUsable.y + narrowUsable.h,
          "narrow logical desktops keep the full frame above the taskbar");
    check(probePtr->resizeCalls == 2
              && probePtr->lastResizeWidth == window->rect.w
              && probePtr->lastResizeHeight == window->rect.h - monolith::window::Window::TITLE_BAR_HEIGHT,
          "desktop clamping notifies the app of its changed client size");

    window->rect = {0, 0, 300, 240};
    window->minimized = true;
    const int resizeCallsBeforeMinimizedRestore = probePtr->resizeCalls;
    SDL_Event altTab{};
    altTab.type = SDL_KEYDOWN;
    altTab.key.keysym.sym = SDLK_TAB;
    altTab.key.keysym.mod = KMOD_ALT;
    wm.handleEvent(altTab);
    check(!window->minimized
              && probePtr->resizeCalls == resizeCallsBeforeMinimizedRestore + 1,
          "restoring a minimized clamped window notifies the app once");

    wm.setContentScale(1.0f);
    wm.setLogicalDesktopSize(500, 400);
    window->rect = {100, 100, 300, 200};
    const int resizeCallsBeforeEdgeDrag = probePtr->resizeCalls;
    wm.m_resizingWindow = window;
    wm.m_resizeDirection = monolith::window::ResizeDirection::BottomRight;
    wm.m_mouseDown = true;
    wm.m_mouseX = 1000;
    wm.m_mouseY = 350;
    wm.update();
    check(window->rect.w <= 500 && window->rect.h <= 372
              && probePtr->resizeCalls == resizeCallsBeforeEdgeDrag + 1
              && probePtr->lastResizeWidth == window->rect.w
              && probePtr->lastResizeHeight == window->rect.h
                  - monolith::window::Window::TITLE_BAR_HEIGHT,
          "interactive resize notifies the app after final desktop clamping");
    wm.m_mouseDown = false;
    wm.m_resizingWindow = nullptr;
    wm.m_resizeDirection = monolith::window::ResizeDirection::None;

    wm.setLogicalDesktopSize(800, 600);
    window->minimized = false;
    window->maximized = false;
    window->rect = {430, 300, 300, 220};
    const auto maximizeButtons = wm.getTitleButtonRects(*window);
    const int resizeCallsBeforeMaximize = probePtr->resizeCalls;
    check(wm.handleTitleBarButtons(
              window, maximizeButtons.maximize.x + 1, maximizeButtons.maximize.y + 1),
          "maximize button toggles the window");
    check(window->maximized && probePtr->resizeCalls == resizeCallsBeforeMaximize + 1,
          "maximizing notifies the app of the client size");

    wm.setLogicalDesktopSize(420, 220);
    const SDL_Rect smallerUsable = wm.getUsableDesktopRect();
    check(window->maximized
              && window->rect.x == smallerUsable.x
              && window->rect.y == smallerUsable.y
              && window->rect.w == smallerUsable.w
              && window->rect.h == smallerUsable.h
              && probePtr->resizeCalls == resizeCallsBeforeMaximize + 2,
          "desktop resize keeps maximized geometry and app size synchronized");

    const auto restoreButtons = wm.getTitleButtonRects(*window);
    const int resizeCallsBeforeMaxRestore = probePtr->resizeCalls;
    check(wm.handleTitleBarButtons(
              window, restoreButtons.maximize.x + 1, restoreButtons.maximize.y + 1),
          "restore button toggles a maximized window");
    check(!window->maximized
              && window->rect.x >= 0
              && window->rect.y >= 0
              && window->rect.x + window->rect.w <= wm.m_logicalWidth
              && window->rect.y + window->rect.h
                  <= wm.getUsableDesktopRect().y + wm.getUsableDesktopRect().h
              && probePtr->resizeCalls == resizeCallsBeforeMaxRestore + 1,
          "restoring after a desktop shrink clamps the frame above the taskbar");

    wm.setLogicalDesktopSize(120, 20);
    const SDL_Rect undersizedUsable = wm.getUsableDesktopRect();
    check(window->rect.y == 0
              && window->rect.h <= undersizedUsable.h,
          "undersized logical desktops keep frame geometry non-negative");

    auto rectInside = [](const SDL_Rect& rect, int width, int height) {
        return rect.x >= 0 && rect.y >= 0 && rect.w >= 0 && rect.h >= 0
            && rect.x <= width - rect.w
            && rect.y <= height - rect.h;
    };

    if (renderer) {
        wm.setContentScale(1.0f);
        wm.setLogicalDesktopSize(120, 120);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        wm.render(renderer);
        check(wm.m_taskbarButtonAreaLeft >= 0
                  && wm.m_taskbarButtonAreaWidth >= 0
                  && wm.m_taskbarButtonAreaLeft + wm.m_taskbarButtonAreaWidth <= 120,
              "narrow taskbars keep the button viewport inside the desktop");
        check(rectInside(wm.m_taskbarLeftArrowRect, 120, 120)
                  && rectInside(wm.m_taskbarRightArrowRect, 120, 120),
              "narrow taskbars keep scroll arrow hit rectangles inside the desktop");
        for (const auto& entry : wm.m_taskbarEntries) {
            check(rectInside(entry.rect, 120, 120),
                  "narrow taskbars keep window button hit rectangles inside the desktop");
        }

        wm.setLogicalDesktopSize(200, 120);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        wm.render(renderer);
        check(wm.m_taskbarNeedsScroll,
              "taskbar scrolling remains active when the button strip is crowded");
        Uint32 arrowPixel = 0;
        SDL_Rect arrowSample = {
            wm.m_taskbarRightArrowRect.x + 1,
            wm.m_taskbarRightArrowRect.y + 1,
            1,
            1
        };
        const bool readArrowPixel = rgbaFormat
            && SDL_RenderReadPixels(renderer, &arrowSample, SDL_PIXELFORMAT_RGBA32,
                                    &arrowPixel, sizeof(arrowPixel)) == 0;
        Uint8 arrowR = 0, arrowG = 0, arrowB = 0, arrowA = 0;
        if (readArrowPixel) {
            SDL_GetRGBA(arrowPixel, rgbaFormat, &arrowR, &arrowG, &arrowB, &arrowA);
        }
        check(readArrowPixel && arrowR == 80 && arrowG == 80 && arrowB == 90,
              "taskbar buttons stay clipped behind the visible right arrow");

        wm.setLogicalDesktopSize(1000, 700);
        wm.m_taskbarScrollOffset = 400;
        wm.setLogicalDesktopSize(120, 120);
        wm.render(renderer);
        check(!wm.m_taskbarNeedsScroll || wm.m_taskbarButtonAreaWidth >= 40,
              "taskbar scrolling is disabled when arrow controls cannot fit");
        check(wm.m_taskbarScrollOffset == 0,
              "shrinking the desktop resets an unusable taskbar scroll offset");
    }

    if (renderer) SDL_DestroyRenderer(renderer);
    if (surface) SDL_FreeSurface(surface);
    if (rgbaFormat) SDL_FreeFormat(rgbaFormat);
    SDL_Quit();

    if (failures == 0) {
        std::cout << "ALL WINDOW COORDINATE TESTS PASSED\n";
        return 0;
    }
    return 1;
}
