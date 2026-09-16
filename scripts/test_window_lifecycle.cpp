// Headless regression test for app-triggered window closes during update().
#include "../src/window/WindowManager.hpp"

#include <iostream>
#include <memory>

namespace {

class ClosingApp final : public monolith::app::App {
public:
    void render(SDL_Renderer*, const SDL_Rect&) override {}

    void update() override {
        if (auto* controller = getController()) {
            controller->close();
        }
    }
};

class UpdateProbe final : public monolith::app::App {
public:
    void render(SDL_Renderer*, const SDL_Rect&) override {}

    void update() override { ++updates; }

    int updates = 0;
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

    {
        monolith::window::WindowManager wm;
        auto closing = std::make_unique<ClosingApp>();
        wm.createWindow("Closing", 40, 80, 260, 180, std::move(closing));

        auto survivor = std::make_unique<UpdateProbe>();
        UpdateProbe* survivorPtr = survivor.get();
        wm.createWindow("Survivor", 400, 80, 260, 180, std::move(survivor));

        wm.update();

        check(survivorPtr->updates == 1,
              "a survivor still updates after an earlier app closes itself");
        check(wm.getWindowAt(100, 160) == nullptr,
              "a window closed during update is removed safely");
    }

    {
        monolith::window::WindowManager wm;
        auto survivor = std::make_unique<UpdateProbe>();
        UpdateProbe* survivorPtr = survivor.get();
        wm.createWindow("Survivor", 40, 80, 260, 180, std::move(survivor));

        auto closing = std::make_unique<ClosingApp>();
        wm.createWindow("Closing", 400, 80, 260, 180, std::move(closing));

        wm.update();

        check(survivorPtr->updates == 1,
              "the update snapshot also handles the focused app closing itself");
        check(wm.getWindowAt(460, 160) == nullptr,
              "the focused app is gone after its controller close");
    }

    if (failures == 0) {
        std::cout << "ALL WINDOW LIFECYCLE TESTS PASSED\n";
        return 0;
    }
    return 1;
}
