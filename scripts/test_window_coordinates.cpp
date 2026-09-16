#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#define private public
#include "../src/window/WindowManager.hpp"
#undef private

#include <cstdlib>
#include <iostream>
#include <memory>

namespace {

class ProbeApp final : public monolith::app::App {
public:
    void render(SDL_Renderer* renderer, const SDL_Rect& contentRect) override {
        SDL_RenderGetClipRect(renderer, &renderClip);
        SDL_GetRenderDrawBlendMode(renderer, &renderBlendMode);
        SDL_GetRenderDrawColor(renderer, &renderColorR, &renderColorG,
                               &renderColorB, &renderColorA);
        if (leakBlendMode) {
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_ADD);
        }
        if (leakDrawColor) {
            SDL_SetRenderDrawColor(renderer, 1, 2, 3, 4);
        }
        renderRect = contentRect;
    }

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
    SDL_Rect renderClip{0, 0, 0, 0};
    SDL_Rect renderRect{0, 0, 0, 0};
    SDL_BlendMode renderBlendMode = SDL_BLENDMODE_NONE;
    bool leakBlendMode = false;
    Uint8 renderColorR = 0;
    Uint8 renderColorG = 0;
    Uint8 renderColorB = 0;
    Uint8 renderColorA = 0;
    bool leakDrawColor = false;
};

class RenderClosingApp final : public monolith::app::App {
public:
    explicit RenderClosingApp(int* observedRenderCalls)
        : observedRenderCalls(observedRenderCalls) {}

    void render(SDL_Renderer*, const SDL_Rect&) override {
        if (observedRenderCalls) ++*observedRenderCalls;
        if (closeOnRender) {
            closeOnRender = false;
            if (auto* controller = getController()) controller->close();
        }
    }

    bool closeOnRender = false;

private:
    int* observedRenderCalls = nullptr;
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
    check(TTF_Init() == 0, "SDL_ttf initializes for taskbar text measurement checks");
    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(
        0, 1280, 720, 32, SDL_PIXELFORMAT_RGBA32);
    SDL_Renderer* renderer = surface ? SDL_CreateSoftwareRenderer(surface) : nullptr;
    SDL_PixelFormat* rgbaFormat = SDL_AllocFormat(SDL_PIXELFORMAT_RGBA32);
    SDL_Window* hostWindow = SDL_CreateWindow(
        "window coordinate test", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
        2048, 1536, SDL_WINDOW_HIDDEN);
    check(renderer != nullptr, "software renderer initializes for taskbar geometry checks");
    check(hostWindow != nullptr, "SDL host window initializes for pointer-position checks");

    monolith::window::WindowManager wm;
    wm.setLogicalDesktopSize(1000, 700);
    wm.setContentScale(2.0f);

    wm.setHeaderOffset(20);
    check(wm.screenToLogicalX(-1) == -1
              && wm.screenToLogicalY(19) == -1
              && wm.screenToLogicalY(20) == 0
              && wm.screenToLogicalY(21) == 0,
          "screen coordinates above a scaled desktop stay outside logical row zero");
    wm.setHeaderOffset(0);

    TTF_Font* font = TTF_OpenFont("assets/fonts/DejaVuSans.ttf", 14);
    check(font != nullptr, "taskbar geometry test loads the shared font");
    wm.setFont(font);

    auto probe = std::make_unique<ProbeApp>();
    ProbeApp* probePtr = probe.get();
    monolith::window::Window* window = wm.createWindow(
        "Probe", 100, 100, 300, 240, std::move(probe));

    auto undersizedProbe = std::make_unique<ProbeApp>();
    ProbeApp* undersizedProbePtr = undersizedProbe.get();
    monolith::window::Window* undersizedWindow = wm.createWindow(
        "Undersized", 20, 20, 20, 20, std::move(undersizedProbe));
    check(undersizedWindow
              && undersizedWindow->rect.w == monolith::window::Window::MIN_WIDTH
              && undersizedWindow->rect.h
                  == monolith::window::Window::MIN_HEIGHT
                      + monolith::window::Window::TITLE_BAR_HEIGHT,
          "new windows honor the shared minimum frame size");
    check(undersizedProbePtr->lastResizeWidth == undersizedWindow->rect.w
              && undersizedProbePtr->lastResizeHeight
                  == undersizedWindow->rect.h
                      - monolith::window::Window::TITLE_BAR_HEIGHT,
          "new apps receive the clamped client size");
    wm.closeWindow(undersizedWindow);

    wm.m_desktopIconSelected = 2;
    wm.m_desktopIconLastClickIndex = 2;
    wm.m_desktopIconLastClickTicks = 1000;
    wm.clearDesktopIconSelection();
    check(wm.m_desktopIconSelected == -1
              && wm.m_desktopIconLastClickIndex == -1
              && wm.m_desktopIconLastClickTicks == 0,
          "clearing an icon selection also clears double-click history");

    wm.m_desktopIconSelected = 1;
    wm.m_desktopIconLastClickIndex = 1;
    wm.m_desktopIconLastClickTicks = 1000;
    SDL_Event ctrlEscape{};
    ctrlEscape.type = SDL_KEYDOWN;
    ctrlEscape.key.keysym.sym = SDLK_ESCAPE;
    ctrlEscape.key.keysym.mod = KMOD_CTRL;
    wm.handleEvent(ctrlEscape);
    check(wm.m_desktopIconSelected == -1
              && wm.m_desktopIconLastClickIndex == -1
              && wm.m_desktopIconLastClickTicks == 0,
          "opening the Start menu clears desktop-icon click history");
    wm.handleEvent(ctrlEscape);

    wm.handleEvent(ctrlEscape);
    SDL_Event menuDownBeforeRender{};
    menuDownBeforeRender.type = SDL_KEYDOWN;
    menuDownBeforeRender.key.keysym.sym = SDLK_DOWN;
    wm.handleEvent(menuDownBeforeRender);
    check(wm.m_showStartMenu
              && !wm.m_startMenuItems.empty()
              && wm.m_startMenuKeyboardIndex == 0,
          "Start menu keyboard input builds hit targets before the first render");
    wm.handleEvent(ctrlEscape);

    wm.handleEvent(ctrlEscape);
    wm.ensureStartMenuHitTargets();
    const SDL_Rect menuBeforeRender = wm.m_startMenuRect;
    wm.m_startMenuItems.clear();
    wm.m_startMenuRect = {0, 0, 0, 0};
    SDL_Event menuClickBeforeRender{};
    menuClickBeforeRender.type = SDL_MOUSEBUTTONDOWN;
    menuClickBeforeRender.button.button = SDL_BUTTON_LEFT;
    menuClickBeforeRender.button.x = menuBeforeRender.x + 2;
    menuClickBeforeRender.button.y = menuBeforeRender.y + 2;
    wm.handleEvent(menuClickBeforeRender);
    check(wm.m_showStartMenu
              && !wm.m_startMenuItems.empty()
              && wm.m_startMenuRect.w > 0,
          "Start menu mouse input rebuilds hit targets before the first render");
    SDL_Event menuClickRelease = menuClickBeforeRender;
    menuClickRelease.type = SDL_MOUSEBUTTONUP;
    wm.handleEvent(menuClickRelease);
    wm.handleEvent(ctrlEscape);

    wm.setLogicalDesktopSize(500, 400);
    wm.render(renderer);
    wm.m_desktopIconSelected = 1;
    wm.m_desktopIconLastClickIndex = 1;
    wm.m_desktopIconLastClickTicks = 1000;
    const SDL_Rect taskbarScreen = wm.logicalRectToScreen(wm.getTaskbarRect());
    SDL_Event taskbarClick{};
    taskbarClick.type = SDL_MOUSEBUTTONDOWN;
    taskbarClick.button.button = SDL_BUTTON_LEFT;
    taskbarClick.button.x = taskbarScreen.x + 20;
    taskbarClick.button.y = taskbarScreen.y + 1;
    wm.handleEvent(taskbarClick);
    check(wm.m_desktopIconSelected == -1
              && wm.m_desktopIconLastClickIndex == -1
              && wm.m_desktopIconLastClickTicks == 0,
          "taskbar clicks clear desktop-icon click history");
    SDL_Event taskbarRelease = taskbarClick;
    taskbarRelease.type = SDL_MOUSEBUTTONUP;
    wm.handleEvent(taskbarRelease);
    wm.m_showStartMenu = false;
    wm.setLogicalDesktopSize(1000, 700);

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
    wm.render(renderer);
    check(probePtr->renderClip.x == probePtr->renderRect.x
              && probePtr->renderClip.y == probePtr->renderRect.y
              && probePtr->renderClip.w == probePtr->renderRect.w
              && probePtr->renderClip.h == probePtr->renderRect.h,
          "WindowManager clips app rendering to the client rectangle");
    SDL_Rect clipAfterRender{};
    SDL_RenderGetClipRect(renderer, &clipAfterRender);
    check(clipAfterRender.w == 0 && clipAfterRender.h == 0,
          "WindowManager restores the renderer clip after app rendering");

    const SDL_Rect expectedFrameClip{7, 9, 180, 120};
    SDL_RenderSetClipRect(renderer, nullptr);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
    SDL_RenderSetClipRect(renderer, &expectedFrameClip);
    wm.render(renderer);
    SDL_Rect clipAfterClippedFrame{};
    SDL_RenderGetClipRect(renderer, &clipAfterClippedFrame);
    check(clipAfterClippedFrame.x == expectedFrameClip.x
              && clipAfterClippedFrame.y == expectedFrameClip.y
              && clipAfterClippedFrame.w == expectedFrameClip.w
              && clipAfterClippedFrame.h == expectedFrameClip.h,
          "WindowManager preserves the caller renderer clip across the full frame");
    SDL_RenderSetClipRect(renderer, nullptr);
    Uint32 outsideFramePixel = 0;
    const SDL_Rect outsideFrameSample{400, 380, 1, 1};
    const bool readOutsideFrame = rgbaFormat
        && SDL_RenderReadPixels(renderer, &outsideFrameSample, SDL_PIXELFORMAT_RGBA32,
                                &outsideFramePixel, sizeof(outsideFramePixel)) == 0;
    Uint8 outsideFrameR = 0, outsideFrameG = 0, outsideFrameB = 0, outsideFrameA = 0;
    if (readOutsideFrame) {
        SDL_GetRGBA(outsideFramePixel, rgbaFormat,
                    &outsideFrameR, &outsideFrameG, &outsideFrameB, &outsideFrameA);
    }
    check(readOutsideFrame && outsideFrameR == 0
              && outsideFrameG == 0 && outsideFrameB == 0,
          "WindowManager keeps shell rendering inside the caller clip");

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

    wm.setLogicalDesktopSize(420, 220);
    window->minimized = false;
    window->maximized = false;
    window->rect = {60, 40, 300, 180};
    const auto maximizeBeforeMinimizeButtons = wm.getTitleButtonRects(*window);
    check(wm.handleTitleBarButtons(
              window,
              maximizeBeforeMinimizeButtons.maximize.x + 1,
              maximizeBeforeMinimizeButtons.maximize.y + 1),
          "maximize can be toggled before a minimized restore");
    const auto minimizeMaximizedButtons = wm.getTitleButtonRects(*window);
    wm.render(renderer);
    check(wm.m_taskbarHitTargetsValid,
          "taskbar targets are valid before a title-bar minimize");
    check(wm.handleTitleBarButtons(
              window,
              minimizeMaximizedButtons.minimize.x + 1,
              minimizeMaximizedButtons.minimize.y + 1),
          "maximized window can be minimized");
    check(window->minimized && window->maximized && !wm.m_taskbarHitTargetsValid,
          "minimizing preserves the maximized window state");

    wm.setLogicalDesktopSize(800, 600);
    const SDL_Rect grownUsable = wm.getUsableDesktopRect();
    const int resizeCallsBeforeMaximizedRestore = probePtr->resizeCalls;
    wm.bringToFront(window);
    check(!window->minimized && window->maximized
              && window->rect.x == grownUsable.x
              && window->rect.y == grownUsable.y
              && window->rect.w == grownUsable.w
              && window->rect.h == grownUsable.h
              && probePtr->resizeCalls == resizeCallsBeforeMaximizedRestore + 1,
          "restoring a minimized maximized window fills a grown desktop immediately");

    wm.setLogicalDesktopSize(120, 20);
    const SDL_Rect undersizedUsable = wm.getUsableDesktopRect();
    check(window->rect.y == 0
              && window->rect.h <= undersizedUsable.h,
          "undersized logical desktops keep frame geometry non-negative");
    wm.render(renderer);
    check(probePtr->renderRect.h >= 0
              && probePtr->renderClip.h >= 0
              && probePtr->renderRect.h == 0
              && probePtr->renderClip.h == 0,
          "WindowManager never passes a negative client height to apps");

    auto rectInside = [](const SDL_Rect& rect, int width, int height) {
        return rect.x >= 0 && rect.y >= 0 && rect.w >= 0 && rect.h >= 0
            && rect.x <= width - rect.w
            && rect.y <= height - rect.h;
    };

    if (renderer) {
        wm.setContentScale(1.0f);
        wm.setLogicalDesktopSize(1000, 700);
        auto longTitleApp = std::make_unique<ProbeApp>();
        auto* longTitleWindow = wm.createWindow(
            "Editor - an unusually long document title.txt", 420, 100, 300, 240,
            std::move(longTitleApp));
        auto unicodeTitleApp = std::make_unique<ProbeApp>();
        auto* unicodeTitleWindow = wm.createWindow(
            "Editor - caf\xC3\xA9.txt", 120, 100, 300, 240,
            std::move(unicodeTitleApp));
        wm.setLogicalDesktopSize(1000, 700);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        wm.render(renderer);
        int measuredUnicodeTitleWidth = 0;
        int measuredUnicodeTitleHeight = 0;
        const bool measuredUnicodeTitle = font
            && TTF_SizeUTF8(font, unicodeTitleWindow->title.c_str(),
                            &measuredUnicodeTitleWidth, &measuredUnicodeTitleHeight) == 0;
        int cachedUnicodeTitleWidth = 0;
        int cachedUnicodeTitleHeight = 0;
        const auto unicodeCache = wm.m_titleCache.find(unicodeTitleWindow->id);
        const bool cachedUnicodeTitle = unicodeCache != wm.m_titleCache.end()
            && unicodeCache->second.texture
            && SDL_QueryTexture(unicodeCache->second.texture, nullptr, nullptr,
                                &cachedUnicodeTitleWidth, &cachedUnicodeTitleHeight) == 0;
        check(measuredUnicodeTitle && cachedUnicodeTitle
                  && cachedUnicodeTitleWidth == measuredUnicodeTitleWidth
                  && cachedUnicodeTitleHeight == measuredUnicodeTitleHeight,
              "window and taskbar titles render as UTF-8");

        wm.setContentScale(1.25f);
        wm.setLogicalDesktopSize(320, 200);
        const auto scaledTaskbarLayout = wm.computeTaskbarLayout();
        check(scaledTaskbarLayout.startButtonRect.w > 0
                  && scaledTaskbarLayout.startButtonRect.h > 0,
              "scaled taskbar exposes a visible Start button rectangle");
        SDL_Event outsideStartDown{};
        outsideStartDown.type = SDL_MOUSEBUTTONDOWN;
        outsideStartDown.button.button = SDL_BUTTON_LEFT;
        outsideStartDown.button.x = scaledTaskbarLayout.startButtonRect.x
            + scaledTaskbarLayout.startButtonRect.w / 2;
        outsideStartDown.button.y = scaledTaskbarLayout.taskbarRect.y;
        wm.handleEvent(outsideStartDown);
        check(!wm.m_showStartMenu,
              "taskbar input ignores the band outside the drawn Start button");
        SDL_Event outsideStartUp = outsideStartDown;
        outsideStartUp.type = SDL_MOUSEBUTTONUP;
        wm.handleEvent(outsideStartUp);

        wm.invalidateShellHitTargets();
        SDL_Event scaledStartDown = outsideStartDown;
        scaledStartDown.button.y = scaledTaskbarLayout.startButtonRect.y
            + scaledTaskbarLayout.startButtonRect.h / 2;
        wm.handleEvent(scaledStartDown);
        check(wm.m_showStartMenu,
              "pre-render scaled Start clicks use the shared button rectangle");
        SDL_Event scaledStartUp = scaledStartDown;
        scaledStartUp.type = SDL_MOUSEBUTTONUP;
        wm.handleEvent(scaledStartUp);
        wm.setStartMenuVisible(false);
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

        SDL_Event rightArrowDown{};
        rightArrowDown.type = SDL_MOUSEBUTTONDOWN;
        rightArrowDown.button.button = SDL_BUTTON_LEFT;
        rightArrowDown.button.x = wm.m_taskbarRightArrowRect.x + 1;
        rightArrowDown.button.y = wm.m_taskbarRightArrowRect.y + 1;
        wm.handleEvent(rightArrowDown);
        check(wm.m_taskbarEntries.empty()
                  && !wm.m_taskbarNeedsScroll
                  && wm.m_taskbarRightArrowRect.w == 0,
              "taskbar arrow scrolling invalidates stale hit targets");
        SDL_Event rightArrowUp = rightArrowDown;
        rightArrowUp.type = SDL_MOUSEBUTTONUP;
        wm.handleEvent(rightArrowUp);

        wm.render(renderer);
        const int wheelX = wm.m_taskbarButtonAreaLeft + 4;
        const int wheelY = wm.logicalToScreenY(wm.getTaskbarRect().y + 4);
        if (hostWindow) {
            SDL_WarpMouseInWindow(hostWindow, wheelX, wheelY);
            SDL_PumpEvents();
        }
        SDL_Event wheelScroll{};
        wheelScroll.type = SDL_MOUSEWHEEL;
        wheelScroll.wheel.y = -1;
        wm.handleEvent(wheelScroll);
        check(wm.m_taskbarEntries.empty()
                  && !wm.m_taskbarNeedsScroll
                  && wm.m_taskbarRightArrowRect.w == 0,
              "taskbar wheel scrolling invalidates stale hit targets");

        wm.setLogicalDesktopSize(40, 60);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        wm.render(renderer);
        const int outsideStartY = wm.logicalToScreenY(wm.getTaskbarRect().y + 4);
        Uint32 outsideStartPixel = 0;
        SDL_Rect outsideStartSample = {60, outsideStartY, 1, 1};
        const bool readOutsideStart = rgbaFormat
            && SDL_RenderReadPixels(renderer, &outsideStartSample, SDL_PIXELFORMAT_RGBA32,
                                    &outsideStartPixel, sizeof(outsideStartPixel)) == 0;
        Uint8 outsideR = 0, outsideG = 0, outsideB = 0, outsideA = 0;
        if (readOutsideStart) {
            SDL_GetRGBA(outsideStartPixel, rgbaFormat,
                        &outsideR, &outsideG, &outsideB, &outsideA);
        }
        check(readOutsideStart && outsideR == 0 && outsideG == 0 && outsideB == 0,
              "narrow taskbars contain the Start button inside the desktop edge");
        SDL_Event outsideStartClick{};
        outsideStartClick.type = SDL_MOUSEBUTTONDOWN;
        outsideStartClick.button.button = SDL_BUTTON_LEFT;
        outsideStartClick.button.x = 60;
        outsideStartClick.button.y = outsideStartY;
        wm.handleEvent(outsideStartClick);
        check(!wm.m_showStartMenu,
              "clicks beyond a narrow Start button do not open the Start menu");
        SDL_Event outsideStartRelease = outsideStartClick;
        outsideStartRelease.type = SDL_MOUSEBUTTONUP;
        wm.handleEvent(outsideStartRelease);

        wm.m_showStartMenu = true;
        wm.setLogicalDesktopSize(40, 60);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        wm.render(renderer);
        const SDL_Rect tinyMenu = wm.m_startMenuRect;
        check(rectInside(tinyMenu, 40, 60)
                  && tinyMenu.y + tinyMenu.h <= wm.getTaskbarRect().y,
              "narrow Start menus stay inside the usable desktop");
        bool startItemsContained = true;
        for (const auto& item : wm.m_startMenuItems) {
            startItemsContained &= rectInside(item.rect, 40, 60)
                && item.rect.x >= tinyMenu.x
                && item.rect.y >= tinyMenu.y
                && item.rect.x + item.rect.w <= tinyMenu.x + tinyMenu.w
                && item.rect.y + item.rect.h <= tinyMenu.y + tinyMenu.h;
        }
        check(startItemsContained,
              "narrow Start menu hit targets stay inside the visible popup");
        wm.m_showStartMenu = false;

        wm.m_showStartMenu = true;
        wm.m_startMenuItems = {
            {{0, 0, 40, 20}, 9},
            {{0, 20, 40, 20}, 9},
            {{0, 40, 40, 20}, 9}
        };
        wm.m_startMenuKeyboardIndex = -1;
        SDL_Event menuDown{};
        menuDown.type = SDL_KEYDOWN;
        menuDown.key.keysym.sym = SDLK_DOWN;
        wm.handleEvent(menuDown);
        wm.handleEvent(menuDown);
        check(wm.m_startMenuKeyboardIndex == 1,
              "Start menu Down moves the keyboard selection");
        SDL_Event menuUp = menuDown;
        menuUp.key.keysym.sym = SDLK_UP;
        wm.handleEvent(menuUp);
        check(wm.m_startMenuKeyboardIndex == 0,
              "Start menu Up moves the keyboard selection");
        wm.handleEvent(menuUp);
        check(wm.m_startMenuKeyboardIndex == 2,
              "Start menu keyboard selection wraps at the first item");
        SDL_Event menuEscape = menuDown;
        menuEscape.key.keysym.sym = SDLK_ESCAPE;
        wm.handleEvent(menuEscape);
        check(!wm.m_showStartMenu && wm.m_startMenuKeyboardIndex == -1,
              "Start menu Escape closes keyboard navigation");

        wm.m_showStartMenu = true;
        wm.m_startMenuItems = {{{0, 0, 40, 20}, 9}};
        wm.m_startMenuKeyboardIndex = 0;
        SDL_Event menuEnter = menuDown;
        menuEnter.key.keysym.sym = SDLK_RETURN;
        wm.handleEvent(menuEnter);
        check(wm.shouldQuit() && !wm.m_showStartMenu,
              "Start menu Enter activates the selected command");
        wm.m_quitRequested = false;

        wm.setLogicalDesktopSize(500, 40);
        wm.render(renderer);
        const bool clockRenderedOnShortClient =
            wm.m_clockHitRect.w > 0 && wm.m_clockHitRect.h > 0;
        if (clockRenderedOnShortClient) {
            wm.m_mouseX = wm.m_clockHitRect.x + wm.m_clockHitRect.w / 2;
            wm.m_mouseY = wm.m_clockHitRect.y + wm.m_clockHitRect.h / 2;
            wm.render(renderer);
        }
        const SDL_Rect tinyUsableScreen = wm.logicalRectToScreen(wm.getUsableDesktopRect());
        check(clockRenderedOnShortClient
                  && rectInside(wm.m_clockTooltipRect,
                         tinyUsableScreen.x + tinyUsableScreen.w,
                         tinyUsableScreen.y + tinyUsableScreen.h)
                  && wm.m_clockTooltipRect.x >= tinyUsableScreen.x
                  && wm.m_clockTooltipRect.y >= tinyUsableScreen.y,
              "clock date tooltip stays inside the usable desktop on short clients");

        wm.setLogicalDesktopSize(1000, 700);
        wm.m_taskbarScrollOffset = 400;
        wm.setLogicalDesktopSize(120, 120);
        wm.render(renderer);
        check(!wm.m_taskbarNeedsScroll || wm.m_taskbarButtonAreaWidth >= 40,
              "taskbar scrolling is disabled when arrow controls cannot fit");
        check(wm.m_taskbarScrollOffset == 0,
              "shrinking the desktop resets an unusable taskbar scroll offset");

        wm.setLogicalDesktopSize(1000, 700);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        wm.render(renderer);
        check(!wm.m_taskbarEntries.empty(),
              "taskbar render populates screen-space hit targets");
        const bool clock24HourBeforeFormatChange = wm.getClock24Hour();
        wm.setClock24Hour(!clock24HourBeforeFormatChange);
        check(wm.m_taskbarEntries.empty()
                  && wm.m_clockHitRect.w == 0
                  && wm.m_clockTooltipRect.w == 0,
              "clock format changes invalidate stale shell hit targets");
        wm.setClock24Hour(clock24HourBeforeFormatChange);
        wm.setContentScale(1.15f);
        check(wm.m_taskbarEntries.empty()
                  && !wm.m_taskbarNeedsScroll
                  && wm.m_taskbarLeftArrowRect.w == 0
                  && wm.m_taskbarRightArrowRect.w == 0,
              "content scaling invalidates stale taskbar hit targets");
        wm.setUiScalePercent(115);
        check(wm.m_taskbarEntries.empty()
                  && wm.m_clockHitRect.w == 0
                  && wm.m_clockTooltipRect.w == 0,
              "font scaling invalidates stale taskbar and clock hit targets");
        wm.setUiScalePercent(100);
        wm.setLogicalDesktopSize(900, 650);
        check(wm.m_taskbarEntries.empty()
                  && wm.m_startMenuItems.empty()
                  && wm.m_startMenuRect.w == 0,
              "desktop resizing invalidates stale shell hit targets");
        wm.setContentScale(1.0f);
        wm.setLogicalDesktopSize(1000, 700);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        wm.render(renderer);
        auto findTaskbarWidth = [&]() {
            for (const auto& entry : wm.m_taskbarEntries) {
                if (entry.window == longTitleWindow) return entry.rect.w;
            }
            return 0;
        };
        const int normalFontButtonWidth = findTaskbarWidth();
        wm.setUiScalePercent(115);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        SDL_RenderClear(renderer);
        wm.render(renderer);
        check(findTaskbarWidth() > normalFontButtonWidth,
              "taskbar buttons grow with measured UI text width");

        const size_t taskbarEntriesBeforeClose = wm.m_taskbarEntries.size();
        wm.closeWindow(longTitleWindow);
        const bool closedEntryRemains = std::any_of(
            wm.m_taskbarEntries.begin(),
            wm.m_taskbarEntries.end(),
            [longTitleWindow](const auto& entry) {
                return entry.window == longTitleWindow;
            });
        check(taskbarEntriesBeforeClose > 0 && !closedEntryRemains,
              "closing a window invalidates its cached taskbar hit target");
        check(!wm.m_taskbarNeedsScroll
                  && wm.m_taskbarLeftArrowRect.w == 0
                  && wm.m_taskbarRightArrowRect.w == 0,
              "closing a window clears stale taskbar scroll targets");

        wm.render(renderer);
        const size_t entriesBeforeNewWindow = wm.m_taskbarEntries.size();
        auto addedApp = std::make_unique<ProbeApp>();
        auto* addedWindow = wm.createWindow(
            "Added window with a title that changes the taskbar", 120, 100, 300, 240,
            std::move(addedApp));
        check(addedWindow && wm.m_taskbarEntries.empty()
                  && wm.m_taskbarLeftArrowRect.w == 0
                  && wm.m_taskbarRightArrowRect.w == 0
                  && entriesBeforeNewWindow > 0,
              "creating and focusing a window clears stale taskbar targets");
        wm.setWindowTitle(addedWindow, "Renamed taskbar window with a longer title");
        check(wm.m_taskbarEntries.empty()
                  && wm.m_taskbarLeftArrowRect.w == 0
                  && wm.m_taskbarRightArrowRect.w == 0,
              "renaming a window clears stale taskbar targets");

        wm.ensureTaskbarHitTargets();
        SDL_Rect addedTaskbarRect{0, 0, 0, 0};
        for (const auto& entry : wm.m_taskbarEntries) {
            if (entry.window == addedWindow) {
                addedTaskbarRect = entry.rect;
                break;
            }
        }
        wm.invalidateShellHitTargets();
        SDL_Event preRenderTaskbarClick{};
        preRenderTaskbarClick.type = SDL_MOUSEBUTTONDOWN;
        preRenderTaskbarClick.button.button = SDL_BUTTON_LEFT;
        preRenderTaskbarClick.button.x = addedTaskbarRect.x + addedTaskbarRect.w / 2;
        preRenderTaskbarClick.button.y = addedTaskbarRect.y + addedTaskbarRect.h / 2;
        wm.handleEvent(preRenderTaskbarClick);
        check(addedTaskbarRect.w > 0 && addedWindow->minimized,
              "taskbar input rebuilds targets before the next render");
        SDL_Event preRenderTaskbarRelease = preRenderTaskbarClick;
        preRenderTaskbarRelease.type = SDL_MOUSEBUTTONUP;
        wm.handleEvent(preRenderTaskbarRelease);
    }

    if (renderer) {
        monolith::window::WindowManager renderWm;
        renderWm.setFont(font);
        auto survivor = std::make_unique<ProbeApp>();
        ProbeApp* survivorPtr = survivor.get();
        renderWm.createWindow("Survivor", 80, 80, 300, 240, std::move(survivor));

        int closingRenderCalls = 0;
        auto closing = std::make_unique<RenderClosingApp>(&closingRenderCalls);
        RenderClosingApp* closingPtr = closing.get();
        renderWm.createWindow("Closing", 420, 80, 300, 240, std::move(closing));
        closingPtr->closeOnRender = true;

        renderWm.render(renderer);
        check(closingRenderCalls == 1 && renderWm.m_windows.size() == 1,
              "render snapshot survives a front app closing during render");
        check(survivorPtr->renderRect.w > 0,
              "render continues with the surviving window after callback removal");

        monolith::window::WindowManager blendWm;
        blendWm.setLogicalDesktopSize(900, 600);
        auto leakingApp = std::make_unique<ProbeApp>();
        ProbeApp* leakingAppPtr = leakingApp.get();
        leakingAppPtr->leakBlendMode = true;
        blendWm.createWindow("Leaking blend app", 40, 40, 300, 220, std::move(leakingApp));
        auto observingApp = std::make_unique<ProbeApp>();
        ProbeApp* observingAppPtr = observingApp.get();
        observingAppPtr->leakDrawColor = false;
        blendWm.createWindow("Observing blend app", 380, 80, 300, 220, std::move(observingApp));

        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_ADD);
        SDL_SetRenderDrawColor(renderer, 9, 8, 7, 6);
        blendWm.render(renderer);
        SDL_BlendMode afterFrameBlend = SDL_BLENDMODE_NONE;
        SDL_GetRenderDrawBlendMode(renderer, &afterFrameBlend);
        Uint8 afterFrameR = 0, afterFrameG = 0, afterFrameB = 0, afterFrameA = 0;
        SDL_GetRenderDrawColor(renderer, &afterFrameR, &afterFrameG,
                               &afterFrameB, &afterFrameA);
        check(leakingAppPtr->renderBlendMode == SDL_BLENDMODE_NONE
                  && observingAppPtr->renderBlendMode == SDL_BLENDMODE_NONE,
              "WindowManager isolates app blend state during composition");
        check(afterFrameBlend == SDL_BLENDMODE_ADD,
              "WindowManager restores the caller blend mode after composition");
        check(afterFrameR == 9 && afterFrameG == 8 && afterFrameB == 7 && afterFrameA == 6,
              "WindowManager restores the caller draw color after composition");

        monolith::window::WindowManager colorWm;
        colorWm.setLogicalDesktopSize(900, 600);
        auto leakingColorApp = std::make_unique<ProbeApp>();
        ProbeApp* leakingColorAppPtr = leakingColorApp.get();
        leakingColorAppPtr->leakDrawColor = true;
        colorWm.createWindow("Leaking draw color app", 40, 40, 300, 220,
                             std::move(leakingColorApp));
        auto observingColorApp = std::make_unique<ProbeApp>();
        ProbeApp* observingColorAppPtr = observingColorApp.get();
        colorWm.createWindow("Observing draw color app", 380, 80, 300, 220,
                             std::move(observingColorApp));
        colorWm.render(renderer);
        check(leakingColorAppPtr->renderColorR == 45
                  && leakingColorAppPtr->renderColorG == 45
                  && leakingColorAppPtr->renderColorB == 50
                  && leakingColorAppPtr->renderColorA == 255
                  && observingColorAppPtr->renderColorR == 45
                  && observingColorAppPtr->renderColorG == 45
                  && observingColorAppPtr->renderColorB == 50
                  && observingColorAppPtr->renderColorA == 255,
              "WindowManager isolates app draw color state during composition");
    }

    if (font) TTF_CloseFont(font);
    TTF_Quit();
    if (renderer) SDL_DestroyRenderer(renderer);
    if (surface) SDL_FreeSurface(surface);
    if (rgbaFormat) SDL_FreeFormat(rgbaFormat);
    if (hostWindow) SDL_DestroyWindow(hostWindow);
    SDL_Quit();

    if (failures == 0) {
        std::cout << "ALL WINDOW COORDINATE TESTS PASSED\n";
        return 0;
    }
    return 1;
}
