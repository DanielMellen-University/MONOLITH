// Headless regression test for Terminal filesystem command results.

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include <filesystem>
#include <iostream>
#include <string>
#include <unistd.h>

#include "../src/app/App.hpp"
#include "../src/fs/Filesystem.hpp"

#define private public
#include "../src/app/TerminalApp.hpp"
#undef private

namespace {

struct TestController final : monolith::app::IWindowController {
    std::vector<std::string> createdPaths;
    std::vector<std::string> changedPaths;

    void close() override {}
    void setTitle(const std::string&) override {}

    void notifyVirtualPathCreated(const std::string& path) override {
        createdPaths.push_back(path);
    }

    void notifyVirtualPathChanged(const std::string& path) override {
        changedPaths.push_back(path);
    }
};

struct TestTerminal final : monolith::app::TerminalApp {
    using monolith::app::App::setController;

    TestTerminal(TTF_Font* font, monolith::fs::Filesystem* fs)
        : TerminalApp(font, fs) {}
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

    const std::filesystem::path hostRoot = std::filesystem::temp_directory_path()
        / ("monolith-terminal-fs-state-" + std::to_string(getpid()));
    std::error_code ec;
    std::filesystem::remove_all(hostRoot, ec);

    monolith::fs::Filesystem fs(hostRoot.string());
    check(fs.initialize(), "terminal filesystem initialize");
    check(fs.createDirectory("/home/monolith"), "create terminal home directory");
    check(fs.writeFile("/home/monolith/.terminal_history", "echo first\r\necho second\r\n"),
          "write CRLF terminal history");
    check(fs.createDirectory("/home/monolith/empty"), "create empty directory");
    check(fs.writeFile("/home/monolith/note.txt", "hello"), "create regular file");
    check(fs.writeFile("/home/monolith/line-endings.txt", "first\r\nsecond\rthird\r"),
          "create mixed line-ending file");
    check(fs.writeFile("/home/monolith/my  file.txt", "exact spacing"),
          "create file with repeated spaces");
    check(fs.createDirectory("/home/monolith/quoted dir"),
          "create directory for quoted completion");

    check(TTF_Init() == 0, "terminal state SDL_ttf initialize");
    TTF_Font* font = TTF_OpenFont("assets/fonts/DejaVuSans.ttf", 14);
    check(font != nullptr, "terminal state loads test font");
    if (!font) {
        TTF_Quit();
        std::filesystem::remove_all(hostRoot, ec);
        return 1;
    }

    TestTerminal terminal(font, &fs);
    TestController controller;
    terminal.setController(&controller);
    check(terminal.m_commandHistory == std::vector<std::string>{"echo first", "echo second"},
          "CRLF history entries lose their carriage returns");

    auto key = [&](SDL_Keycode sym, SDL_Keymod mod = KMOD_NONE) {
        SDL_Keysym keysym{};
        keysym.sym = sym;
        keysym.mod = mod;
        terminal.handleKeyDown(keysym);
    };
    auto text = [&](const char* value) {
        terminal.processTextInput(value);
    };

    terminal.m_commandHistory = {"echo one", "echo two", "echo three"};
    terminal.m_inputBuffer.clear();
    terminal.m_inputCursorPos = 0;
    key(SDLK_UP);
    check(terminal.m_historyIndex == 2 && terminal.m_inputBuffer == "echo three",
          "history navigation recalls the newest command");
    key(SDLK_r, KMOD_CTRL);
    text("two");
    key(SDLK_RETURN);
    check(terminal.m_inputBuffer == "echo two" && terminal.m_historyIndex == -1,
          "accepted reverse search clears stale history navigation");
    key(SDLK_DOWN);
    check(terminal.m_inputBuffer == "echo two",
          "down does not replace an accepted search result with stale input");

    terminal.m_inputBuffer = "echo saved command";
    terminal.m_inputCursorPos = 5;
    key(SDLK_r, KMOD_CTRL);
    text("no-match");
    key(SDLK_ESCAPE);
    check(terminal.m_inputBuffer == "echo saved command" && terminal.m_inputCursorPos == 5,
          "canceling reverse search restores the original input caret");

    terminal.executeCommand("ls /home/monolith/empty");
    check(!terminal.m_history.empty() && terminal.m_history.back() == "(empty)",
          "ls reports an empty directory");

    terminal.executeCommand("ls /home/monolith/note.txt");
    check(!terminal.m_history.empty() && terminal.m_history.back() == "• note.txt",
          "ls reports a regular file");

    terminal.executeCommand("ls /home/monolith/missing");
    check(!terminal.m_history.empty()
              && terminal.m_history.back() == "ls: /home/monolith/missing: No such file or directory",
          "ls reports a missing path");

    terminal.executeCommand("mkdir /home/monolith/created-dir");
    check(!controller.createdPaths.empty()
              && controller.createdPaths.back() == "/home/monolith/created-dir",
          "mkdir notifies the shell about a created directory");
    terminal.executeCommand("touch /home/monolith/created.txt");
    check(!controller.createdPaths.empty()
              && controller.createdPaths.back() == "/home/monolith/created.txt",
          "touch notifies the shell about a created file");
    terminal.executeCommand("cp /home/monolith/note.txt /home/monolith/copied.txt");
    check(!controller.createdPaths.empty()
              && controller.createdPaths.back() == "/home/monolith/copied.txt",
          "cp notifies the shell about a copied file");
    terminal.executeCommand("cp /home/monolith/note.txt /home/monolith/copied.txt");
    check(!controller.changedPaths.empty()
              && controller.changedPaths.back() == "/home/monolith/copied.txt",
          "cp notifies the shell about an overwritten file");

    terminal.m_history.clear();
    terminal.executeCommand("cat /home/monolith/line-endings.txt");
    check(terminal.m_history == std::vector<std::string>{"first", "second", "third", ""},
          "cat normalizes CRLF and lone-CR line endings");

    terminal.m_history.clear();
    terminal.executeCommand("cat \"/home/monolith/my  file.txt\"");
    check(terminal.m_history == std::vector<std::string>{"exact spacing"},
          "quoted command paths preserve repeated spaces");

    const std::filesystem::path danglingPath = hostRoot / "home/monolith/dangling";
    std::filesystem::create_symlink(hostRoot / "missing-terminal-target", danglingPath, ec);
    check(!ec, "create terminal dangling symlink");
    terminal.executeCommand("rm /home/monolith/dangling");
    check(std::filesystem::symlink_status(danglingPath, ec).type()
              == std::filesystem::file_type::not_found,
          "rm removes an in-root dangling symlink entry");

    terminal.m_inputBuffer = "ls /";
    terminal.m_inputCursorPos = static_cast<int>(terminal.m_inputBuffer.size());
    terminal.handleTabCompletion();
    check(terminal.m_inputBuffer == "ls /home/",
          "absolute-root completion searches the virtual root");

    terminal.m_inputBuffer = "cat \"/home/monolith/my  ";
    terminal.m_inputCursorPos = static_cast<int>(terminal.m_inputBuffer.size());
    terminal.handleTabCompletion();
    check(terminal.m_inputBuffer == "cat \"/home/monolith/my  file.txt\"",
          "quoted file completion adds the closing double quote");

    terminal.m_inputBuffer = "cd \"/home/monolith/quoted";
    terminal.m_inputCursorPos = static_cast<int>(terminal.m_inputBuffer.size());
    terminal.handleTabCompletion();
    check(terminal.m_inputBuffer == "cd \"/home/monolith/quoted dir/",
          "quoted directory completion stays open for continued navigation");

    terminal.m_inputBuffer = "cat \"/home/monolith/my  file.txt\"";
    terminal.m_inputCursorPos = static_cast<int>(terminal.m_inputBuffer.size());
    terminal.handleTabCompletion();
    check(terminal.m_inputBuffer == "cat \"/home/monolith/my  file.txt\"",
          "completion after a closed quoted file leaves the command unchanged");

    terminal.m_history.assign(40, "output");
    terminal.onResize(320, 240);
    const int visibleLines = terminal.getMaxVisibleLines({0, 0, 320, 240});
    check(visibleLines > 0 && visibleLines < 40,
          "terminal visible lines match the rendered history area");
    terminal.scrollHistory(1000);
    check(terminal.m_scrollOffset == 40 - visibleLines,
          "terminal scrollback stops at the oldest fully visible output");
    terminal.onResize(320, 40);
    check(terminal.getMaxVisibleLines({0, 0, 320, 40}) == 0,
          "terminal reports no history rows when the input strip fills the client");
    check(terminal.m_scrollOffset == 40 - visibleLines,
          "terminal resize preserves a scrollback offset inside tiny client bounds");
    terminal.onResize(320, 240);
    terminal.m_scrollOffset = 39;
    check(TTF_SetFontSize(font, 22) == 0,
          "terminal state applies a larger test font");
    terminal.onUiScaleChanged();
    const int scaledVisibleLines = terminal.getMaxVisibleLines({0, 0, 320, 240});
    check(terminal.m_scrollOffset == 40 - scaledVisibleLines,
          "terminal text scaling clamps scrollback to the new history area");
    const SDL_Rect tinyContent{20, 30, 120, 8};
    const SDL_Rect tinyInputBar = terminal.getInputBarRect(tinyContent);
    check(tinyInputBar.x >= tinyContent.x
              && tinyInputBar.y >= tinyContent.y
              && tinyInputBar.x + tinyInputBar.w <= tinyContent.x + tinyContent.w
              && tinyInputBar.y + tinyInputBar.h <= tinyContent.y + tinyContent.h,
          "terminal input bar stays inside an undersized client area");

    check(fs.createDirectory("/home/monolith/work/nested"),
          "create terminal cwd move source");
    terminal.m_cwd = "/home/monolith/work/nested";
    check(fs.rename("/home/monolith/work", "/home/monolith/moved-work"),
          "move terminal cwd parent");
    terminal.onVirtualPathMoved("/home/monolith/work", "/home/monolith/moved-work");
    check(terminal.m_cwd == "/home/monolith/moved-work/nested",
          "terminal cwd follows a moved parent directory");

    check(fs.createDirectory("/home/monolith/to-delete/nested"),
          "create terminal cwd deletion source");
    terminal.m_cwd = "/home/monolith/to-delete/nested";
    check(fs.removeRecursive("/home/monolith/to-delete"),
          "remove terminal cwd parent");
    terminal.onVirtualPathRemoved("/home/monolith/to-delete");
    check(terminal.m_cwd == "/home/monolith",
          "terminal cwd returns to a valid parent after deletion");

    std::filesystem::remove_all(hostRoot, ec);
    if (failures == 0) {
        std::cout << "ALL TERMINAL FILESYSTEM TESTS PASSED\n";
        TTF_CloseFont(font);
        TTF_Quit();
        return 0;
    }
    std::cerr << failures << " test(s) failed\n";
    TTF_CloseFont(font);
    TTF_Quit();
    return 1;
}
