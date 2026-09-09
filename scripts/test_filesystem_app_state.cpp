// Headless regression test for Filesystem Browser selection after filtering.

#include "../src/fs/Filesystem.hpp"

#include <SDL2/SDL.h>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <string>
#include <unistd.h>

#define private public
#include "../src/app/FilesystemApp.hpp"
#undef private

namespace {

void key(monolith::app::FilesystemApp& app, SDL_Keycode sym, SDL_Keymod mod = KMOD_NONE) {
    SDL_Event event{};
    event.type = SDL_KEYDOWN;
    event.key.keysym.sym = sym;
    event.key.keysym.mod = mod;
    app.handleEvent(event);
}

void text(monolith::app::FilesystemApp& app, const char* value) {
    SDL_Event event{};
    event.type = SDL_TEXTINPUT;
    std::snprintf(event.text.text, sizeof(event.text.text), "%s", value);
    app.handleEvent(event);
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
        / ("monolith-filesystem-app-state-" + std::to_string(getpid()));
    std::error_code ec;
    std::filesystem::remove_all(hostRoot, ec);

    monolith::fs::Filesystem fs(hostRoot.string());
    check(fs.initialize(), "browser state filesystem initialize");
    fs.createDirectory("/home/monolith");
    fs.writeFile("/home/monolith/a.txt", "a");
    fs.writeFile("/home/monolith/b.txt", "b");
    fs.writeFile("/home/monolith/c.txt", "c");
    for (int i = 0; i < 12; ++i) {
        fs.writeFile("/home/monolith/note_" + std::to_string(i) + ".txt", "note");
    }

    monolith::app::FilesystemApp browser(nullptr, &fs);
    browser.onResize(400, 240);
    check(browser.m_entries.size() == 15, "browser loads the complete directory listing");

    browser.setSelection(static_cast<int>(browser.m_entries.size()) - 1);
    browser.m_scrollOffset = 8;
    key(browser, SDLK_f, KMOD_CTRL);
    text(browser, "a");
    check(browser.m_entries.size() == 1 && browser.m_entries.front().name == "a.txt",
          "filter leaves only the matching entry");
    check(browser.m_selectedIndex == -1 && browser.m_selectedSet.empty(),
          "filter clears a selection that is no longer present");
    check(browser.m_scrollOffset == 0, "filter clamps scrolling to the reduced result set");

    key(browser, SDLK_ESCAPE);
    check(browser.m_entries.size() == 15, "escape restores the full listing");
    check(browser.selectEntryNamed("b.txt", false), "select an entry before narrowing again");
    key(browser, SDLK_f, KMOD_CTRL);
    text(browser, "b");
    check(browser.m_selectedIndex == 0 && browser.m_entries.front().name == "b.txt",
          "filter restores the selected entry by name");

    check(fs.writeFile("/home/monolith/move_a.txt", "a"), "create first cut source");
    check(fs.writeFile("/home/monolith/move_b.txt", "b"), "create second cut source");
    check(fs.createDirectory("/home/monolith/dest"), "create cut destination");
    check(fs.writeFile("/home/monolith/dest/move_a.txt", "existing"),
          "create cut destination conflict");
    browser.setCurrentPath("/home/monolith");
    check(browser.selectEntryNamed("move_a.txt", false), "select first cut source");
    int moveBIndex = -1;
    for (size_t i = 0; i < browser.m_entries.size(); ++i) {
        if (browser.m_entries[i].name == "move_b.txt") {
            moveBIndex = static_cast<int>(i);
            break;
        }
    }
    check(moveBIndex >= 0, "find second cut source");
    if (moveBIndex >= 0) {
        browser.setSelection(moveBIndex, true);
    }
    browser.copySelectedToClipboard(true);
    browser.setCurrentPath("/home/monolith/dest");
    browser.pasteFromClipboard();
    check(!fs.exists("/home/monolith/move_b.txt")
              && fs.readFile("/home/monolith/dest/move_b.txt") == "b",
          "partial cut moves the non-conflicting source");
    check(fs.exists("/home/monolith/move_a.txt"),
          "partial cut leaves the conflicting source in place");
    check(browser.m_clipboardPaths.size() == 1
              && browser.m_clipboardPaths.front() == "/home/monolith/move_a.txt"
              && browser.m_clipboardIsCut,
          "partial cut clipboard keeps only the retryable source");

    std::filesystem::remove_all(hostRoot, ec);
    if (failures == 0) {
        std::cout << "ALL FILESYSTEM APP STATE TESTS PASSED\n";
        return 0;
    }
    std::cerr << failures << " test(s) failed\n";
    return 1;
}
