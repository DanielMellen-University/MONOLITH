// Headless regression test for focus handoff when the active window is minimized.
#include "../src/window/WindowManager.hpp"

#include <iostream>
#include <memory>

namespace {

class FocusProbe final : public monolith::app::App {
public:
    void render(SDL_Renderer*, const SDL_Rect&) override {}

    void handleEvent(const SDL_Event& event) override {
        if (event.type == SDL_KEYDOWN) ++keyDowns;
    }

    void onFocusGained() override { ++focusGained; }
    void onFocusLost() override { ++focusLost; }

    int keyDowns = 0;
    int focusGained = 0;
    int focusLost = 0;
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
    auto first = std::make_unique<FocusProbe>();
    FocusProbe* firstPtr = first.get();
    wm.createWindow("First", 100, 100, 300, 240, std::move(first));

    auto second = std::make_unique<FocusProbe>();
    FocusProbe* secondPtr = second.get();
    auto* secondWindow = wm.createWindow("Second", 500, 100, 300, 240, std::move(second));

    check(secondPtr->focusGained == 1, "newest window receives focus");

    // WindowManager title-button geometry is fixed: size 16, spacing 6, right padding 10.
    const int buttonX = secondWindow->rect.x + secondWindow->rect.w - 10 - 16 * 3 - 6 * 2 + 4;
    const int buttonY = secondWindow->rect.y + (monolith::window::Window::TITLE_BAR_HEIGHT - 16) / 2 + 4;
    check(wm.handleTitleBarButtons(secondWindow, buttonX, buttonY),
          "minimize title button is handled");
    check(secondWindow->minimized, "active window becomes minimized");

    SDL_Event key{};
    key.type = SDL_KEYDOWN;
    key.key.keysym.sym = SDLK_a;
    wm.handleEvent(key);
    check(firstPtr->keyDowns == 1, "keyboard focus moves to visible survivor");
    check(secondPtr->keyDowns == 0, "minimized window receives no hidden key input");
    check(secondPtr->focusLost == 1, "minimized window receives focus-lost notification");
    check(firstPtr->focusGained == 2, "visible survivor receives focus-gained notification");

    if (failures == 0) {
        std::cout << "ALL WINDOW FOCUS TESTS PASSED\n";
        return 0;
    }
    return 1;
}
