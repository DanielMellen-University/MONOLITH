// Headless regression test for Terminal filesystem command results.

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include <filesystem>
#include <iostream>
#include <string>
#include <unistd.h>

#include "../src/fs/Filesystem.hpp"

#define private public
#include "../src/app/TerminalApp.hpp"
#undef private

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
    check(fs.createDirectory("/home/monolith/empty"), "create empty directory");
    check(fs.writeFile("/home/monolith/note.txt", "hello"), "create regular file");

    monolith::app::TerminalApp terminal(nullptr, &fs);
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

    terminal.m_inputBuffer = "ls /";
    terminal.m_inputCursorPos = static_cast<int>(terminal.m_inputBuffer.size());
    terminal.handleTabCompletion();
    check(terminal.m_inputBuffer == "ls /home/",
          "absolute-root completion searches the virtual root");

    std::filesystem::remove_all(hostRoot, ec);
    if (failures == 0) {
        std::cout << "ALL TERMINAL FILESYSTEM TESTS PASSED\n";
        return 0;
    }
    std::cerr << failures << " test(s) failed\n";
    return 1;
}
