// Headless regression test for the Shut Down dirty-document guard.
#include "../src/fs/Filesystem.hpp"
#define private public
#include "../src/window/WindowManager.hpp"
#undef private

#include <SDL2/SDL_ttf.h>

#include <filesystem>
#include <iostream>
#include <memory>
#include <unistd.h>

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

class QuitOpeningApp final : public monolith::app::App {
public:
    void render(SDL_Renderer*, const SDL_Rect&) override {}

    bool allowClose() override {
        ++allowCloseCalls;
        if (!openedWindow) {
            openedWindow = true;
            if (auto* controller = getController()) {
                controller->openPath("/docs/shutdown-opened.txt");
            }
        }
        return false;
    }

    int allowCloseCalls = 0;

private:
    bool openedWindow = false;
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

    {
        const std::filesystem::path hostRoot = std::filesystem::temp_directory_path()
            / ("monolith-window-quit-snapshot-" + std::to_string(getpid()));
        std::error_code ec;
        std::filesystem::remove_all(hostRoot, ec);
        monolith::fs::Filesystem fs(hostRoot.string());
        check(fs.initialize(), "shutdown snapshot filesystem initialize");
        check(fs.createDirectory("/docs"), "shutdown snapshot creates editor directory");
        check(fs.writeFile("/docs/shutdown-opened.txt", "opened during shutdown"),
              "shutdown snapshot writes editor source");

        check(TTF_Init() == 0, "shutdown snapshot SDL_ttf initialize");
        TTF_Font* font = TTF_OpenFont("assets/fonts/DejaVuSans.ttf", 14);
        check(font != nullptr, "shutdown snapshot loads headless font");
        if (font) {
            monolith::window::WindowManager wm;
            wm.setAppResources(font, &fs);

            auto opener = std::make_unique<QuitOpeningApp>();
            QuitOpeningApp* openerPtr = opener.get();
            wm.createWindow("Opener", 40, 40, 260, 180, std::move(opener));

            auto dirty = std::make_unique<QuitProbe>(true);
            QuitProbe* dirtyPtr = dirty.get();
            wm.createWindow("Dirty", 340, 40, 260, 180, std::move(dirty));

            wm.requestQuit();
            check(!wm.shouldQuit(),
                  "shutdown remains blocked after a callback opens another window");
            check(openerPtr->allowCloseCalls == 1 && dirtyPtr->allowCloseCalls == 1,
                  "shutdown snapshot still checks every original app");
            check(wm.m_windows.size() == 3 && wm.focusEditorForFile("/docs/shutdown-opened.txt"),
                  "shutdown callback-created editor remains registered");

            TTF_CloseFont(font);
        }
        TTF_Quit();
        std::filesystem::remove_all(hostRoot, ec);
    }

    if (failures == 0) {
        std::cout << "ALL WINDOW QUIT TESTS PASSED\n";
        return 0;
    }
    return 1;
}
