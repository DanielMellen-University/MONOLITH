#include "TerminalApp.hpp"
#include <algorithm>
#include <cctype>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace monolith::app {

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
        saveCommandHistory();
        executeCommand(command);
    } else {
        addOutput(""); // blank line for empty input
    }
    m_scrollOffset = 0;   // always jump back to bottom after running a command
}

void TerminalApp::executeCommand(const std::string& commandLine) {
    // Build arg list (simple whitespace split; enables reliable flag + operand handling for rm/cp/mv).
    // Still no quoting for filenames containing spaces (accepted current limitation).
    std::vector<std::string> args;
    {
        std::istringstream iss(commandLine);
        std::string tok;
        while (iss >> tok) {
            args.push_back(tok);
        }
    }

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
            auto entries = m_fs->listEntries(target);
            if (entries.empty()) {
                addOutput("(empty)");
            } else {
                for (const auto& e : entries) {
                    std::string prefix = e.isDirectory ? "▶ " : "• ";
                    addOutput(prefix + e.name);
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

                    bool ok = false;
                    if (recursive && m_fs->isDirectory(srcPath)) {
                        ok = copyRecursive(srcPath, dstPath);
                    } else {
                        // file (or non-recursive dir would have been caught above)
                        std::string content = m_fs->readFile(srcPath);
                        ok = m_fs->writeFile(dstPath, content);
                    }
                    if (!ok) {
                        addOutput("cp: cannot create '" + dst + "'");
                    }
                    // success is silent
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
            std::string content = m_fs->readFile(path);
            if (!content.empty() || m_fs->isFile(path)) {
                addOutput(content);
            } else {
                addOutput("cat: " + rest + ": No such file");
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
                } else if (recursive) {
                    if (removeRecursive(path)) {
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
                if (path.size() >= 5 && path.compare(path.size() - 5, 5, ".modr") == 0) {
                    ctrl->openInDrawing(path);
                    addOutput("Opened in Drawing: " + path);
                } else {
                    ctrl->openInTextEditor(path);
                    addOutput("Opened in Text Editor: " + path);
                }
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
            if (m_fs->writeFile(path, "")) {
                // success - silent or could stat it
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
            m_searchBuffer += text;
            updateReverseSearchMatch();
        }
        return;
    }

    if (text && *text) {
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
            if (!m_searchBuffer.empty()) {
                m_searchBuffer.pop_back();
                updateReverseSearchMatch();
            }
            return;
        }
        // Ctrl+R while searching → find older match
        if ((keysym.mod & KMOD_CTRL) && keysym.sym == SDLK_r) {
            searchPreviousMatch();
            return;
        }
        // Any other key (arrows, etc.) cancels search for now
        if (keysym.sym == SDLK_LEFT || keysym.sym == SDLK_RIGHT ||
            keysym.sym == SDLK_UP   || keysym.sym == SDLK_DOWN ||
            keysym.sym == SDLK_HOME || keysym.sym == SDLK_END) {
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
                m_inputBuffer.erase(m_inputCursorPos - 1, 1);
                m_inputCursorPos--;
            }
            break;

        case SDLK_ESCAPE:
            // Could clear input in future
            m_inputBuffer.clear();
            m_historyIndex = -1;
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
            if (m_inputCursorPos > 0) m_inputCursorPos--;
            break;

        case SDLK_RIGHT:
            if (m_inputCursorPos < static_cast<int>(m_inputBuffer.size())) m_inputCursorPos++;
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

void TerminalApp::scrollHistory(int delta) {
    int visible = getMaxVisibleLines({0, 0, m_clientWidth, m_clientHeight});
    int maxScroll = static_cast<int>(m_history.size()) - visible + 2;
    if (maxScroll < 0) maxScroll = 0;

    m_scrollOffset += delta;
    if (m_scrollOffset < 0) m_scrollOffset = 0;
    if (m_scrollOffset > maxScroll) m_scrollOffset = maxScroll;
}

void TerminalApp::handleMouseWheel(const SDL_MouseWheelEvent& e) {
    // Positive e.y = scroll up (toward older history)
    scrollHistory(e.y * 2);
}

void TerminalApp::enterReverseSearch() {
    if (m_searchMode) {
        // Already in search — pressing Ctrl+R again should find older match
        searchPreviousMatch();
        return;
    }

    m_searchMode = true;
    m_searchBuffer.clear();
    m_searchSavedInput = m_inputBuffer;
    m_searchMatchIndex = -1;
    updateReverseSearchMatch();
}

void TerminalApp::exitReverseSearch(bool accept) {
    if (!m_searchMode) return;

    if (accept && m_searchMatchIndex >= 0 && m_searchMatchIndex < static_cast<int>(m_commandHistory.size())) {
        m_inputBuffer = m_commandHistory[m_searchMatchIndex];
        m_inputCursorPos = static_cast<int>(m_inputBuffer.size());
    } else {
        // Cancel: restore previous input
        m_inputBuffer = m_searchSavedInput;
        m_inputCursorPos = static_cast<int>(m_inputBuffer.size());
    }

    m_searchMode = false;
    m_searchBuffer.clear();
    m_searchMatchIndex = -1;
    m_searchSavedInput.clear();
    m_scrollOffset = 0;
}

void TerminalApp::updateReverseSearchMatch() {
    m_searchMatchIndex = -1;

    if (m_searchBuffer.empty()) {
        return;
    }

    // Search backward from the end (or from current match - 1 if repeating)
    int start = (m_searchMatchIndex >= 0) ? m_searchMatchIndex - 1 : static_cast<int>(m_commandHistory.size()) - 1;

    for (int i = start; i >= 0; --i) {
        if (m_commandHistory[i].find(m_searchBuffer) != std::string::npos) {
            m_searchMatchIndex = i;
            break;
        }
    }
}

void TerminalApp::searchPreviousMatch() {
    if (!m_searchMode) return;

    // Search for an older match than the current one
    int start = (m_searchMatchIndex > 0) ? m_searchMatchIndex - 1 : -1;

    for (int i = start; i >= 0; --i) {
        if (m_commandHistory[i].find(m_searchBuffer) != std::string::npos) {
            m_searchMatchIndex = i;
            return;
        }
    }
    // No older match found — keep current
}

void TerminalApp::loadCommandHistory() {
    if (!m_fs) return;

    std::string content = m_fs->readFile(HISTORY_FILE);
    if (content.empty()) return;

    std::istringstream iss(content);
    std::string line;
    while (std::getline(iss, line)) {
        if (!line.empty()) {
            m_commandHistory.push_back(line);
        }
    }
}

void TerminalApp::saveCommandHistory() {
    if (!m_fs) return;

    std::ostringstream oss;
    for (const auto& cmd : m_commandHistory) {
        oss << cmd << '\n';
    }
    m_fs->writeFile(HISTORY_FILE, oss.str());
}

bool TerminalApp::removeRecursive(const std::string& virtualPath) {
    if (!m_fs) return false;

    if (m_fs->isFile(virtualPath)) {
        return m_fs->remove(virtualPath);
    }

    if (!m_fs->isDirectory(virtualPath)) {
        return false;
    }

    auto entries = m_fs->listEntries(virtualPath);
    for (const auto& entry : entries) {
        std::string child = joinPath(virtualPath, entry.name);
        if (!removeRecursive(child)) {
            return false;
        }
    }

    return m_fs->remove(virtualPath);
}

void TerminalApp::handleTabCompletion() {
    if (m_inputCursorPos == 0) return;

    // Only complete up to the cursor position
    std::string textBeforeCursor = m_inputBuffer.substr(0, m_inputCursorPos);

    // Find the start of the current word
    size_t spacePos = textBeforeCursor.find_last_of(' ');
    std::string prefix;
    size_t wordStart = 0;

    if (spacePos != std::string::npos) {
        wordStart = spacePos + 1;
        prefix = textBeforeCursor.substr(wordStart);
    } else {
        prefix = textBeforeCursor;
    }

    if (prefix.empty()) return;

    std::vector<std::string> matches;

    bool isFirstWord = (spacePos == std::string::npos);

    if (isFirstWord) {
        matches = getCommandCompletions(prefix);
    } else {
        matches = getPathCompletions(prefix);
    }

    if (matches.empty()) return;

    if (matches.size() == 1) {
        // Single match - complete it
        std::string completion = matches[0];
        std::string newWord = completion;

        // For paths, append / if it's a directory
        if (!isFirstWord) {
            std::string fullPath = resolvePath(newWord);
            if (m_fs && m_fs->isDirectory(fullPath) && !newWord.empty() && newWord.back() != '/') {
                newWord += "/";
            }
        }

        // Replace the partial word
        m_inputBuffer.replace(wordStart, prefix.size(), newWord);
        m_inputCursorPos = wordStart + newWord.size();
    } else {
        // Multiple matches - complete common prefix if possible
        std::string common = prefix;
        for (const auto& m : matches) {
            size_t len = std::min(common.size(), m.size());
            while (len > common.size() || common.substr(0, len) != m.substr(0, len)) {
                len--;
            }
            common = common.substr(0, len);
        }

        if (common.size() > prefix.size()) {
            m_inputBuffer.replace(wordStart, prefix.size(), common);
            m_inputCursorPos = wordStart + common.size();
        } else {
            // Show possible completions in output
            addOutput("Possible completions:");
            for (const auto& m : matches) {
                addOutput("  " + m);
            }
        }
    }
}

std::vector<std::string> TerminalApp::getCommandCompletions(const std::string& prefix) const {
    static const std::vector<std::string> commands = {
        "echo", "clear", "help", "date", "whoami", "version", "ver",
        "ls", "pwd", "cd", "mkdir", "touch", "cp", "rm", "mv",
        "cat", "edit", "open", "history", "exit", "quit"
    };

    std::vector<std::string> matches;
    for (const auto& cmd : commands) {
        if (cmd.size() >= prefix.size() && cmd.compare(0, prefix.size(), prefix) == 0) {
            matches.push_back(cmd);
        }
    }
    return matches;
}

std::vector<std::string> TerminalApp::getPathCompletions(const std::string& partial) const {
    if (!m_fs) return {};

    // Split partial into dir part and file part
    std::string dirPart;
    std::string filePrefix = partial;

    size_t lastSlash = partial.find_last_of('/');
    if (lastSlash != std::string::npos) {
        dirPart = partial.substr(0, lastSlash);
        filePrefix = partial.substr(lastSlash + 1);
    }

    std::string searchDir = dirPart.empty() ? m_cwd : resolvePath(dirPart);

    auto entries = m_fs->listEntries(searchDir);
    std::vector<std::string> matches;

    std::string base = dirPart.empty() ? "" : dirPart + "/";

    for (const auto& e : entries) {
        if (e.name.size() >= filePrefix.size() &&
            e.name.compare(0, filePrefix.size(), filePrefix) == 0) {

            std::string completion = base + e.name;
            if (e.isDirectory) {
                // We can let the caller decide to add / or not
            }
            matches.push_back(completion);
        }
    }

    return matches;
}

void TerminalApp::handleEvent(const SDL_Event& event) {
    if (event.type == SDL_TEXTINPUT) {
        processTextInput(event.text.text);
    }
    else if (event.type == SDL_KEYDOWN) {
        // Ignore key repeats for now or handle as needed
        handleKeyDown(event.key.keysym);
    }
    else if (event.type == SDL_MOUSEWHEEL) {
        handleMouseWheel(event.wheel);
    }
}

int TerminalApp::getLineHeight() const {
    if (!m_font) return 18;
    return TTF_FontHeight(m_font);
}

int TerminalApp::getMaxVisibleLines(const SDL_Rect& contentRect) const {
    int lh = getLineHeight();
    if (lh <= 0) return 10;
    return std::max(3, (contentRect.h - 20) / lh); // small padding
}

std::string TerminalApp::resolvePath(const std::string& path) const {
    if (!m_fs) return path;

    std::string target = path;
    if (target.empty()) {
        target = m_cwd;
    } else if (target[0] != '/') {
        // relative path - centralize glue via joinPath
        target = joinPath(m_cwd, target);
    }

    return m_fs->normalize(target);
}

std::string TerminalApp::getInputPrompt() const {
    // Show abbreviated cwd + prompt symbol
    std::string displayCwd = m_cwd;
    if (displayCwd == "/home/monolith") {
        displayCwd = "~";
    } else if (displayCwd.rfind("/home/monolith/", 0) == 0) {
        displayCwd = "~" + displayCwd.substr(14);  // after /home/monolith
    }
    return displayCwd + "> ";
}

std::string TerminalApp::joinPath(const std::string& base, const std::string& name) const {
    if (!m_fs) return name;
    if (name.empty()) return m_fs->normalize(base.empty() ? "/" : base);
    if (base.empty()) return m_fs->normalize(name);

    std::string glued = base;
    if (!glued.empty() && glued.back() != '/') {
        glued += '/';
    }
    glued += name;
    return m_fs->normalize(glued);
}

bool TerminalApp::copyRecursive(const std::string& src, const std::string& dst) {
    if (!m_fs) return false;

    if (m_fs->isFile(src)) {
        std::string content = m_fs->readFile(src);
        return m_fs->writeFile(dst, content);
    }

    if (!m_fs->isDirectory(src)) return false;

    // Ensure destination dir (createDirectory is safe if it already exists as dir in our FS impl)
    if (!m_fs->createDirectory(dst)) {
        // If it exists but is a file, writeFile later would fail anyway; treat as failure for copy
        if (!m_fs->isDirectory(dst)) return false;
    }

    auto entries = m_fs->listEntries(src);
    for (const auto& entry : entries) {
        std::string childSrc = joinPath(src, entry.name);
        std::string childDst = joinPath(dst, entry.name);
        if (!copyRecursive(childSrc, childDst)) {
            return false;
        }
    }
    return true;
}

void TerminalApp::render(SDL_Renderer* renderer, const SDL_Rect& contentRect) {
    if (!m_font) {
        // Fallback: just draw dark background
        SDL_SetRenderDrawColor(renderer, 15, 15, 18, 255);
        SDL_RenderFillRect(renderer, &contentRect);
        return;
    }

    // Terminal background
    SDL_SetRenderDrawColor(renderer, 18, 18, 22, 255);
    SDL_RenderFillRect(renderer, &contentRect);

    const int lineHeight = getLineHeight();
    const int padding = 8;

    // === Robust input area reservation (critical for maximized windows) ===
    // We reserve a fixed strip at the very bottom of the content area for the input.
    // This must never be eaten into by history, even when the window is very tall.
    const int inputStripHeight = lineHeight + 14;   // prompt + padding + breathing room
    const int inputBottomY = contentRect.y + contentRect.h - 6; // small margin from absolute bottom
    const int inputLineY = inputBottomY - lineHeight;

    // History gets everything above the input strip
    SDL_Rect historyRect = {
        contentRect.x + padding,
        contentRect.y + padding,
        contentRect.w - padding * 2,
        contentRect.h - padding * 2 - inputStripHeight - 4
    };

    // Draw input background strip (clearly separated)
    SDL_SetRenderDrawColor(renderer, 28, 28, 34, 255);
    SDL_Rect inputBar = {
        contentRect.x + 4,
        inputLineY - 4,
        contentRect.w - 8,
        inputStripHeight
    };
    SDL_RenderFillRect(renderer, &inputBar);

    SDL_Color textColor = {200, 205, 210, 255};

    // Draw input line (normal or reverse-search mode)
    if (m_searchMode) {
        // Reverse-i-search UI
        std::string searchPrompt = "(reverse-i-search)`" + m_searchBuffer + "': ";
        std::string matchText;

        if (m_searchMatchIndex >= 0 && m_searchMatchIndex < static_cast<int>(m_commandHistory.size())) {
            matchText = m_commandHistory[m_searchMatchIndex];
        } else if (!m_searchBuffer.empty()) {
            matchText = "(no match)";
        }

        std::string full = searchPrompt + matchText;

        SDL_Surface* surf = TTF_RenderUTF8_Blended(m_font, full.c_str(), textColor);
        if (surf) {
            SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
            if (tex) {
                SDL_Rect dst = {contentRect.x + padding, inputLineY, surf->w, surf->h};
                SDL_RenderCopy(renderer, tex, nullptr, &dst);
                SDL_DestroyTexture(tex);
            }
            SDL_FreeSurface(surf);
        }

        // Cursor on the search buffer part
        int cursorX = contentRect.x + padding + (int)(searchPrompt.size() - 1) * 8; // rough monospace estimate
        // Better: measure the actual text width up to the end of searchBuffer
        {
            std::string prefix = "(reverse-i-search)`" + m_searchBuffer;
            int w = 0, h = 0;
            TTF_SizeUTF8(m_font, prefix.c_str(), &w, &h);
            cursorX = contentRect.x + padding + w;
        }

        SDL_SetRenderDrawColor(renderer, 180, 200, 180, 230);
        SDL_Rect cur = {cursorX, inputLineY + 2, 8, lineHeight - 4};
        SDL_RenderFillRect(renderer, &cur);
    } else {
        // Normal prompt + input with cursor
        std::string prompt = getInputPrompt();
        std::string beforeCursor = prompt + m_inputBuffer.substr(0, m_inputCursorPos);
        std::string afterCursor  = m_inputBuffer.substr(m_inputCursorPos);

        // Draw text before cursor
        SDL_Surface* surf = TTF_RenderUTF8_Blended(m_font, beforeCursor.c_str(), textColor);
        int cursorX = contentRect.x + padding;
        if (surf) {
            SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
            if (tex) {
                SDL_Rect dst = {cursorX, inputLineY, surf->w, surf->h};
                SDL_RenderCopy(renderer, tex, nullptr, &dst);
                cursorX += surf->w;
                SDL_DestroyTexture(tex);
            }
            SDL_FreeSurface(surf);
        }

        // Draw cursor block
        int cursorW = 8;
        SDL_SetRenderDrawColor(renderer, 180, 200, 180, 230);
        SDL_Rect cursorRect = {cursorX, inputLineY + 2, cursorW, lineHeight - 4};
        SDL_RenderFillRect(renderer, &cursorRect);

        // Draw text after cursor
        if (!afterCursor.empty()) {
            SDL_Surface* surf2 = TTF_RenderUTF8_Blended(m_font, afterCursor.c_str(), textColor);
            if (surf2) {
                SDL_Texture* tex2 = SDL_CreateTextureFromSurface(renderer, surf2);
                if (tex2) {
                    SDL_Rect dst2 = {cursorX + cursorW, inputLineY, surf2->w, surf2->h};
                    SDL_RenderCopy(renderer, tex2, nullptr, &dst2);
                    SDL_DestroyTexture(tex2);
                }
                SDL_FreeSurface(surf2);
            }
        }
    }

    // Draw history, respecting scroll offset (0 = bottom/newest)
    if (!m_history.empty()) {
        int total = static_cast<int>(m_history.size());
        int startIdx = total - 1 - m_scrollOffset;
        if (startIdx < 0) startIdx = 0;

        int y = historyRect.y + historyRect.h - lineHeight;

        for (int i = startIdx; i >= 0 && y >= historyRect.y; --i) {
            const std::string& line = m_history[i];

            SDL_Surface* surf = TTF_RenderUTF8_Blended(m_font, line.c_str(), textColor);
            if (surf) {
                SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
                if (tex) {
                    SDL_Rect dst = {
                        historyRect.x,
                        y,
                        std::min(surf->w, historyRect.w),
                        surf->h
                    };
                    SDL_RenderCopy(renderer, tex, nullptr, &dst);
                    SDL_DestroyTexture(tex);
                }
                SDL_FreeSurface(surf);
            }

            y -= lineHeight;
        }
    }

    // Note: Cursor for input is drawn earlier in the input strip section above.
    // The old duplicate cursor block that used full-buffer calculation was removed.
}

void TerminalApp::onResize(int clientWidth, int clientHeight) {
    m_clientWidth = clientWidth;
    m_clientHeight = clientHeight;
    // Keep scroll within bounds after resize
    int maxScroll = static_cast<int>(m_history.size()) - getMaxVisibleLines({0,0, m_clientWidth, m_clientHeight}) + 2;
    if (maxScroll < 0) maxScroll = 0;
    if (m_scrollOffset > maxScroll) m_scrollOffset = maxScroll;
}

} // namespace monolith::app
