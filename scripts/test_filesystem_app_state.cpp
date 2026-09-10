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

    const std::filesystem::path danglingPath = hostRoot / "home/monolith/dangling";
    std::filesystem::create_symlink(hostRoot / "missing-browser-target", danglingPath, ec);
    check(!ec, "create browser dangling symlink");
    browser.refreshEntries();
    check(browser.selectEntryNamed("dangling", false),
          "select dangling browser entry");
    browser.openFileEntry("dangling");
    check(browser.m_statusMessage == "Open failed: not a regular file",
          "browser rejects a dangling entry before shell open routing");
    check(fs.removeRecursive("/home/monolith/dangling"),
          "remove browser dangling test entry");
    browser.refreshEntries();

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
    key(browser, SDLK_ESCAPE);
    check(browser.m_entries.size() == 15, "clear the filter before delete selection coverage");

    check(browser.selectEntryNamed("a.txt", false), "select first item for multi-selection refresh");
    int cIndex = -1;
    for (size_t i = 0; i < browser.m_entries.size(); ++i) {
        if (browser.m_entries[i].name == "c.txt") {
            cIndex = static_cast<int>(i);
            break;
        }
    }
    check(cIndex >= 0, "find second multi-selection item");
    if (cIndex >= 0) {
        browser.setSelection(cIndex, true);
    }
    check(browser.selectedIndicesSorted().size() == 2,
          "multi-selection contains both selected items before refresh");
    key(browser, SDLK_f, KMOD_CTRL);
    text(browser, "txt");
    check(browser.selectedIndicesSorted().size() == 2
              && browser.m_selectedIndex >= 0
              && browser.m_entries[static_cast<size_t>(browser.m_selectedIndex)].name == "c.txt",
          "filter refresh preserves the visible multi-selection and primary item");
    key(browser, SDLK_ESCAPE);
    check(browser.selectedIndicesSorted().size() == 2,
          "clearing a filter preserves the visible multi-selection");

    check(browser.selectEntryNamed("a.txt", false), "select a single entry for transient state reset");
    browser.startRenameSelected();
    check(browser.m_renaming, "rename mode is active before directory change");
    browser.m_showContextMenu = true;
    browser.m_contextMenuTarget = 0;
    browser.m_contextMenuItems = {"Open", "Rename"};
    browser.m_confirmingDelete = true;
    browser.m_pendingDeleteIndex = 0;
    browser.setCurrentPath("/home/monolith");
    check(!browser.m_renaming && !browser.m_showContextMenu
              && browser.m_contextMenuItems.empty()
              && !browser.m_confirmingDelete,
          "changing directories clears transient rename and context-menu state");
    browser.m_showContextMenu = true;
    browser.m_contextMenuItems = {"Refresh"};
    browser.m_contextMenuTarget = 0;
    browser.refreshEntries();
    check(!browser.m_showContextMenu && browser.m_contextMenuItems.empty(),
          "refresh clears a stale context menu target");

    check(browser.selectEntryNamed("a.txt", false), "select an item before filtered delete");
    browser.requestDeleteSelected();
    key(browser, SDLK_f, KMOD_CTRL);
    text(browser, "does-not-exist");
    check(!browser.m_confirmingDelete && browser.m_selectedIndex == -1,
          "filtering away a delete target cancels its confirmation");
    key(browser, SDLK_ESCAPE);

    check(browser.selectEntryNamed("a.txt", false), "select an item before delete confirmation");
    browser.requestDeleteSelected();
    check(browser.m_confirmingDelete, "delete confirmation arms for the selected item");
    int bIndex = -1;
    for (size_t i = 0; i < browser.m_entries.size(); ++i) {
        if (browser.m_entries[i].name == "b.txt") {
            bIndex = static_cast<int>(i);
            break;
        }
    }
    check(bIndex >= 0, "find the alternate selection target");
    if (bIndex >= 0) {
        browser.toggleSelection(bIndex);
        check(!browser.m_confirmingDelete,
              "Ctrl-style selection changes cancel delete confirmation");
        browser.selectEntryNamed("a.txt", false);
        browser.requestDeleteSelected();
        browser.selectRange(0, bIndex);
        check(!browser.m_confirmingDelete,
              "range selection changes cancel delete confirmation");
    }
    browser.cancelPendingDelete();

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

    check(fs.createDirectory("/home/monolith/dest/sub"),
          "create browser folder move source");
    check(fs.writeFile("/home/monolith/dest/sub/inside.txt", "inside"),
          "write browser folder move child");
    browser.setCurrentPath("/home/monolith/dest/sub");
    check(browser.m_currentPath == "/home/monolith/dest/sub",
          "open browser folder move source");
    check(fs.rename("/home/monolith/dest/sub", "/home/monolith/moved-sub"),
          "move browser current folder");
    browser.onVirtualPathMoved("/home/monolith/dest/sub", "/home/monolith/moved-sub");
    check(browser.m_currentPath == "/home/monolith/moved-sub"
              && browser.m_entries.size() == 1
              && browser.m_entries.front().name == "inside.txt",
          "browser view follows a moved parent directory");

    check(fs.createDirectory("/home/monolith/moved-sub/to-delete"),
          "create browser deletion source");
    browser.setCurrentPath("/home/monolith/moved-sub/to-delete");
    check(fs.removeRecursive("/home/monolith/moved-sub/to-delete"),
          "remove browser current folder");
    browser.onVirtualPathRemoved("/home/monolith/moved-sub/to-delete");
    check(browser.m_currentPath == "/home/monolith/moved-sub",
          "browser view returns to a valid parent after deletion");

    std::filesystem::remove_all(hostRoot, ec);
    if (failures == 0) {
        std::cout << "ALL FILESYSTEM APP STATE TESTS PASSED\n";
        return 0;
    }
    std::cerr << failures << " test(s) failed\n";
    return 1;
}
