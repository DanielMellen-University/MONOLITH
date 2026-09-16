// Headless regression test for app-triggered window closes during update().
#define private public
#include "../src/window/WindowManager.hpp"
#undef private
#include "../src/fs/Filesystem.hpp"

#include <filesystem>
#include <iostream>
#include <memory>
#include <unistd.h>

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

class NotificationClosingApp final : public monolith::app::App {
public:
    void render(SDL_Renderer*, const SDL_Rect&) override {}

    void onVirtualPathCreated(const std::string&) override {
        if (auto* controller = getController()) {
            controller->close();
        }
    }
};

class NotificationProbe final : public monolith::app::App {
public:
    void render(SDL_Renderer*, const SDL_Rect&) override {}

    void onVirtualPathCreated(const std::string&) override { ++notifications; }

    int notifications = 0;
};

class ResizeClosingApp final : public monolith::app::App {
public:
    explicit ResizeClosingApp(bool closeOnResize = false)
        : closeOnResize(closeOnResize) {}

    void render(SDL_Renderer*, const SDL_Rect&) override {}

    void onResize(int, int) override {
        if (closeOnResize) {
            if (auto* controller = getController()) {
                controller->close();
            }
        }
    }

    bool closeOnResize = false;
};

class ActivationClosingApp final : public monolith::app::App {
public:
    void render(SDL_Renderer*, const SDL_Rect&) override {}

    void onFocusGained() override {
        ++focusGained;
        if (closeOnFocusGained) {
            closeOnFocusGained = false;
            if (auto* controller = getController()) {
                controller->close();
            }
        }
    }

    bool closeOnFocusGained = false;
    int focusGained = 0;
};

class ActivationProbe final : public monolith::app::App {
public:
    void render(SDL_Renderer*, const SDL_Rect&) override {}

    void onFocusGained() override { ++focusGained; }

    void handleEvent(const SDL_Event& event) override {
        if (event.type == SDL_KEYDOWN) ++keyDowns;
    }

    int focusGained = 0;
    int keyDowns = 0;
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

    {
        const std::filesystem::path hostRoot = std::filesystem::temp_directory_path()
            / ("monolith-window-notification-" + std::to_string(getpid()));
        monolith::fs::Filesystem fs(hostRoot.string());
        check(fs.initialize(), "notification probe filesystem initialize");

        monolith::window::WindowManager wm;
        wm.setAppResources(nullptr, &fs);
        auto closing = std::make_unique<NotificationClosingApp>();
        wm.createWindow("Closing", 40, 80, 260, 180, std::move(closing));

        auto survivor = std::make_unique<NotificationProbe>();
        NotificationProbe* survivorPtr = survivor.get();
        wm.createWindow("Survivor", 400, 80, 260, 180, std::move(survivor));

        wm.notifyVirtualPathCreated("/home/monolith/created.txt");

        check(survivorPtr->notifications == 1,
              "a survivor receives a path notification after an earlier app closes");
        check(wm.getWindowAt(100, 160) == nullptr,
              "notification-triggered close removes the source window safely");

        std::error_code ec;
        std::filesystem::remove_all(hostRoot, ec);
    }

    {
        monolith::window::WindowManager wm;
        auto closing = std::make_unique<ResizeClosingApp>();
        ResizeClosingApp* closingPtr = closing.get();
        wm.createWindow("Closing", 400, 400, 260, 180, std::move(closing));
        closingPtr->closeOnResize = true;

        wm.setLogicalDesktopSize(500, 400);

        check(wm.getWindowAt(300, 250) == nullptr,
              "a window closed from logical resize is removed safely");
    }

    {
        monolith::window::WindowManager wm;
        auto closing = std::make_unique<ResizeClosingApp>();
        ResizeClosingApp* closingPtr = closing.get();
        auto* closingWindow = wm.createWindow("Closing", 40, 80, 260, 180,
                                               std::move(closing));
        closingPtr->closeOnResize = true;
        wm.applyRestoredGeometry(closingWindow, 120, 140, 300, 220, false, false);

        check(wm.getWindowAt(120, 160) == nullptr,
              "session restore removes a window closed from its resize callback");
    }

    {
        monolith::window::WindowManager wm;
        auto closing = std::make_unique<ResizeClosingApp>();
        ResizeClosingApp* closingPtr = closing.get();
        auto* window = wm.createWindow("Closing", 100, 100, 260, 180, std::move(closing));
        window->maximized = true;
        closingPtr->closeOnResize = true;

        wm.update();

        check(wm.getWindowAt(100, 100) == nullptr,
              "a maximized window closed from update resize is removed safely");
    }

    {
        monolith::window::WindowManager wm;
        auto closing = std::make_unique<ResizeClosingApp>(true);
        auto* created = wm.createWindow("Closing", 40, 80, 260, 180,
                                       std::move(closing));

        check(created == nullptr && wm.getWindowAt(60, 140) == nullptr,
              "initial resize callback can close a window before creation returns");
    }

    {
        monolith::window::WindowManager wm;
        auto closing = std::make_unique<ActivationClosingApp>();
        ActivationClosingApp* closingPtr = closing.get();
        auto* closingWindow = wm.createWindow("Closing", 40, 80, 260, 180,
                                               std::move(closing));

        auto survivor = std::make_unique<ActivationProbe>();
        ActivationProbe* survivorPtr = survivor.get();
        wm.createWindow("Survivor", 400, 80, 260, 180, std::move(survivor));

        closingPtr->closeOnFocusGained = true;
        SDL_Event down{};
        leftButton(down, SDL_MOUSEBUTTONDOWN, closingWindow->rect.x + 20,
                   closingWindow->rect.y + monolith::window::Window::TITLE_BAR_HEIGHT + 20);
        wm.handleEvent(down);

        check(wm.getWindowAt(60, 140) == nullptr,
              "click activation removes a window closed from focus gained");

        SDL_Event key{};
        key.type = SDL_KEYDOWN;
        key.key.keysym.sym = SDLK_a;
        wm.handleEvent(key);
        check(survivorPtr->keyDowns == 1,
              "click activation keeps keyboard focus on the surviving window");
    }

    {
        monolith::window::WindowManager wm;
        auto closing = std::make_unique<ResizeClosingApp>();
        ResizeClosingApp* closingPtr = closing.get();
        auto* closingWindow = wm.createWindow("Closing", 40, 80, 260, 180,
                                               std::move(closing));
        auto survivor = std::make_unique<UpdateProbe>();
        wm.createWindow("Survivor", 400, 80, 260, 180, std::move(survivor));

        closingWindow->rect.y = 650;
        closingPtr->closeOnResize = true;
        SDL_Event down{};
        leftButton(down, SDL_MOUSEBUTTONDOWN, 60, 690);
        wm.handleEvent(down);

        check(wm.getWindowAt(60, 690) == nullptr,
              "resize callback close removes the target before activation continues");
    }

    if (failures == 0) {
        std::cout << "ALL WINDOW LIFECYCLE TESTS PASSED\n";
        return 0;
    }
    return 1;
}
