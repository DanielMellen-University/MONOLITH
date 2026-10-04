// Headless regression test for Terminal editing and filesystem command results.

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <deque>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <ostream>
#include <string>
#include <string_view>
#include <streambuf>
#include <unistd.h>
#include <vector>

#include "../src/app/App.hpp"
#include "../src/fs/Filesystem.hpp"
#include "../src/app/TerminalLexer.hpp"
#include "../src/app/Utf8.hpp"
#include "../src/detail/BufferedStreamWriter.hpp"

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

struct FailingStreamBuffer final : std::streambuf {
protected:
    std::streamsize xsputn(const char*, std::streamsize count) override {
        return count > 0 ? count - 1 : 0;
    }
};

bool persistedHistoryMatches(monolith::fs::Filesystem& fs, const std::string& path,
                             const std::vector<std::string>& commands) {
    size_t commandIndex = 0;
    size_t commandOffset = 0;
    bool matches = true;
    const bool readOk = fs.readFileChunks(path, [&](std::string_view chunk) {
        for (const char byte : chunk) {
            if (commandIndex >= commands.size()) {
                matches = false;
                return false;
            }
            const std::string& command = commands[commandIndex];
            const char expected = commandOffset < command.size()
                ? command[commandOffset] : '\n';
            if (byte != expected) {
                matches = false;
                return false;
            }
            ++commandOffset;
            if (commandOffset == command.size() + 1) {
                commandOffset = 0;
                ++commandIndex;
            }
        }
        return true;
    });
    return readOk && matches && commandIndex == commands.size() && commandOffset == 0;
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

    FailingStreamBuffer failingStreamBuffer;
    std::ostream failingOutput(&failingStreamBuffer);
    monolith::detail::BufferedStreamWriter failingWriter(failingOutput);
    const std::string fullWriterChunk(16 * 1024, 'x');
    check(!failingWriter.append(fullWriterChunk) && !failingWriter.finish(),
          "bounded stream writer propagates a failed chunk write");

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
    std::string chunkBoundaryContent(16 * 1024 - 1, 'a');
    chunkBoundaryContent += "\r\nb\r";
    check(fs.writeFile("/home/monolith/chunk-boundary.txt", chunkBoundaryContent),
          "create Terminal chunk-boundary line endings");
    std::string manyCatLines;
    for (int i = 0; i < 5001; ++i) {
        manyCatLines += "line " + std::to_string(i);
        if (i + 1 < 5001) manyCatLines.push_back('\n');
    }
    check(fs.writeFile("/home/monolith/many-lines.txt", manyCatLines),
          "create Terminal cat line-limit fixture");
    check(fs.writeFile("/home/monolith/long-line.txt",
                       std::string(monolith::app::TerminalApp::kMaxScrollbackLineBytes + 100,
                                   'z')),
          "create Terminal long-line fixture");
    check(fs.writeFile("/home/monolith/empty.txt", ""),
          "create empty Terminal cat fixture");
    check(fs.writeFile("/home/monolith/my  file.txt", "exact spacing"),
          "create file with repeated spaces");
    check(fs.writeFile("/home/monolith/O'Brien.txt", "apostrophe path"),
          "create file with an apostrophe");
    check(fs.createDirectory("/home/monolith/quoted dir"),
          "create directory for quoted completion");
    check(fs.createDirectory("/home/monolith/only-entry"),
          "create directory for empty-prefix completion");
    check(fs.writeFile("/home/monolith/only-entry/result.txt", "result"),
          "create unique empty-prefix completion candidate");
    check(fs.createDirectory("/home/monolith/unicode"),
          "create Unicode completion directory");
    check(fs.writeFile("/home/monolith/unicode/\xC3\xA9" "clair.txt", "a"),
          "write first Unicode terminal completion candidate");
    check(fs.writeFile("/home/monolith/unicode/\xC3\xAA" "cole.txt", "b"),
          "write second Unicode terminal completion candidate");

    check(SDL_Init(SDL_INIT_VIDEO) == 0, "terminal state SDL initialize");
    check(TTF_Init() == 0, "terminal state SDL_ttf initialize");
    TTF_Font* font = TTF_OpenFont("assets/fonts/DejaVuSans.ttf", 14);
    check(font != nullptr, "terminal state loads test font");
    if (!font) {
        TTF_Quit();
        SDL_Quit();
        std::filesystem::remove_all(hostRoot, ec);
        return 1;
    }

    TestTerminal terminal(font, &fs);
    TestController controller;
    terminal.setController(&controller);
    check(terminal.m_commandHistory == std::vector<std::string>{"echo first", "echo second"},
          "CRLF history entries lose their carriage returns");

    terminal.m_history.clear();
    terminal.m_historyBytes = 0;
    for (int i = 0; i < 2002; ++i) {
        terminal.addOutput("line " + std::to_string(i));
    }
    check(terminal.m_history.size() == 2000
              && terminal.m_history.front() == "line 2"
              && terminal.m_history.back() == "line 2001"
              && terminal.m_historyViewportMeasures.size() == terminal.m_history.size(),
          "scrollback keeps the newest rows at its row cap");
    const std::string* retainedRow = &terminal.m_history[1000];
    const auto* retainedMeasure = &terminal.m_historyViewportMeasures[1000];
    terminal.addOutput("line 2002");
    check(terminal.m_history.size() == 2000
              && terminal.m_history.front() == "line 3"
              && terminal.m_history.back() == "line 2002"
              && retainedRow == &terminal.m_history[999]
              && *retainedRow == "line 1002"
              && retainedMeasure == &terminal.m_historyViewportMeasures[999],
          "scrollback evicts oldest rows without relocating retained rows or measurements");

    const size_t truncationPrefixBytes = std::string("[truncated] ").size();
    std::string oversizedRow(
        monolith::app::TerminalApp::kMaxScrollbackLineBytes - truncationPrefixBytes - 1,
        'x');
    oversizedRow += "\xF0\x9F\x8C\x8B";
    oversizedRow += std::string(20, 't');
    terminal.addOutput(oversizedRow);
    bool storedRowIsCompleteUtf8 = true;
    const std::string storedContent = terminal.m_history.back().substr(truncationPrefixBytes);
    for (size_t pos = 0; pos < storedContent.size();) {
        const size_t charBytes = monolith::app::utf8CodepointByteLen(storedContent, pos);
        if (charBytes == 0 || pos + charBytes > storedContent.size()) {
            storedRowIsCompleteUtf8 = false;
            break;
        }
        pos += charBytes;
    }
    check(terminal.m_history.back().size() <= monolith::app::TerminalApp::kMaxScrollbackLineBytes
              && terminal.m_history.back().rfind("[truncated] ", 0) == 0
              && storedRowIsCompleteUtf8,
          "oversized terminal rows are UTF-8-safe capped and marked");
    const std::string fullScrollbackRow(
        monolith::app::TerminalApp::kMaxScrollbackLineBytes, 'y');
    for (int i = 0; i < 130; ++i) terminal.addOutput(fullScrollbackRow);
    check(terminal.m_historyBytes <= monolith::app::TerminalApp::kMaxScrollbackBytes
              && terminal.m_history.size() <= monolith::app::TerminalApp::kMaxScrollbackLines
              && terminal.m_historyViewportMeasures.size() == terminal.m_history.size(),
          "terminal scrollback stays within its total byte and row budgets");

    terminal.m_commandHistory.assign(500, "old command");
    terminal.m_inputBuffer = "new command";
    terminal.m_inputCursorPos = static_cast<int>(terminal.m_inputBuffer.size());
    terminal.submitInput();
    check(terminal.m_commandHistory.size() == 500
              && terminal.m_commandHistory.front() == "old command"
              && terminal.m_commandHistory.back() == "new command",
          "command history keeps its cap when a new command arrives");

    terminal.m_commandHistory.assign(
        40, std::string(monolith::app::TerminalApp::kMaxCommandHistoryEntryBytes, 'o'));
    terminal.m_inputBuffer = "echo recent";
    terminal.m_inputCursorPos = static_cast<int>(terminal.m_inputBuffer.size());
    terminal.submitInput();
    size_t retainedCommandHistoryBytes = 0;
    for (const auto& command : terminal.m_commandHistory) {
        retainedCommandHistoryBytes += command.size() + 1;
    }
    std::uint64_t savedHistoryBytes = 0;
    check(terminal.m_commandHistory.back() == "echo recent"
              && terminal.m_commandHistory.size() < 40
              && retainedCommandHistoryBytes
                  <= monolith::app::TerminalApp::kMaxCommandHistoryBytes
              && fs.fileSize(monolith::app::TerminalApp::HISTORY_FILE, savedHistoryBytes)
              && savedHistoryBytes <= monolith::app::TerminalApp::kMaxCommandHistoryBytes,
          "command history trims oldest entries to its byte budget before persisting");

    TestTerminal streamedHistoryTerminal(font, &fs);
    streamedHistoryTerminal.m_commandHistory.clear();
    for (int index = 0; index < 32; ++index) {
        const size_t commandBytes = index % 2 == 0
            ? monolith::app::TerminalApp::kMaxCommandHistoryEntryBytes
            : monolith::app::TerminalApp::kMaxCommandHistoryEntryBytes - 2;
        streamedHistoryTerminal.m_commandHistory.emplace_back(
            commandBytes, static_cast<char>('a' + index % 26));
    }
    std::uint64_t streamedHistoryBytes = 0;
    check(streamedHistoryTerminal.saveCommandHistory()
              && !streamedHistoryTerminal.m_commandHistorySaveFailed
              && fs.fileSize(monolith::app::TerminalApp::HISTORY_FILE,
                             streamedHistoryBytes)
              && streamedHistoryBytes
                  == monolith::app::TerminalApp::kMaxCommandHistoryBytes
              && persistedHistoryMatches(
                  fs, monolith::app::TerminalApp::HISTORY_FILE,
                  streamedHistoryTerminal.m_commandHistory),
          "Terminal streams exact command history at the 2 MiB limit");

    const auto historyBeforeOversizedCommand = terminal.m_commandHistory;
    terminal.m_inputBuffer = std::string(
        monolith::app::TerminalApp::kMaxCommandHistoryEntryBytes + 1, 'x');
    terminal.m_inputCursorPos = static_cast<int>(terminal.m_inputBuffer.size());
    terminal.submitInput();
    check(terminal.m_commandHistory == historyBeforeOversizedCommand
              && std::find(terminal.m_history.begin(), terminal.m_history.end(),
                           "Command not saved to history: exceeds 64 KiB.")
                     != terminal.m_history.end()
              && std::any_of(terminal.m_history.begin(), terminal.m_history.end(),
                             [](const std::string& line) {
                                 return line.find("Unknown command: ") != std::string::npos;
                             }),
          "oversized commands still execute but are not retained in history");

    std::string legacyHistory;
    const std::string legacyHistoryEntry = std::string(1000, 'l') + '\n';
    legacyHistory.reserve(legacyHistoryEntry.size() * 3000
                          + monolith::app::TerminalApp::kMaxCommandHistoryEntryBytes + 20);
    for (int i = 0; i < 3000; ++i) legacyHistory += legacyHistoryEntry;
    legacyHistory += std::string(
        monolith::app::TerminalApp::kMaxCommandHistoryEntryBytes + 1, 'x') + '\n';
    legacyHistory += "echo newest\n";
    check(fs.writeFile(monolith::app::TerminalApp::HISTORY_FILE, legacyHistory),
          "write oversized legacy command history fixture");
    TestTerminal loadedHistoryTerminal(font, &fs);
    size_t loadedCommandHistoryBytes = 0;
    for (const auto& command : loadedHistoryTerminal.m_commandHistory) {
        loadedCommandHistoryBytes += command.size() + 1;
    }
    std::uint64_t migratedHistoryBytes = 0;
    check(loadedHistoryTerminal.m_commandHistory.size()
                  <= monolith::app::TerminalApp::kMaxCommandHistory
              && loadedCommandHistoryBytes
                  <= monolith::app::TerminalApp::kMaxCommandHistoryBytes
              && loadedHistoryTerminal.m_commandHistory.back() == "echo newest"
              && fs.fileSize(monolith::app::TerminalApp::HISTORY_FILE, migratedHistoryBytes)
              && migratedHistoryBytes
                  <= monolith::app::TerminalApp::kMaxCommandHistoryBytes,
          "streamed history loading keeps newest entries bounded and migrates oversized files");

    std::string trailingOversizedHistory;
    for (int i = 0; i < 600; ++i) {
        trailingOversizedHistory += "echo fallback " + std::to_string(i) + '\n';
    }
    trailingOversizedHistory += std::string(
        monolith::app::TerminalApp::kMaxCommandHistoryReadBytes + 1, 'x') + '\n';
    check(fs.writeFile(monolith::app::TerminalApp::HISTORY_FILE, trailingOversizedHistory),
          "write history with an oversized final legacy command");
    TestTerminal fallbackHistoryTerminal(font, &fs);
    check(fallbackHistoryTerminal.m_commandHistory.size()
                  == monolith::app::TerminalApp::kMaxCommandHistory
              && fallbackHistoryTerminal.m_commandHistory.front() == "echo fallback 100"
              && fallbackHistoryTerminal.m_commandHistory.back() == "echo fallback 599",
          "history loader recovers older valid commands behind an oversized file tail");
    std::uint64_t fallbackHistoryBytes = 0;
    check(fs.fileSize(monolith::app::TerminalApp::HISTORY_FILE, fallbackHistoryBytes)
              && fallbackHistoryBytes
                  <= monolith::app::TerminalApp::kMaxCommandHistoryBytes,
          "history fallback rewrites the file inside its byte budget");

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

    terminal.m_commandHistory = {"echo one", "echo two", "echo two again", "echo last"};
    terminal.m_inputBuffer.clear();
    terminal.m_inputCursorPos = 0;
    key(SDLK_r, KMOD_CTRL);
    text("echo");
    check(terminal.m_searchMatchIndex == 3,
          "reverse search starts at the newest matching command");
    key(SDLK_r, KMOD_CTRL);
    check(terminal.m_searchMatchIndex == 2,
          "repeated reverse search moves to the previous matching command");
    key(SDLK_r, KMOD_CTRL);
    check(terminal.m_searchMatchIndex == 1,
          "reverse search keeps walking older matching commands");
    key(SDLK_ESCAPE);

    terminal.m_commandHistory = {"echo foo", "echo foobar"};
    terminal.m_inputBuffer.clear();
    terminal.m_inputCursorPos = 0;
    key(SDLK_r, KMOD_CTRL);
    text("foo");
    check(terminal.m_searchMatchIndex == 1,
          "reverse search finds the newest initial query match");
    text("b");
    check(terminal.m_searchBuffer == "foob" && terminal.m_searchMatchIndex == 1,
          "refining reverse search keeps a still-matching current entry");
    key(SDLK_BACKSPACE);
    check(terminal.m_searchBuffer == "foo" && terminal.m_searchMatchIndex == 1,
          "shortening reverse search reselects the newest matching entry");
    key(SDLK_r, KMOD_CTRL);
    check(terminal.m_searchMatchIndex == 0,
          "Ctrl+R still walks older matches after query editing");
    key(SDLK_ESCAPE);

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

    const std::string existingTouchPath = "/home/monolith/existing-touch.txt";
    const auto existingTouchHostPath = hostRoot / existingTouchPath.substr(1);
    check(fs.writeFile(existingTouchPath, "preserve this content"),
          "create existing-file touch fixture");
    const auto oldModifiedTime = std::filesystem::file_time_type::clock::now()
        - std::chrono::hours(24);
    std::filesystem::last_write_time(existingTouchHostPath, oldModifiedTime, ec);
    check(!ec, "age existing-file touch fixture");
    controller.changedPaths.clear();
    terminal.executeCommand("touch " + existingTouchPath);
    const auto newModifiedTime = std::filesystem::last_write_time(existingTouchHostPath, ec);
    check(!ec && newModifiedTime > oldModifiedTime
              && fs.readFile(existingTouchPath) == "preserve this content"
              && controller.changedPaths.empty(),
          "touch updates existing file time without changing content or signaling content edits");

    terminal.executeCommand("cp /home/monolith/note.txt /home/monolith/copied.txt");
    check(!controller.createdPaths.empty()
              && controller.createdPaths.back() == "/home/monolith/copied.txt",
          "cp notifies the shell about a copied file");
    terminal.executeCommand("cp /home/monolith/note.txt /home/monolith/copied.txt");
    check(!controller.changedPaths.empty()
              && controller.changedPaths.back() == "/home/monolith/copied.txt",
          "cp notifies the shell about an overwritten file");

    check(fs.createDirectory("/home/monolith/tree-source/nested/deep"),
          "create recursive copy source tree");
    check(fs.writeFile("/home/monolith/tree-source/nested/deep/file.txt", "new content"),
          "write recursive copy source child");
    check(fs.createDirectory("/home/monolith/tree-source/side"),
          "create recursive copy sibling directory");
    check(fs.writeFile("/home/monolith/tree-source/side/side.txt", "side content"),
          "write recursive copy sibling file");
    check(fs.writeFile("/home/monolith/tree-source/root.txt", "root content"),
          "write recursive copy root file");
    check(fs.createDirectory("/home/monolith/tree-dest/tree-source/nested/deep"),
          "create recursive copy destination tree");
    check(fs.writeFile(
              "/home/monolith/tree-dest/tree-source/nested/deep/file.txt", "old content"),
          "write recursive copy destination child");
    check(fs.createDirectory("/home/monolith/tree-dest/tree-source/side"),
          "create recursive copy destination sibling directory");
    check(fs.writeFile("/home/monolith/tree-dest/tree-source/side/side.txt", "old side"),
          "write recursive copy destination sibling file");
    check(fs.writeFile("/home/monolith/tree-dest/tree-source/root.txt", "old root"),
          "write recursive copy destination root file");
    controller.changedPaths.clear();
    terminal.executeCommand(
        "cp -r /home/monolith/tree-source /home/monolith/tree-dest");
    check(fs.readFile("/home/monolith/tree-dest/tree-source/nested/deep/file.txt")
              == "new content",
          "recursive cp overwrites the nested destination file");
    check(fs.readFile("/home/monolith/tree-dest/tree-source/side/side.txt") == "side content"
              && fs.readFile("/home/monolith/tree-dest/tree-source/root.txt") == "root content",
          "recursive cp overwrites sibling and root-level files");
    check(controller.changedPaths
              == std::vector<std::string>{
                  "/home/monolith/tree-dest/tree-source",
                  "/home/monolith/tree-dest/tree-source/nested",
                  "/home/monolith/tree-dest/tree-source/nested/deep",
                  "/home/monolith/tree-dest/tree-source/nested/deep/file.txt",
                  "/home/monolith/tree-dest/tree-source/side",
                  "/home/monolith/tree-dest/tree-source/side/side.txt",
                  "/home/monolith/tree-dest/tree-source/root.txt"},
          "recursive cp preserves directory-first depth-first notification order");

    terminal.m_history.clear();
    terminal.m_historyBytes = 0;
    terminal.executeCommand("cat /home/monolith/line-endings.txt");
    check(terminal.m_history == std::deque<std::string>{"first", "second", "third", ""},
          "cat normalizes CRLF and lone-CR line endings");

    terminal.m_history.clear();
    terminal.m_historyBytes = 0;
    terminal.executeCommand("cat /home/monolith/chunk-boundary.txt");
    check(terminal.m_history
              == std::deque<std::string>{std::string(16 * 1024 - 1, 'a'), "b", ""},
          "cat normalizes CRLF when its bytes cross a read-chunk boundary");

    terminal.m_history.clear();
    terminal.m_historyBytes = 0;
    terminal.executeCommand("cat /home/monolith/long-line.txt");
    check(terminal.m_history.size() == 1
              && terminal.m_history.back().size()
                  <= monolith::app::TerminalApp::kMaxScrollbackLineBytes
              && terminal.m_history.back().rfind("[truncated] ", 0) == 0,
          "cat truncates an oversized streamed row within the scrollback byte limit");

    terminal.m_history.clear();
    terminal.m_historyBytes = 0;
    terminal.executeCommand("cat /home/monolith/empty.txt");
    check(terminal.m_history == std::deque<std::string>{""},
          "cat displays an empty row for an empty file");

    terminal.m_history.clear();
    terminal.m_historyBytes = 0;
    terminal.executeCommand("cat /home/monolith/many-lines.txt");
    check(terminal.m_history.size() == monolith::app::TerminalApp::kMaxScrollbackLines
              && terminal.m_history.back() == "… cat: output truncated at 5000 lines",
          "cat stops its chunk reader at the configured line limit");

    terminal.m_history.clear();
    terminal.m_historyBytes = 0;
    terminal.executeCommand("cat \"/home/monolith/my  file.txt\"");
    check(terminal.m_history == std::deque<std::string>{"exact spacing"},
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

    terminal.m_inputBuffer = "cat '/home/monolith/O";
    terminal.m_inputCursorPos = static_cast<int>(terminal.m_inputBuffer.size());
    terminal.handleTabCompletion();
    check(terminal.m_inputBuffer == "cat '/home/monolith/O'\\''Brien.txt'",
          "single-quoted completion escapes apostrophes and closes the path");
    const auto apostropheCommand = monolith::app::tokenizeCommandLine(
        terminal.m_inputBuffer);
    check(apostropheCommand.error.empty()
              && apostropheCommand.args
                  == std::vector<std::string>{"cat", "/home/monolith/O'Brien.txt"},
          "single-quoted completion preserves the apostrophe path argument");

    terminal.m_inputBuffer = "cat /home/monolith/unicode/";
    terminal.m_inputCursorPos = static_cast<int>(terminal.m_inputBuffer.size());
    terminal.handleTabCompletion();
    check(terminal.m_inputBuffer == "cat /home/monolith/unicode/",
          "ambiguous terminal completion does not insert a partial UTF-8 codepoint");

    terminal.m_cwd = "/home/monolith/only-entry";
    terminal.m_inputBuffer = "cd ";
    terminal.m_inputCursorPos = static_cast<int>(terminal.m_inputBuffer.size());
    terminal.handleTabCompletion();
    check(terminal.m_inputBuffer == "cd result.txt",
          "empty path prefixes complete from the terminal working directory");

    terminal.m_commandHistory = {"first command", "second command"};
    terminal.m_inputBuffer = "draft";
    terminal.m_inputCursorPos = 2;
    key(SDLK_UP);
    check(terminal.m_inputBuffer == "second command" && terminal.m_historyIndex == 1,
          "terminal history navigation recalls the newest command");
    key(SDLK_DOWN);
    check(terminal.m_inputBuffer == "draft" && terminal.m_inputCursorPos == 2,
          "terminal history navigation restores the draft caret");
    key(SDLK_UP);
    text(" edited");
    check(terminal.m_inputBuffer == "second command edited" && terminal.m_historyIndex == -1,
          "editing a recalled command exits history navigation");
    key(SDLK_DOWN);
    check(terminal.m_inputBuffer == "second command edited" && terminal.m_historyIndex == -1,
          "down does not overwrite an edited recalled command");

    terminal.m_inputBuffer = "echo \xC3\xA9x";
    terminal.m_inputCursorPos = static_cast<int>(terminal.m_inputBuffer.size());
    key(SDLK_LEFT, KMOD_SHIFT);
    key(SDLK_LEFT, KMOD_SHIFT);
    const auto selectedUtf8 = terminal.inputSelectionRange();
    check(terminal.hasInputSelection()
              && terminal.m_inputBuffer.substr(
                     selectedUtf8.first, selectedUtf8.second - selectedUtf8.first)
                  == "\xC3\xA9x",
          "shift selection expands by complete UTF-8 codepoints");
    key(SDLK_RIGHT);
    check(!terminal.hasInputSelection()
              && terminal.m_inputCursorPos == static_cast<int>(terminal.m_inputBuffer.size()),
          "plain cursor movement collapses selection toward the movement direction");

    terminal.m_inputBuffer = "echo \xC3\xA9x";
    terminal.m_inputCursorPos = static_cast<int>(terminal.m_inputBuffer.size());
    key(SDLK_LEFT, KMOD_SHIFT);
    key(SDLK_LEFT, KMOD_SHIFT);
    key(SDLK_c, KMOD_CTRL);
    char* copiedSelection = SDL_GetClipboardText();
    const bool clipboardCopied = copiedSelection
        && std::string(copiedSelection) == "\xC3\xA9x";
    SDL_free(copiedSelection);
    key(SDLK_x, KMOD_CTRL);
    check(clipboardCopied && terminal.m_inputBuffer == "echo "
              && terminal.m_inputCursorPos == 5,
          "copy and cut preserve selected UTF-8 text and delete its range");

    terminal.m_inputBuffer = "echo abc";
    terminal.m_inputCursorPos = static_cast<int>(terminal.m_inputBuffer.size());
    key(SDLK_LEFT, KMOD_SHIFT);
    key(SDLK_LEFT, KMOD_SHIFT);
    text("z");
    check(terminal.m_inputBuffer == "echo az" && !terminal.hasInputSelection(),
          "typed text replaces the selected input range");

    check(SDL_SetClipboardText("echo\tpasted\r\nsecond\tline\nthird line") == 0,
          "set multiline terminal paste fixture");
    terminal.m_inputBuffer = "draft";
    terminal.m_inputCursorPos = static_cast<int>(terminal.m_inputBuffer.size());
    key(SDLK_a, KMOD_CTRL);
    key(SDLK_v, KMOD_CTRL);
    check(terminal.m_inputBuffer == "echo pasted second line third line"
              && terminal.m_inputCursorPos == static_cast<int>(terminal.m_inputBuffer.size()),
          "terminal paste replaces selection and flattens all clipboard lines and tabs");

    terminal.m_inputBuffer = "A\xF0\x9F\x8C\x8B" "B";
    terminal.m_inputCursorPos = static_cast<int>(terminal.m_inputBuffer.size());
    key(SDLK_LEFT, KMOD_SHIFT);
    key(SDLK_LEFT, KMOD_SHIFT);
    key(SDLK_BACKSPACE);
    check(terminal.m_inputBuffer == "A" && !terminal.hasInputSelection(),
          "backspace deletes a selected multi-byte UTF-8 range");

    terminal.onResize(360, 200);
    terminal.m_inputBuffer = "abcdef";
    terminal.m_inputCursorPos = 0;
    terminal.m_inputHorizontalScrollPx = 0;
    const std::string& prompt = terminal.getInputPrompt();
    const std::string originalPrompt = prompt;
    const char* promptStorage = prompt.data();
    const bool reusedPrompt = &terminal.getInputPrompt() == &prompt
        && terminal.getInputPrompt().data() == promptStorage
        && prompt == originalPrompt;
    const std::string originalCwd = terminal.m_cwd;
    terminal.m_cwd = "/home/monolith/prompt-test";
    const std::string changedPrompt = terminal.getInputPrompt();
    terminal.m_cwd = originalCwd;
    const std::string restoredPrompt = terminal.getInputPrompt();
    check(reusedPrompt && changedPrompt == "~/prompt-test> "
              && restoredPrompt == originalPrompt,
          "Terminal reuses its prompt until the working directory changes");
    int afterTwoWidth = 0;
    int afterFiveWidth = 0;
    int ignoredHeight = 0;
    TTF_SizeUTF8(font, (prompt + "ab").c_str(), &afterTwoWidth, &ignoredHeight);
    TTF_SizeUTF8(font, (prompt + "abcde").c_str(), &afterFiveWidth, &ignoredHeight);
    const SDL_Rect inputBar = terminal.getInputBarRect({0, 0, 360, 200});
    SDL_Event mouse{};
    mouse.type = SDL_MOUSEBUTTONDOWN;
    mouse.button.button = SDL_BUTTON_LEFT;
    mouse.button.x = 8 + afterTwoWidth;
    mouse.button.y = inputBar.y + inputBar.h / 2;
    terminal.handleEvent(mouse);
    mouse = {};
    mouse.type = SDL_MOUSEMOTION;
    mouse.motion.x = 8 + afterFiveWidth;
    mouse.motion.y = inputBar.y + inputBar.h / 2;
    terminal.handleEvent(mouse);
    const auto mouseSelection = terminal.inputSelectionRange();
    const SDL_Rect inputContent{0, 0, 360, 200};
    const int measuredMouseCursor = terminal.inputCursorAtX(
        8 + afterFiveWidth, inputContent);
    const char* inputMeasureStorage = terminal.m_inputMeasureScratch.data();
    const int repeatedMouseCursor = terminal.inputCursorAtX(
        8 + afterFiveWidth, inputContent);
    check(measuredMouseCursor == repeatedMouseCursor
              && terminal.m_inputMeasureScratch.data() == inputMeasureStorage,
          "Terminal reuses input-prefix storage during mouse hit testing");
    mouse = {};
    mouse.type = SDL_MOUSEBUTTONUP;
    mouse.button.button = SDL_BUTTON_LEFT;
    mouse.button.x = 8 + afterFiveWidth;
    mouse.button.y = inputBar.y + inputBar.h / 2;
    terminal.handleEvent(mouse);
    check(terminal.m_inputBuffer.substr(
              mouseSelection.first, mouseSelection.second - mouseSelection.first) == "cde"
              && !terminal.m_selectingInputWithMouse,
          "mouse dragging selects text at measured UTF-8 caret boundaries");
    terminal.clearInputSelection();

    terminal.m_history.assign(40, "output");
    terminal.m_historyBytes = 40 * std::string("output").size();
    terminal.onResize(320, 240);
    const int visibleLines = terminal.getMaxVisibleLines({0, 0, 320, 240});
    check(visibleLines > 0 && visibleLines < 40,
          "terminal visible lines match the rendered history area");
    terminal.scrollHistory(1000);
    check(terminal.m_scrollOffset == 40 - visibleLines,
          "terminal scrollback stops at the oldest fully visible output");
    terminal.m_scrollOffset = 0;
    SDL_Keysym verticalPageUp{};
    verticalPageUp.sym = SDLK_PAGEUP;
    terminal.handleKeyDown(verticalPageUp);
    check(terminal.m_scrollOffset == 3,
          "Terminal Page Up keeps its vertical history behavior");
    terminal.m_scrollOffset = 40 - visibleLines;
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
    const SDL_Rect narrowHistory = terminal.getHistoryRect({20, 30, 12, 8});
    check(narrowHistory.w >= 0 && narrowHistory.h >= 0,
          "terminal history clip stays non-negative in a narrow client area");

    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(
        0, 240, 240, 32, SDL_PIXELFORMAT_RGBA32);
    SDL_Renderer* renderer = surface ? SDL_CreateSoftwareRenderer(surface) : nullptr;
    check(renderer != nullptr, "terminal state creates a software renderer");
    if (renderer) {
        const SDL_Rect expectedClip{5, 6, 140, 120};
        SDL_RenderSetClipRect(renderer, &expectedClip);
        terminal.render(renderer, {0, 0, 200, 200});
        const char* inputRenderStorage = terminal.m_renderInputText.data();
        const char* cursorRenderStorage = terminal.m_renderCursorPrefix.data();
        const char* selectionStartStorage = terminal.m_renderSelectionStartText.data();
        const char* selectionEndStorage = terminal.m_renderSelectionEndText.data();
        const bool selectionWasRendered = terminal.hasInputSelection();
        const auto firstMeasuredRow = std::find_if(
            terminal.m_historyViewportMeasures.begin(),
            terminal.m_historyViewportMeasures.end(),
            [](const auto& measure) { return measure.valid; });
        const std::size_t measuredRowIndex = firstMeasuredRow
            == terminal.m_historyViewportMeasures.end()
            ? terminal.m_historyViewportMeasures.size()
            : static_cast<std::size_t>(std::distance(
                  terminal.m_historyViewportMeasures.begin(), firstMeasuredRow));
        const monolith::app::TerminalApp::HistoryViewportMeasure firstHistoryMeasure =
            firstMeasuredRow == terminal.m_historyViewportMeasures.end()
            ? monolith::app::TerminalApp::HistoryViewportMeasure{}
            : *firstMeasuredRow;
        SDL_Rect restoredClip{};
        SDL_RenderGetClipRect(renderer, &restoredClip);
        check(restoredClip.x == expectedClip.x
                  && restoredClip.y == expectedClip.y
                  && restoredClip.w == expectedClip.w
                  && restoredClip.h == expectedClip.h,
              "terminal restores the caller renderer clip after rendering");
        const SDL_Color terminalTextColor{200, 205, 210, 255};
        const auto firstOutputTexture = terminal.m_textTextureCache.get(
            renderer, font, "output", terminalTextColor);
        const std::string inputBeforeCursor = terminal.getInputPrompt()
            + terminal.m_inputBuffer.substr(0, terminal.m_inputCursorPos);
        const auto firstInputTexture = terminal.m_textTextureCache.get(
            renderer, font, inputBeforeCursor.c_str(), terminalTextColor);
        const size_t cachedTextureCount = terminal.m_textTextureCache.size();
        terminal.render(renderer, {0, 0, 200, 200});
        const bool reusedHistoryMeasure = measuredRowIndex
                < terminal.m_historyViewportMeasures.size()
            && terminal.m_historyViewportMeasures[measuredRowIndex].valid
            && terminal.m_historyViewportMeasures[measuredRowIndex].pixelWidth
                == firstHistoryMeasure.pixelWidth
            && terminal.m_historyViewportMeasures[measuredRowIndex].visibleBytes
                == firstHistoryMeasure.visibleBytes;
        const auto repeatedOutputTexture = terminal.m_textTextureCache.get(
            renderer, font, "output", terminalTextColor);
        const auto repeatedInputTexture = terminal.m_textTextureCache.get(
            renderer, font, inputBeforeCursor.c_str(), terminalTextColor);
        const bool reusedInputRenderBuffers =
            terminal.m_renderInputText.data() == inputRenderStorage
            && terminal.m_renderCursorPrefix.data() == cursorRenderStorage
            && (!selectionWasRendered
                || (terminal.m_renderSelectionStartText.data() == selectionStartStorage
                    && terminal.m_renderSelectionEndText.data() == selectionEndStorage));
        check(firstOutputTexture && firstInputTexture
                  && repeatedOutputTexture.handle == firstOutputTexture.handle
                  && repeatedInputTexture.handle == firstInputTexture.handle
                  && terminal.m_textTextureCache.size() == cachedTextureCount
                  && firstHistoryMeasure.valid
                  && firstHistoryMeasure.pixelWidth == 184
                  && reusedHistoryMeasure
                  && reusedInputRenderBuffers,
              "Terminal reuses input, scrollback textures, and visible-row measurements between frames");
        terminal.m_searchMode = true;
        terminal.m_searchBuffer = "second";
        terminal.m_searchCursorPos = 3;
        terminal.m_searchMatchIndex = -1;
        const std::string reverseSearchText =
            "(reverse-i-search)`second': (no match)";
        const std::string reverseSearchCursorPrefix = "(reverse-i-search)`sec";
        const size_t beforeReverseSearch = terminal.m_textTextureCache.size();
        terminal.render(renderer, {0, 0, 200, 200});
        const char* reverseSearchTextStorage = terminal.m_renderInputText.data();
        const char* reverseSearchCursorStorage = terminal.m_renderCursorPrefix.data();
        const auto reverseSearchTexture = terminal.m_textTextureCache.get(
            renderer, font, reverseSearchText.c_str(), terminalTextColor);
        const size_t afterReverseSearchRender = terminal.m_textTextureCache.size();
        const auto firstReverseSearchCursorTexture = terminal.m_textTextureCache.get(
            renderer, font, reverseSearchCursorPrefix.c_str(), terminalTextColor);
        int expectedReverseSearchCursorWidth = 0;
        int expectedReverseSearchCursorHeight = 0;
        const bool measuredReverseSearchCursor = TTF_SizeUTF8(
            font, reverseSearchCursorPrefix.c_str(),
            &expectedReverseSearchCursorWidth, &expectedReverseSearchCursorHeight) == 0;
        const size_t afterReverseSearchCursorLookup = terminal.m_textTextureCache.size();
        terminal.render(renderer, {0, 0, 200, 200});
        const bool reusedReverseSearchBuffers =
            terminal.m_renderInputText.data() == reverseSearchTextStorage
            && terminal.m_renderCursorPrefix.data() == reverseSearchCursorStorage;
        const auto repeatedReverseSearchCursorTexture = terminal.m_textTextureCache.get(
            renderer, font, reverseSearchCursorPrefix.c_str(), terminalTextColor);
        check(reverseSearchTexture
                  && terminal.m_textTextureCache.size() > beforeReverseSearch
                  && firstReverseSearchCursorTexture
                  && afterReverseSearchRender == afterReverseSearchCursorLookup
                  && repeatedReverseSearchCursorTexture.handle
                      == firstReverseSearchCursorTexture.handle
                  && measuredReverseSearchCursor
                  && repeatedReverseSearchCursorTexture.width == expectedReverseSearchCursorWidth
                  && expectedReverseSearchCursorHeight > 0
                  && terminal.m_textTextureCache.size() == afterReverseSearchCursorLookup
                  && reusedReverseSearchBuffers,
              "Terminal reuses reverse-search text and cursor-prefix metrics between frames");
        terminal.m_searchMode = false;
        const size_t beforeClear = terminal.m_textTextureCache.size();
        terminal.executeCommand("clear");
        check(terminal.m_textTextureCache.size() == beforeClear
                  && terminal.m_historyBytes == 0
                  && terminal.m_historyViewportMeasures.empty(),
              "Terminal clears scrollback measurements while retaining bounded text textures");
        terminal.addOutput("output");
        terminal.render(renderer, {0, 0, 200, 200});
        const auto outputAfterClear = terminal.m_textTextureCache.get(
            renderer, font, "output", terminalTextColor);
        check(outputAfterClear.handle == firstOutputTexture.handle,
              "Terminal reuses scrollback text after output resumes");
        terminal.m_historyHorizontalScrollPx = 48;
        terminal.onUiScaleChanged();
        check(terminal.m_textTextureCache.size() == 0
                  && terminal.m_historyHorizontalScrollPx == 0
                  && std::none_of(
                      terminal.m_historyViewportMeasures.begin(),
                      terminal.m_historyViewportMeasures.end(),
                      [](const auto& measure) { return measure.valid; }),
              "Terminal clears renderer textures and viewport measurements when UI scale changes");

        std::string longUnicodeLine;
        for (int i = 0; i < 5000; ++i) {
            longUnicodeLine += "\xC3\xA9" "\xF0\x9F\x8C\x8B";
        }
        terminal.m_history.assign(2, longUnicodeLine);
        terminal.m_historyBytes = longUnicodeLine.size() * terminal.m_history.size();
        terminal.m_scrollOffset = 0;
        terminal.render(renderer, {0, 0, 200, 200});
        const SDL_Rect historyRect = terminal.getHistoryRect({0, 0, 200, 200});
        const auto unicodeMeasure = std::find_if(
            terminal.m_historyViewportMeasures.begin(),
            terminal.m_historyViewportMeasures.end(),
            [](const auto& measure) { return measure.valid; });
        const bool unicodeMeasureIsViewportBound =
            unicodeMeasure != terminal.m_historyViewportMeasures.end()
            && unicodeMeasure->pixelWidth == historyRect.w
            && unicodeMeasure->visibleBytes < longUnicodeLine.size()
            && monolith::app::utf8ClampToCodepointBoundary(
                   longUnicodeLine, unicodeMeasure->visibleBytes)
                == unicodeMeasure->visibleBytes;
        size_t largestCachedTextBytes = 0;
        bool cachedPrefixesAreCompleteUtf8 = true;
        bool cachedTexturesFitViewport = true;
        bool foundLongLinePrefix = false;
        for (const auto& [key, cachedTexture] : terminal.m_textTextureCache.m_entries) {
            const std::string& cachedText = key.text;
            if (cachedText.size() < 2
                || cachedText.compare(0, 2, longUnicodeLine, 0, 2) != 0) {
                continue;
            }
            foundLongLinePrefix = true;
            largestCachedTextBytes = std::max(largestCachedTextBytes, cachedText.size());
            for (size_t pos = 0; pos < cachedText.size();) {
                const size_t charBytes = monolith::app::utf8CodepointByteLen(cachedText, pos);
                if (charBytes == 0 || pos + charBytes > cachedText.size()) {
                    cachedPrefixesAreCompleteUtf8 = false;
                    break;
                }
                pos += charBytes;
            }
            if (cachedTexture.width > historyRect.w) {
                cachedTexturesFitViewport = false;
            }
        }
        check(unicodeMeasureIsViewportBound
                  && foundLongLinePrefix
                  && cachedTexturesFitViewport
                  && largestCachedTextBytes < longUnicodeLine.size() / 10
                  && cachedPrefixesAreCompleteUtf8,
              "Terminal caches viewport-sized, complete UTF-8 prefixes of long rows");

        std::string horizontallyScrollableLine;
        for (int i = 0; i < 100; ++i) {
            horizontallyScrollableLine += "\xC3\xA9" "\xF0\x9F\x8C\x8B";
        }
        horizontallyScrollableLine += "TAIL";
        terminal.m_history.assign(2, horizontallyScrollableLine);
        terminal.m_historyBytes = horizontallyScrollableLine.size() * terminal.m_history.size();
        terminal.m_historyViewportMeasures.clear();
        terminal.m_scrollOffset = 0;
        terminal.m_historyHorizontalScrollPx = 0;
        terminal.render(renderer, {0, 0, 200, 200});
        const SDL_Rect horizontalHistoryRect = terminal.getHistoryRect({0, 0, 200, 200});
        std::size_t initialStartBytes = 0;
        std::size_t initialVisibleBytes = 0;
        const bool initialRange = terminal.getVisibleHistoryRangeBytes(
            0, horizontalHistoryRect.w, 0, initialStartBytes, initialVisibleBytes);
        SDL_Keysym panRight{};
        panRight.sym = SDLK_PAGEUP;
        panRight.mod = KMOD_SHIFT;
        for (int i = 0; i < 100; ++i) terminal.handleKeyDown(panRight);
        terminal.render(renderer, {0, 0, 200, 200});
        std::size_t pannedStartBytes = 0;
        std::size_t pannedVisibleBytes = 0;
        const bool pannedRange = terminal.getVisibleHistoryRangeBytes(
            0, horizontalHistoryRect.w, terminal.m_historyHorizontalScrollPx,
            pannedStartBytes, pannedVisibleBytes);
        const std::string pannedText = pannedRange
            ? horizontallyScrollableLine.substr(pannedStartBytes, pannedVisibleBytes)
            : std::string{};
        int pannedPixelWidth = 0;
        int pannedTextHeight = 0;
        const bool pannedTextMeasured = TTF_SizeUTF8(
            font, pannedText.c_str(), &pannedPixelWidth, &pannedTextHeight) == 0;
        const auto pannedTexture = terminal.m_textTextureCache.get(
            renderer, font, pannedText.c_str(), terminalTextColor);
        check(initialRange && initialStartBytes == 0
                  && horizontallyScrollableLine.substr(
                         initialStartBytes, initialVisibleBytes).find("TAIL") == std::string::npos,
              "Terminal starts long output rows at their left edge");
        check(terminal.m_historyHorizontalScrollPx > 0,
              "Shift+Page Up advances Terminal's horizontal output offset");
        check(pannedRange && pannedStartBytes > 0,
              "Terminal measures a later UTF-8-safe output range after horizontal panning");
        check(pannedRange && pannedText.find("TAIL") != std::string::npos,
              "Terminal horizontal panning can reveal the end of a long output row");
        check(pannedTextMeasured && pannedPixelWidth <= horizontalHistoryRect.w
                  && pannedTexture && pannedTexture.width <= horizontalHistoryRect.w
                  && monolith::app::utf8ClampToCodepointBoundary(
                         horizontallyScrollableLine, pannedStartBytes) == pannedStartBytes
                  && monolith::app::utf8ClampToCodepointBoundary(
                         horizontallyScrollableLine,
                         pannedStartBytes + pannedVisibleBytes)
                      == pannedStartBytes + pannedVisibleBytes,
              "Terminal pans and renders only a viewport-sized UTF-8 segment");

        SDL_Keysym panLeft{};
        panLeft.sym = SDLK_PAGEDOWN;
        panLeft.mod = KMOD_SHIFT;
        for (int i = 0; i < 100; ++i) terminal.handleKeyDown(panLeft);
        check(terminal.m_historyHorizontalScrollPx == 0,
              "Shift+Page Down returns Terminal output to the left edge");
        terminal.handleKeyDown(panRight);
        terminal.addOutput("new output resets the panned view");
        check(terminal.m_historyHorizontalScrollPx == 0,
              "new Terminal output resets horizontal scrollback to the left edge");

        terminal.m_history.assign(40, "output");
        terminal.m_historyBytes = 40 * std::string("output").size();
        terminal.m_scrollOffset = 0;
        terminal.render(renderer, {0, 0, 200, 200});
        const auto outputBeforeScroll = terminal.m_textTextureCache.get(
            renderer, font, "output", terminalTextColor);
        const size_t beforeScrollCount = terminal.m_textTextureCache.size();
        terminal.scrollHistory(1);
        check(terminal.m_textTextureCache.size() == beforeScrollCount
                  && terminal.m_textTextureCache.get(
                         renderer, font, "output", terminalTextColor).handle
                      == outputBeforeScroll.handle,
              "Terminal retains unchanged text textures when scrolling history");
        terminal.render(renderer, {0, 0, 200, 200});
        check(terminal.m_textTextureCache.size() == beforeScrollCount,
              "Terminal reuses repeated rows after scrolling");
        terminal.onResize(210, 200);
        terminal.render(renderer, {0, 0, 210, 200});
        check(terminal.m_textTextureCache.size() == beforeScrollCount
                  && std::any_of(
                      terminal.m_historyViewportMeasures.begin(),
                      terminal.m_historyViewportMeasures.end(),
                      [](const auto& measure) {
                          return measure.valid && measure.pixelWidth == 194;
                      }),
              "Terminal reuses text textures and refreshes visible-row measurements after resizing");

        terminal.m_scrollOffset = 39;
        terminal.render(renderer, {0, 0, 320, 40});
        check(terminal.m_clientWidth == 320
                  && terminal.m_clientHeight == 40
                  && terminal.getMaxVisibleLines({0, 0, 320, 40}) == 0,
              "Terminal direct renders synchronize client geometry before scrolling");
        terminal.m_textTextureCache.clear();
        SDL_DestroyRenderer(renderer);
    }
    if (surface) SDL_FreeSurface(surface);

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

    const std::filesystem::path hostHistoryPath =
        hostRoot / "home/monolith/.terminal_history";
    std::filesystem::remove(hostHistoryPath, ec);
    ec.clear();
    check(std::filesystem::create_directory(hostHistoryPath, ec) && !ec,
          "block Terminal command-history persistence");
    TestTerminal persistenceTerminal(font, &fs);
    persistenceTerminal.m_inputBuffer = "echo still runs";
    persistenceTerminal.m_inputCursorPos =
        static_cast<int>(persistenceTerminal.m_inputBuffer.size());
    persistenceTerminal.submitInput();
    check(persistenceTerminal.m_commandHistorySaveFailed
              && persistenceTerminal.m_commandHistory
                  == std::vector<std::string>{"echo still runs"}
              && std::find(persistenceTerminal.m_history.begin(),
                           persistenceTerminal.m_history.end(),
                           "still runs") != persistenceTerminal.m_history.end()
              && persistenceTerminal.m_history.back()
                  == "Warning: Terminal command history could not be saved.",
          "Terminal reports history-save failure after executing the command");

    ec.clear();
    std::filesystem::remove_all(hostHistoryPath, ec);
    persistenceTerminal.m_inputBuffer = "echo recovered command";
    persistenceTerminal.m_inputCursorPos =
        static_cast<int>(persistenceTerminal.m_inputBuffer.size());
    persistenceTerminal.submitInput();
    std::string persistedHistory;
    check(!ec && !persistenceTerminal.m_commandHistorySaveFailed
              && persistenceTerminal.m_history.back()
                  == "Terminal command history save recovered."
              && fs.readFile(monolith::app::TerminalApp::HISTORY_FILE, persistedHistory)
              && persistedHistory.find("echo recovered command\n")
                  != std::string::npos,
          "Terminal retries history persistence and reports successful recovery");
    TestTerminal reloadedPersistenceTerminal(font, &fs);
    check(std::find(reloadedPersistenceTerminal.m_commandHistory.begin(),
                    reloadedPersistenceTerminal.m_commandHistory.end(),
                    "echo recovered command")
              != reloadedPersistenceTerminal.m_commandHistory.end(),
          "Terminal reloads commands persisted after a failed write");

    const std::string oversizedStartupHistory(
        monolith::app::TerminalApp::kMaxCommandHistoryBytes + 1, 'x');
    check(fs.writeFile(monolith::app::TerminalApp::HISTORY_FILE,
                       oversizedStartupHistory),
          "write oversized Terminal history for migration failure");
    std::filesystem::permissions(
        hostHistoryPath,
        std::filesystem::perms::owner_read
            | std::filesystem::perms::group_read
            | std::filesystem::perms::others_read,
        std::filesystem::perm_options::replace, ec);
    check(!ec, "make oversized Terminal history unwritable");
    TestTerminal failedMigrationTerminal(font, &fs);
    std::string preservedStartupHistory;
    check(failedMigrationTerminal.m_commandHistorySaveFailed
              && failedMigrationTerminal.m_history.back()
                  == "Warning: Terminal command history could not be saved."
              && fs.readFile(monolith::app::TerminalApp::HISTORY_FILE,
                             preservedStartupHistory)
              && preservedStartupHistory == oversizedStartupHistory,
          "Terminal reports a failed startup migration and preserves the old history");
    std::filesystem::permissions(
        hostHistoryPath, std::filesystem::perms::owner_write,
        std::filesystem::perm_options::add, ec);
    check(!ec, "restore Terminal history write permission after migration test");

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
    SDL_Quit();
    return 1;
}
