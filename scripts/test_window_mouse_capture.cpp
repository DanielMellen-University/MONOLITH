#include "../src/window/WindowManager.hpp"

#include <iostream>
#include <memory>

namespace {

class CaptureApp final : public monolith::app::App {
public:
    void render(SDL_Renderer*, const SDL_Rect&) override {}

    void handleEvent(const SDL_Event& event) override {
        if (event.type == SDL_MOUSEBUTTONDOWN) {
            ++downs;
            downX = event.button.x;
            downY = event.button.y;
        } else if (event.type == SDL_MOUSEBUTTONUP) {
            ++ups;
            upX = event.button.x;
            upY = event.button.y;
        } else if (event.type == SDL_MOUSEMOTION) {
            ++motions;
            motionX = event.motion.x;
            motionY = event.motion.y;
        }
    }

    int downs = 0;
    int ups = 0;
    int motions = 0;
    int downX = -1;
    int downY = -1;
    int upX = -1;
    int upY = -1;
    int motionX = -1;
    int motionY = -1;
};

void leftButton(SDL_Event& event, Uint32 type, int x, int y) {
    event = {};
    event.type = type;
    event.button.button = SDL_BUTTON_LEFT;
    event.button.x = x;
    event.button.y = y;
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

    monolith::window::WindowManager wm;
    wm.setLogicalDesktopSize(1000, 700);

    auto first = std::make_unique<CaptureApp>();
    CaptureApp* firstPtr = first.get();
    wm.createWindow("First", 100, 100, 300, 240, std::move(first));

    auto second = std::make_unique<CaptureApp>();
    CaptureApp* secondPtr = second.get();
    wm.createWindow("Second", 500, 100, 300, 240, std::move(second));

    SDL_Event down{};
    leftButton(down, SDL_MOUSEBUTTONDOWN, 120, 150);
    wm.handleEvent(down);
    check(firstPtr->downs == 1 && firstPtr->downX == 20 && firstPtr->downY == 18,
          "first client receives translated button down");

    SDL_Event motion{};
    motion.type = SDL_MOUSEMOTION;
    motion.motion.x = 900;
    motion.motion.y = 650;
    wm.handleEvent(motion);
    check(firstPtr->motions == 1 && firstPtr->motionX == 800 && firstPtr->motionY == 518,
          "captured client receives motion outside its window");

    SDL_Event secondDown{};
    leftButton(secondDown, SDL_MOUSEBUTTONDOWN, 520, 150);
    wm.handleEvent(secondDown);
    check(secondPtr->downs == 1, "focus can change while the button is held");

    SDL_Event up{};
    leftButton(up, SDL_MOUSEBUTTONUP, 520, 150);
    wm.handleEvent(up);
    check(firstPtr->ups == 1 && firstPtr->upX == 420 && firstPtr->upY == 18,
          "release returns to the original client after focus changes");
    check(secondPtr->ups == 0, "focused replacement does not steal the release");

    if (failures == 0) {
        std::cout << "ALL WINDOW MOUSE CAPTURE TESTS PASSED\n";
        return 0;
    }
    return 1;
}
