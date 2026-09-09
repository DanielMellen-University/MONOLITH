#include "TerminalApp.hpp"
#include "FilePath.hpp"
#include "TerminalLexer.hpp"
#include "Utf8.hpp"
#include <algorithm>
#include <cctype>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace monolith::app {

namespace {

std::string normalizeLineEndings(const std::string& text) {
    std::string normalized;
    normalized.reserve(text.size());
    for (size_t i = 0; i < text.size(); ++i) {
        if (text[i] == '\r') {
            if (i + 1 < text.size() && text[i + 1] == '\n') {
                ++i;
            }
            normalized.push_back('\n');
        } else {
            normalized.push_back(text[i]);
        }
    }
    return normalized;
}

} // namespace

TerminalApp::TerminalApp(TTF_Font* font, monolith::fs::Filesystem* fs)
    : m_font(font), m_fs(fs)
{
    loadCommandHistory();

    // Welcome message
    addOutput("Monolith Terminal");
    addOutput("Type 'help' for a list of commands.");
    addOutput("");
}

void TerminalApp::addOutput(const std::string& line) {
    m_history.push_back(line);
    while (m_history.size() > kMaxScrollbackLines) {
        m_history.erase(m_history.begin());
    }
    m_scrollOffset = 0;   // auto-scroll to bottom on new output
}

void TerminalApp::submitInput() {
    std::string command = m_inputBuffer;
    m_inputBuffer.clear();
    m_inputCursorPos = 0;
    m_historyIndex = -1;
    m_savedInputBuffer.clear();

    // Echo the command as the user typed it
    addOutput(getInputPrompt() + command);

    if (!command.empty()) {
        m_commandHistory.push_back(command);
        while (m_commandHistory.size() > kMaxCommandHistory) {
            m_commandHistory.erase(m_commandHistory.begin());
        }
        saveCommandHistory();
        executeCommand(command);
    } else {
        addOutput(""); // blank line for empty input
    }
    m_scrollOffset = 0;   // always jump back to bottom after running a command
}

void TerminalApp::executeCommand(const std::string& commandLine) {
    // Build arg list with quoting so paths containing spaces work (see TerminalLexer).
    CommandTokens tokens = tokenizeCommandLine(commandLine);
    if (!tokens.error.empty()) {
        addOutput("parse error: " + tokens.error);
        return;
    }
    const std::vector<std::string>& args = tokens.args;

    std::string cmd = args.empty() ? "" : args[0];

    // Rebuild a "rest" for backward compat with untouched commands (echo etc.)
    std::string rest;
    for (size_t i = 1; i < args.size(); ++i) {
        if (i > 1) rest += " ";
        rest += args[i];
    }

    if (cmd == "echo") {
        addOutput(rest);
    }
    else if (cmd == "clear") {
        m_history.clear();
    }
    else if (cmd == "help") {
        addOutput("Available commands:");
        addOutput("  echo <text>     - Print text");
        addOutput("  clear           - Clear the screen");
        addOutput("  date            - Show current date and time");
        addOutput("  whoami          - Print current user");
        addOutput("  version         - Show Monolith version");
        addOutput("  ls [path]       - List directory contents (▶ = directory)");
        addOutput("  pwd             - Print working directory");
        addOutput("  cd [dir]        - Change directory");
        addOutput("  mkdir <dir>     - Create directory");
        addOutput("  touch <file>    - Create empty file");
        addOutput("  cp [-r] <src> <dst> - Copy file (or dir tree with -r); dst dir supported");
        addOutput("  rm [-r] <path>  - Remove file or directory (-r for recursive)");
        addOutput("  mv <src> <dst>  - Move/rename file or directory (dst dir supported)");
        addOutput("  cat <file>      - Show file contents");
        addOutput("  edit <file>     - Open a text file in the Text Editor");
        addOutput("  open <path>     - Open a file (.modr in Drawing, else Editor)");
        addOutput("  history         - Show command history");
        addOutput("  help            - Show this message");
        addOutput("  exit / quit     - Close this terminal");
        addOutput("");
        addOutput("Quoting: use \"...\" or '...' to keep spaces in an argument.");
    }
    else if (cmd == "date") {
        std::time_t now = std::time(nullptr);
        std::string timeStr = std::ctime(&now);
        // ctime adds a trailing newline
        if (!timeStr.empty() && timeStr.back() == '\n') {
            timeStr.pop_back();
        }
        addOutput(timeStr);
    }
    else if (cmd == "whoami") {
        addOutput("monolith");
    }
    else if (cmd == "version" || cmd == "ver") {
        addOutput("Monolith Terminal v0.1");
        addOutput("Built on SDL2 + custom window manager");
    }
    else if (cmd == "ls") {
        if (m_fs) {
            std::string target = rest.empty() ? m_cwd : resolvePath(rest);
            if (m_fs->isFile(target)) {
                addOutput("• " + m_fs->baseName(target));
            } else if (!m_fs->isDirectory(target)) {
                addOutput("ls: " + (rest.empty() ? target : rest)
                          + ": No such file or directory");
            } else {
                auto entries = m_fs->listEntries(target);
                if (entries.empty()) {
                    addOutput("(empty)");
                } else {
                    for (const auto& e : entries) {
                        std::string prefix = e.isDirectory ? "▶ " : "• ";
                        addOutput(prefix + e.name);
                    }
                }
            }
        } else {
            addOutput("Filesystem not available");
        }
    }
    else if (cmd == "pwd") {
        addOutput(m_cwd);
    }
    else if (cmd == "mv") {
        if (!m_fs) {
            addOutput("Filesystem not available");
        } else {
            // Use args for clean src/dst (index 1,2); support dst-as-dir semantics
            std::string src = (args.size() > 1 ? args[1] : "");
            std::string dst = (args.size() > 2 ? args[2] : "");
            if (src.empty() || dst.empty()) {
                addOutput("mv: missing file operand");
            } else {
                std::string srcPath = resolvePath(src);
                std::string dstPath = resolvePath(dst);
                if (!m_fs->exists(srcPath)) {
                    addOutput("mv: cannot stat '" + src + "': No such file or directory");
                } else {
                    // If dst is an existing directory, place src's basename inside it
                    if (m_fs->isDirectory(dstPath)) {
                        std::string base = srcPath;
                        size_t slash = base.find_last_of('/');
                        if (slash != std::string::npos) base = base.substr(slash + 1);
                        if (base.empty()) base = src;  // fallback
                        dstPath = joinPath(dstPath, base);
                    }
                    if (m_fs->rename(srcPath, dstPath)) {
                        // success
                    } else {
                        addOutput("mv: cannot move '" + src + "' to '" + dst + "'");
                    }
                }
            }
        }
    }
    else if (cmd == "cp") {
        if (!m_fs) {
            addOutput("Filesystem not available");
        } else {
            // Parse with args for -r flag support + clean operands
            bool recursive = false;
            std::vector<std::string> operands;
            for (size_t i = 1; i < args.size(); ++i) {
                const std::string& a = args[i];
                if (a == "-r" || a == "-rf" || a == "-r") {
                    recursive = true;
                } else {
                    operands.push_back(a);
                }
            }
            std::string src = (operands.size() > 0 ? operands[0] : "");
            std::string dst = (operands.size() > 1 ? operands[1] : "");
            if (src.empty() || dst.empty()) {
                addOutput("cp: missing file operand");
            } else {
                std::string srcPath = resolvePath(src);
                std::string dstPath = resolvePath(dst);
                if (!m_fs->exists(srcPath)) {
                    addOutput("cp: cannot stat '" + src + "': No such file or directory");
                } else if (m_fs->isDirectory(srcPath) && !recursive) {
                    addOutput("cp: omitting directory '" + src + "'");
                } else {
                    // If dst exists and is a dir, place source basename inside it (standard cp behavior)
                    if (m_fs->isDirectory(dstPath)) {
                        std::string base = srcPath;
                        size_t slash = base.find_last_of('/');
                        if (slash != std::string::npos) base = base.substr(slash + 1);
                        if (base.empty()) base = src;
                        dstPath = joinPath(dstPath, base);
                    }

                    if (m_fs->isSameOrDescendant(srcPath, dstPath)) {
                        addOutput("cp: cannot copy a directory into itself");
                    } else {
                        bool ok = false;
                        if (recursive && m_fs->isDirectory(srcPath)) {
                            ok = m_fs->copyRecursive(srcPath, dstPath);
                        } else {
                            // Files use the same verified copy path as recursive trees.
                            ok = m_fs->copyRecursive(srcPath, dstPath);
                        }
                        if (!ok) {
                            addOutput("cp: cannot create '" + dst + "'");
                        }
                        // success is silent
                    }
                }
            }
        }
    }
    else if (cmd == "cd") {
        if (!m_fs) {
            addOutput("Filesystem not available");
        } else if (rest.empty()) {
            m_cwd = "/home/monolith";
            addOutput(m_cwd);
        } else {
            std::string newPath = resolvePath(rest);
            if (m_fs->isDirectory(newPath)) {
                m_cwd = newPath;
            } else {
                addOutput("cd: " + rest + ": No such directory");
            }
        }
    }
    else if (cmd == "cat") {
        if (m_fs && !rest.empty()) {
            std::string path = resolvePath(rest);
            if (!m_fs->isFile(path)) {
                addOutput("cat: " + rest + ": No such file");
            } else {
                std::string content;
                if (!m_fs->readFile(path, content)) {
                    addOutput("cat: " + rest + ": Could not read file");
                    return;
                }
                content = normalizeLineEndings(content);
                // Split into scrollback lines so multi-line files render correctly.
                size_t linesOut = 0;
                size_t start = 0;
                while (start <= content.size()) {
                    size_t nl = content.find('\n', start);
                    if (nl == std::string::npos) {
                        addOutput(content.substr(start));
                        ++linesOut;
                        break;
                    }
                    addOutput(content.substr(start, nl - start));
                    ++linesOut;
                    start = nl + 1;
                    if (linesOut >= kMaxCatLines) {
                        addOutput("… cat: output truncated at "
                                  + std::to_string(kMaxCatLines) + " lines");
                        break;
                    }
                }
            }
        } else if (!m_fs) {
            addOutput("Filesystem not available");
        } else {
            addOutput("cat: missing file operand");
        }
    }
    else if (cmd == "mkdir") {
        if (!m_fs) {
            addOutput("Filesystem not available");
        } else if (rest.empty()) {
            addOutput("mkdir: missing operand");
        } else {
            std::string path = resolvePath(rest);
            if (m_fs->exists(path)) {
                addOutput("mkdir: cannot create directory '" + rest + "': File exists");
            } else if (m_fs->createDirectory(path)) {
                // success - silent like real mkdir
            } else {
                addOutput("mkdir: cannot create directory '" + rest + "'");
            }
        }
    }
    else if (cmd == "history") {
        if (m_commandHistory.empty()) {
            addOutput("No commands in history yet.");
        } else {
            for (size_t i = 0; i < m_commandHistory.size(); ++i) {
                std::ostringstream line;
                line << std::setw(4) << (i + 1) << "  " << m_commandHistory[i];
                addOutput(line.str());
            }
        }
    }
    else if (cmd == "rm") {
        if (!m_fs) {
            addOutput("Filesystem not available");
        } else {
            // Robust flag parsing from args (no more brittle substr on rest)
            bool recursive = false;
            std::string target;
            for (size_t i = 1; i < args.size(); ++i) {
                const std::string& a = args[i];
                if (a == "-r" || a == "-rf") {
                    recursive = true;
                } else if (target.empty()) {
                    target = a;
                }
            }
            if (target.empty()) {
                addOutput("rm: missing operand");
            } else {
                std::string path = resolvePath(target);
                if (!m_fs->exists(path)) {
                    addOutput("rm: cannot remove '" + target + "': No such file or directory");
                } else if (m_fs->normalize(path) == "/") {
                    addOutput("rm: cannot remove '/'");
                } else if (recursive) {
                    if (m_fs->removeRecursive(path)) {
                        // success
                    } else {
                        addOutput("rm: failed to remove '" + target + "'");
                    }
                } else if (m_fs->remove(path)) {
                    // success - silent
                } else {
                    addOutput("rm: cannot remove '" + target + "' (use -r for directories)");
                }
            }
        }
    }
    else if (cmd == "edit") {
        if (!m_fs) {
            addOutput("Filesystem not available");
        } else if (rest.empty()) {
            addOutput("edit: missing file operand");
        } else {
            std::string path = resolvePath(rest);
            if (!m_fs->isFile(path)) {
                addOutput("edit: " + rest + ": No such file");
            } else if (auto* ctrl = getController()) {
                ctrl->openInTextEditor(path);
                addOutput("Opened in Text Editor: " + path);
            } else {
                addOutput("edit: cannot open editor (no controller)");
            }
        }
    }
    else if (cmd == "open") {
        if (!m_fs) {
            addOutput("Filesystem not available");
        } else if (rest.empty()) {
            addOutput("open: missing file operand");
        } else {
            std::string path = resolvePath(rest);
            if (!m_fs->isFile(path)) {
                addOutput("open: " + rest + ": No such file");
            } else if (auto* ctrl = getController()) {
                // Shell routes by extension (.modr → Drawing, else Editor).
                ctrl->openPath(path);
                const bool isModr = hasCaseInsensitiveSuffix(path, ".modr");
                addOutput(std::string("Opened with ") +
                          (isModr ? "Drawing: " : "Text Editor: ") + path);
            } else {
                addOutput("open: cannot open file (no controller)");
            }
        }
    }
    else if (cmd == "touch") {
        if (!m_fs) {
            addOutput("Filesystem not available");
        } else if (rest.empty()) {
            addOutput("touch: missing file operand");
        } else {
            std::string path = resolvePath(rest);
            if (m_fs->isDirectory(path)) {
                addOutput("touch: cannot touch '" + rest + "': Is a directory");
            } else if (m_fs->isFile(path)) {
                // Already exists: leave content unchanged (do not truncate).
            } else if (m_fs->writeFile(path, "")) {
                // Created empty file.
            } else {
                addOutput("touch: cannot touch '" + rest + "'");
            }
        }
    }
    else if (cmd == "exit" || cmd == "quit") {
        if (auto* ctrl = getController()) {
            ctrl->close();
        } else {
            addOutput("Cannot close window (no controller)");
        }
    }
    else {
        addOutput("Unknown command: " + cmd);
        addOutput("Type 'help' for available commands.");
    }
}

void TerminalApp::processTextInput(const char* text) {
    if (m_searchMode) {
        if (text && *text) {
            m_searchCursorPos = std::min(m_searchCursorPos, m_searchBuffer.size());
            const std::string inserted = text;
            m_searchBuffer.insert(m_searchCursorPos, inserted);
            m_searchCursorPos += inserted.size();
            updateReverseSearchMatch();
        }
        return;
    }

    if (text && *text) {
        m_inputCursorPos = std::clamp(
            m_inputCursorPos,
            0,
            static_cast<int>(m_inputBuffer.size())
        );
        m_inputBuffer.insert(m_inputCursorPos, text);
        m_inputCursorPos += strlen(text);
    }

    // Safety clamp
    if (m_inputCursorPos < 0) m_inputCursorPos = 0;
    if (m_inputCursorPos > static_cast<int>(m_inputBuffer.size())) {
        m_inputCursorPos = static_cast<int>(m_inputBuffer.size());
    }
}

void TerminalApp::handleKeyDown(const SDL_Keysym& keysym) {
    // === Reverse search mode special handling ===
    if (m_searchMode) {
        if (keysym.sym == SDLK_RETURN || keysym.sym == SDLK_KP_ENTER) {
            exitReverseSearch(true);   // accept match
            return;
        }
        if (keysym.sym == SDLK_ESCAPE) {
            exitReverseSearch(false);  // cancel
            return;
        }
        if (keysym.sym == SDLK_BACKSPACE) {
            erasePreviousUtf8Codepoint(m_searchBuffer, m_searchCursorPos);
            updateReverseSearchMatch();
            return;
        }
        if (keysym.sym == SDLK_DELETE) {
            m_searchCursorPos = std::min(m_searchCursorPos, m_searchBuffer.size());
            const std::size_t next = utf8NextCodepointStart(m_searchBuffer, m_searchCursorPos);
            if (next > m_searchCursorPos) {
                m_searchBuffer.erase(m_searchCursorPos, next - m_searchCursorPos);
                updateReverseSearchMatch();
            }
            return;
        }
        if (keysym.sym == SDLK_LEFT) {
            m_searchCursorPos = utf8PrevCodepointStart(m_searchBuffer, m_searchCursorPos);
            return;
        }
        if (keysym.sym == SDLK_RIGHT) {
            m_searchCursorPos = utf8NextCodepointStart(m_searchBuffer, m_searchCursorPos);
            return;
        }
        if (keysym.sym == SDLK_HOME) {
            m_searchCursorPos = 0;
            return;
        }
        if (keysym.sym == SDLK_END) {
            m_searchCursorPos = m_searchBuffer.size();
            return;
        }
        // Ctrl+R while searching → find older match
        if ((keysym.mod & KMOD_CTRL) && keysym.sym == SDLK_r) {
            searchPreviousMatch();
            return;
        }
        // Up/down cancel search so the restored input can use history navigation.
        if (keysym.sym == SDLK_UP || keysym.sym == SDLK_DOWN) {
            exitReverseSearch(false);
            // Fall through to normal handling of that key on the restored input
        } else {
            return; // swallow other keys while in search
        }
    }

    switch (keysym.sym) {
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
            submitInput();
            break;

        case SDLK_BACKSPACE:
            if (m_inputCursorPos > 0) {
                std::size_t cursor = static_cast<std::size_t>(m_inputCursorPos);
                erasePreviousUtf8Codepoint(m_inputBuffer, cursor);
                m_inputCursorPos = static_cast<int>(cursor);
            }
            break;

        case SDLK_DELETE: {
            const std::size_t cursor = static_cast<std::size_t>(
                std::clamp(m_inputCursorPos, 0, static_cast<int>(m_inputBuffer.size())));
            const std::size_t next = utf8NextCodepointStart(m_inputBuffer, cursor);
            if (next > cursor) {
                m_inputBuffer.erase(cursor, next - cursor);
            }
            break;
        }

        case SDLK_ESCAPE:
            m_inputBuffer.clear();
            m_inputCursorPos = 0;
            m_historyIndex = -1;
            m_savedInputBuffer.clear();
            break;

        case SDLK_UP:
            if (!m_commandHistory.empty()) {
                if (m_historyIndex == -1) {
                    m_savedInputBuffer = m_inputBuffer;
                    m_historyIndex = static_cast<int>(m_commandHistory.size()) - 1;
                } else if (m_historyIndex > 0) {
                    m_historyIndex--;
                }
                m_inputBuffer = m_commandHistory[m_historyIndex];
                m_inputCursorPos = static_cast<int>(m_inputBuffer.size());
            }
            break;

        case SDLK_DOWN:
            if (m_historyIndex != -1) {
                m_historyIndex++;
                if (m_historyIndex >= static_cast<int>(m_commandHistory.size())) {
                    m_historyIndex = -1;
                    m_inputBuffer = m_savedInputBuffer;
                    m_inputCursorPos = static_cast<int>(m_inputBuffer.size());
                } else {
                    m_inputBuffer = m_commandHistory[m_historyIndex];
                    m_inputCursorPos = static_cast<int>(m_inputBuffer.size());
                }
            }
            break;

        case SDLK_LEFT:
            if (m_inputCursorPos > 0) {
                m_inputCursorPos = static_cast<int>(utf8PrevCodepointStart(
                    m_inputBuffer, static_cast<std::size_t>(m_inputCursorPos)));
            }
            break;

        case SDLK_RIGHT:
            if (m_inputCursorPos < static_cast<int>(m_inputBuffer.size())) {
                m_inputCursorPos = static_cast<int>(utf8NextCodepointStart(
                    m_inputBuffer, static_cast<std::size_t>(m_inputCursorPos)));
            }
            break;

        case SDLK_HOME:
            m_inputCursorPos = 0;
            break;

        case SDLK_END:
            m_inputCursorPos = static_cast<int>(m_inputBuffer.size());
            break;

        case SDLK_PAGEUP:
            scrollHistory(3);
            break;

        case SDLK_PAGEDOWN:
            scrollHistory(-3);
            break;

        case SDLK_TAB:
            if (!m_searchMode) {
                handleTabCompletion();
            }
            break;

        default:
            break;
    }

    // Global Ctrl+R triggers reverse search (when not already handled above)
    if ((keysym.mod & KMOD_CTRL) && keysym.sym == SDLK_r) {
        enterReverseSearch();
    }

    // Safety clamp
    if (m_inputCursorPos < 0) m_inputCursorPos = 0;
    if (m_inputCursorPos > static_cast<int>(m_inputBuffer.size())) {
        m_inputCursorPos = static_cast<int>(m_inputBuffer.size());
    }
}

#include "TerminalApp_rest.inc"
