// Headless regression test for focus handoff when the active window is minimized.
#include "../src/fs/Filesystem.hpp"
#define private public
#include "../src/window/WindowManager.hpp"
#undef private

#include <filesystem>
#include <iostream>
#include <memory>
#include <unistd.h>

namespace {

class FocusProbe final : public monolith::app::App {
public:
    void render(SDL_Renderer*, const SDL_Rect&) override {}

    void handleEvent(const SDL_Event& event) override {
        if (event.type == SDL_KEYDOWN) ++keyDowns;
    }

    void onFocusGained() override { ++focusGained; }
    void onFocusLost() override { ++focusLost; }
    void onUiScaleChanged() override { ++uiScaleChanges; }

    int keyDowns = 0;
    int focusGained = 0;
    int focusLost = 0;
    int uiScaleChanges = 0;
};

bool minimizeWindow(monolith::window::WindowManager& wm, monolith::window::Window* window) {
    if (!window) return false;
    const int buttonX = window->rect.x + window->rect.w - 10 - 16 * 3 - 6 * 2 + 4;
    const int buttonY = window->rect.y
        + (monolith::window::Window::TITLE_BAR_HEIGHT - 16) / 2 + 4;
    return wm.handleTitleBarButtons(window, buttonX, buttonY);
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

    const std::filesystem::path hostRoot = std::filesystem::temp_directory_path()
        / ("monolith-window-focus-" + std::to_string(getpid()));
    monolith::fs::Filesystem fs(hostRoot.string());
    check(fs.initialize(), "focus probe filesystem initialize");

    monolith::window::WindowManager wm;
    wm.setAppResources(nullptr, &fs);
    auto first = std::make_unique<FocusProbe>();
    FocusProbe* firstPtr = first.get();
    wm.createWindow("First", 100, 100, 300, 240, std::move(first));

    auto second = std::make_unique<FocusProbe>();
    FocusProbe* secondPtr = second.get();
    auto* secondWindow = wm.createWindow("Second", 500, 100, 300, 240, std::move(second));
    wm.associateEditorWithFile(secondWindow, "/docs/note.txt");

    check(secondPtr->focusGained == 1, "newest window receives focus");

    check(minimizeWindow(wm, secondWindow),
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

    check(wm.focusEditorForFile("/docs/note.txt"),
          "file bridge finds the existing editor");
    check(!secondWindow->minimized, "reopening an editor restores its minimized window");
    wm.handleEvent(key);
    check(secondPtr->keyDowns == 1, "restored editor receives keyboard focus");

    auto third = std::make_unique<FocusProbe>();
    FocusProbe* thirdPtr = third.get();
    auto* thirdWindow = wm.createWindow("Third", 180, 180, 300, 240, std::move(third));
    wm.associateDrawingWithFile(thirdWindow, "/drawings/sketch.modr");
    check(minimizeWindow(wm, thirdWindow), "third window minimize is handled");
    // Simulate a minimized session rectangle that no longer fits the desktop.
    // Reopening the file must make the window visible without putting its
    // title bar under the taskbar or beyond the left edge.
    thirdWindow->rect.x = -500;
    thirdWindow->rect.y = 700;
    check(wm.focusDrawingForFile("/drawings/sketch.modr"),
          "file bridge finds the existing drawing");
    check(!thirdWindow->minimized, "reopening a Drawing restores its minimized window");
    check(thirdWindow->rect.x == 0 && thirdWindow->rect.y == 452,
          "restored Drawing geometry is clamped before it becomes visible");
    wm.handleEvent(key);
    check(thirdPtr->keyDowns == 1, "restored Drawing receives keyboard focus");

    auto restored = std::make_unique<FocusProbe>();
    FocusProbe* restoredPtr = restored.get();
    auto* restoredWindow = wm.createWindow("Restored", 240, 220, 300, 240,
                                           std::move(restored));
    const int restoredFocusLostBefore = restoredPtr->focusLost;
    wm.applyRestoredGeometry(restoredWindow, 240, 220, 300, 240, true, false);
    check(restoredWindow->minimized && wm.m_focusedWindow != restoredWindow,
          "minimized session geometry does not retain keyboard focus");
    check(restoredPtr->focusLost == restoredFocusLostBefore + 1,
          "minimized session geometry sends focus-lost notification");
    check(wm.m_focusedWindow && !wm.m_focusedWindow->minimized,
          "minimized session geometry hands focus to a visible window");

    check(minimizeWindow(wm, secondWindow),
          "minimize editor before shared UI scale change");
    wm.setUiScalePercent(115);
    check(firstPtr->uiScaleChanges == 1 && secondPtr->uiScaleChanges == 1
              && thirdPtr->uiScaleChanges == 1,
          "shared UI scale reaches visible and minimized apps");

    std::error_code ec;
    std::filesystem::remove_all(hostRoot, ec);

    if (failures == 0) {
        std::cout << "ALL WINDOW FOCUS TESTS PASSED\n";
        return 0;
    }
    return 1;
}
