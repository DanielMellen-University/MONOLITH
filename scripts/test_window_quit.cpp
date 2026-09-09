// Headless regression test for the Shut Down dirty-document guard.
#include "../src/window/WindowManager.hpp"

#include <iostream>
#include <memory>

namespace {

class QuitProbe final : public monolith::app::App {
public:
    explicit QuitProbe(bool dirty) : m_dirty(dirty) {}

    void render(SDL_Renderer*, const SDL_Rect&) override {}

    bool allowClose() override {
        ++allowCloseCalls;
        if (!m_dirty) return true;
        if (!m_confirmed) {
            m_confirmed = true;
            return false;
        }
        return true;
    }

    int allowCloseCalls = 0;

private:
    bool m_dirty = false;
    bool m_confirmed = false;
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
        auto clean = std::make_unique<QuitProbe>(false);
        QuitProbe* cleanPtr = clean.get();
        wm.createWindow("Clean", 40, 40, 260, 180, std::move(clean));
        wm.requestQuit();
        check(wm.shouldQuit(), "clean shutdown is accepted immediately");
        check(cleanPtr->allowCloseCalls == 1, "clean shutdown checks the app close contract");
    }

    {
        monolith::window::WindowManager wm;
        auto dirty = std::make_unique<QuitProbe>(true);
        QuitProbe* dirtyPtr = dirty.get();
        wm.createWindow("Dirty", 40, 40, 260, 180, std::move(dirty));

        wm.requestQuit();
        check(!wm.shouldQuit(), "dirty shutdown is blocked on the first request");
        check(dirtyPtr->allowCloseCalls == 1, "dirty shutdown arms the app close guard");

        wm.requestQuit();
        check(wm.shouldQuit(), "confirmed dirty shutdown is accepted");
        check(dirtyPtr->allowCloseCalls == 2, "confirmed shutdown checks the app a second time");
    }

    {
        monolith::window::WindowManager wm;
        auto first = std::make_unique<QuitProbe>(true);
        QuitProbe* firstPtr = first.get();
        wm.createWindow("First dirty", 40, 40, 260, 180, std::move(first));

        auto second = std::make_unique<QuitProbe>(true);
        QuitProbe* secondPtr = second.get();
        wm.createWindow("Second dirty", 340, 40, 260, 180, std::move(second));

        wm.requestQuit();
        check(!wm.shouldQuit(), "multiple dirty shutdown is blocked on the first request");
        check(firstPtr->allowCloseCalls == 1 && secondPtr->allowCloseCalls == 1,
              "first shutdown request arms every dirty app");

        wm.requestQuit();
        check(wm.shouldQuit(), "multiple dirty shutdown succeeds on the second request");
        check(firstPtr->allowCloseCalls == 2 && secondPtr->allowCloseCalls == 2,
              "second shutdown request confirms every dirty app");
    }

    {
        monolith::window::WindowManager wm;
        auto dirty = std::make_unique<QuitProbe>(true);
        QuitProbe* dirtyPtr = dirty.get();
        wm.createWindow("Native close", 40, 40, 260, 180, std::move(dirty));

        SDL_Event quit{};
        quit.type = SDL_QUIT;
        wm.handleEvent(quit);
        check(!wm.shouldQuit(), "native window close is blocked by a dirty document");
        check(dirtyPtr->allowCloseCalls == 1,
              "native window close uses the app close contract");

        wm.handleEvent(quit);
        check(wm.shouldQuit(), "confirmed native window close is accepted");
        check(dirtyPtr->allowCloseCalls == 2,
              "confirmed native close checks the app a second time");
    }

    if (failures == 0) {
        std::cout << "ALL WINDOW QUIT TESTS PASSED\n";
        return 0;
    }
    return 1;
}
