#include "../src/window/WindowManager.hpp"

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
    check(probePtr->resizeCalls == 2
              && probePtr->lastResizeWidth == window->rect.w
              && probePtr->lastResizeHeight == window->rect.h - monolith::window::Window::TITLE_BAR_HEIGHT,
          "desktop clamping notifies the app of its changed client size");

    window->rect = {0, 0, 300, 240};
    window->minimized = true;
    const int resizeCallsBeforeRestore = probePtr->resizeCalls;
    SDL_Event altTab{};
    altTab.type = SDL_KEYDOWN;
    altTab.key.keysym.sym = SDLK_TAB;
    altTab.key.keysym.mod = KMOD_ALT;
    wm.handleEvent(altTab);
    check(!window->minimized && probePtr->resizeCalls == resizeCallsBeforeRestore + 1,
          "restoring a minimized clamped window notifies the app once");

    wm.setLogicalDesktopSize(120, 20);
    check(window->rect.y == 0,
          "undersized logical desktops keep the window origin non-negative");

    if (failures == 0) {
        std::cout << "ALL WINDOW COORDINATE TESTS PASSED\n";
        return 0;
    }
    return 1;
}
