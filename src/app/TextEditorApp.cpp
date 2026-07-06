#include "TextEditorApp.hpp"
#include <algorithm>
#include <cctype>
#include <sstream>
#include <unordered_set>
#include <vector>

namespace monolith::app {

namespace {

std::string commonPrefix(const std::vector<std::string>& values) {
    if (values.empty()) return "";

    std::string common = values.front();
    for (size_t i = 1; i < values.size(); ++i) {
        size_t len = 0;
        while (len < common.size() && len < values[i].size() && common[len] == values[i][len]) {
            ++len;
        }
        common.resize(len);
        if (common.empty()) break;
    }
    return common;
}

bool isCodeExtension(const std::string& ext) {
    static const std::unordered_set<std::string> kCodeExtensions = {
        "c", "cc", "cpp", "cxx", "h", "hh", "hpp", "hxx",
        "py", "js", "ts", "rs", "go", "java", "cs", "lua",
        "sh", "bash", "zsh", "json", "yaml", "yml", "toml",
        "xml", "html", "css", "md"
    };
    return kCodeExtensions.count(ext) > 0;
}

bool isSingleQuoteStringStart(const std::string& line, size_t index) {
    if (index >= line.size() || line[index] != '\'') return false;

    if (index > 0) {
        const unsigned char prev = static_cast<unsigned char>(line[index - 1]);
        if (std::isalnum(prev) || line[index - 1] == '_') {
            return false;
        }
    }

    for (size_t j = index + 1; j < line.size(); ++j) {
        if (line[j] == '\\' && j + 1 < line.size()) {
            ++j;
            continue;
        }
        if (line[j] == '\'') {
            return true;
        }
    }
    return false;
}

bool isSyntaxKeyword(const std::string& word) {
    static const std::unordered_set<std::string> kKeywords = {
        "if", "else", "elif", "endif", "while", "for", "do", "switch", "case",
        "break", "continue", "return", "true", "false", "null", "nil", "none",
        "function", "fn", "def", "class", "struct", "enum", "import", "include",
        "let", "var", "const", "int", "float", "double", "bool", "boolean",
        "string", "void", "and", "or", "not", "in", "new", "delete", "this",
        "public", "private", "protected", "static", "virtual", "override",
        "namespace", "using", "typedef", "template", "typename", "sizeof",
        "try", "catch", "throw", "finally", "async", "await", "yield",
        "print", "println", "echo"
    };
    return kKeywords.count(word) > 0;
}

} // namespace

TextEditorApp::TextEditorApp(TTF_Font* font, monolith::fs::Filesystem* fs, const std::string& initialPath)
    : m_font(font), m_fs(fs)
{
    m_lines = { "" };

    if (!initialPath.empty() && m_fs) {
        loadInitialFile(initialPath);
    }

    // Start with a small welcome buffer if nothing loaded
    if (m_lines.size() == 1 && m_lines[0].empty() && m_filePath.empty()) {
        m_lines[0] = "Welcome to the Monolith Text Editor!";
        m_lines.emplace_back("");
        m_lines.emplace_back("Basic controls:");
        m_lines.emplace_back("  - Type to insert text");
        m_lines.emplace_back("  - Enter for new line");
        m_lines.emplace_back("  - Arrow keys, Home, End to move");
        m_lines.emplace_back("  - Backspace / Delete to remove text");
        m_lines.emplace_back("  - Ctrl+S to save");
        m_lines.emplace_back("  - Ctrl+O to open by path");
        m_lines.emplace_back("  - Ctrl+Shift+S to save as");
        m_lines.emplace_back("  - Ctrl+Z / Ctrl+Y to undo / redo");
        m_lines.emplace_back("");
        m_lines.emplace_back("This editor is very early. More features coming.");
        m_cursorRow = 0;
        m_cursorCol = 0;
    }

    refreshSyntaxMode();
}

TextEditorApp::SyntaxMode TextEditorApp::syntaxModeForPath(const std::string& path) const {
    if (path.empty()) return SyntaxMode::Light;

    const size_t dot = path.find_last_of('.');
    if (dot == std::string::npos || dot + 1 >= path.size()) {
        return SyntaxMode::Light;
    }

    std::string ext = path.substr(dot + 1);
    for (char& c : ext) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return isCodeExtension(ext) ? SyntaxMode::Code : SyntaxMode::Light;
}

void TextEditorApp::refreshSyntaxMode() {
    m_syntaxMode = syntaxModeForPath(m_filePath);
}

std::vector<TextEditorApp::ColoredSpan> TextEditorApp::tokenizeLine(const std::string& line) const {
    static const SDL_Color kNormal   = {200, 205, 210, 255};
    static const SDL_Color kComment  = {120, 145, 120, 255};
    static const SDL_Color kString   = {220, 175, 115, 255};
    static const SDL_Color kNumber   = {180, 155, 225, 255};
    static const SDL_Color kKeyword  = {115, 170, 225, 255};

    std::vector<ColoredSpan> spans;
    const size_t n = line.size();
    size_t i = 0;

    auto appendSpan = [&](size_t start, size_t length, const SDL_Color& color) {
        if (length == 0) return;
        if (!spans.empty()
            && spans.back().color.r == color.r
            && spans.back().color.g == color.g
            && spans.back().color.b == color.b
            && spans.back().color.a == color.a
            && spans.back().start + spans.back().length == start) {
            spans.back().length += length;
            return;
        }
        spans.push_back({start, length, color});
    };

    auto emitNormalRun = [&](size_t start, size_t end) {
        if (end > start) {
            appendSpan(start, end - start, kNormal);
        }
    };

    while (i < n) {
        if (i + 1 < n && line[i] == '/' && line[i + 1] == '/') {
            appendSpan(i, n - i, kComment);
            break;
        }
        if (line[i] == '#') {
            appendSpan(i, n - i, kComment);
            break;
        }

        const bool doubleQuote = (line[i] == '"');
        const bool singleQuote = (line[i] == '\'' && m_syntaxMode == SyntaxMode::Code
                                  && isSingleQuoteStringStart(line, i));
        if (doubleQuote || singleQuote) {
            const char quote = line[i];
            size_t j = i + 1;
            while (j < n) {
                if (line[j] == '\\' && j + 1 < n) {
                    j += 2;
                    continue;
                }
                if (line[j] == quote) {
                    ++j;
                    break;
                }
                ++j;
            }
            appendSpan(i, j - i, kString);
            i = j;
            continue;
        }

        const unsigned char ch = static_cast<unsigned char>(line[i]);
        if (std::isdigit(ch)
            || (line[i] == '.' && i + 1 < n
                && std::isdigit(static_cast<unsigned char>(line[i + 1])))) {
            size_t j = i;
            if (line[j] == '-') ++j;
            while (j < n) {
                const unsigned char cj = static_cast<unsigned char>(line[j]);
                if (!std::isdigit(cj) && line[j] != '.') break;
                ++j;
            }
            appendSpan(i, j - i, kNumber);
            i = j;
            continue;
        }

        if (std::isalpha(ch) || line[i] == '_') {
            size_t j = i + 1;
            while (j < n) {
                const unsigned char cj = static_cast<unsigned char>(line[j]);
                if (!std::isalnum(cj) && line[j] != '_') break;
                ++j;
            }
            const std::string word = line.substr(i, j - i);
            if (m_syntaxMode == SyntaxMode::Code && isSyntaxKeyword(word)) {
                appendSpan(i, j - i, kKeyword);
            } else {
                appendSpan(i, j - i, kNormal);
            }
            i = j;
            continue;
        }

        emitNormalRun(i, i + 1);
        ++i;
    }

    if (spans.empty() && !line.empty()) {
        appendSpan(0, line.size(), kNormal);
    }

    return spans;
}

void TextEditorApp::drawColoredLine(SDL_Renderer* renderer, const std::string& line, int x, int y,
                                    int maxWidth) const {
    if (!m_font || line.empty()) return;

    const auto spans = tokenizeLine(line);
    int curX = x;
    const int rightEdge = x + maxWidth;

    for (const auto& span : spans) {
        if (curX >= rightEdge) break;
        if (span.start >= line.size()) continue;

        const size_t available = line.size() - span.start;
        const size_t len = std::min(span.length, available);
        if (len == 0) continue;

        const std::string text = line.substr(span.start, len);
        SDL_Surface* surf = TTF_RenderText_Blended(m_font, text.c_str(), span.color);
        if (!surf) continue;

        SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
        if (tex) {
            const int drawW = std::min(surf->w, rightEdge - curX);
            SDL_Rect dst = {curX, y, drawW, surf->h};
            SDL_RenderCopy(renderer, tex, nullptr, &dst);
            SDL_DestroyTexture(tex);
            curX += surf->w;
        }
        SDL_FreeSurface(surf);
    }
}

void TextEditorApp::loadInitialFile(const std::string& virtualPath) {
    if (!m_fs) return;

    std::string normalized = m_fs->normalize(virtualPath);
    if (m_fs->isFile(normalized)) {
        std::string content = m_fs->readFile(normalized);
        if (!content.empty() || m_fs->isFile(normalized)) {
            m_lines.clear();
            std::istringstream iss(content);
            std::string line;
            while (std::getline(iss, line)) {
                // getline strips the newline; if the file ends with a newline we get an extra empty line naturally
                m_lines.push_back(line);
            }
            if (content.empty() || content.back() == '\n') {
                m_lines.emplace_back("");
            }
            if (m_lines.empty()) {
                m_lines = { "" };
            }
            m_filePath = normalized;
            m_cursorRow = 0;
            m_cursorCol = 0;
            m_dirty = false;
            refreshSyntaxMode();
        }
    }
}

bool TextEditorApp::saveCurrentFile() {
    if (!m_fs || m_filePath.empty()) {
        return false;
    }

    std::ostringstream oss;
    for (size_t i = 0; i < m_lines.size(); ++i) {
        oss << m_lines[i];
        if (i + 1 < m_lines.size()) {
            oss << '\n';
        }
    }
    // If last line is non-empty we may want a trailing newline? For now match common behavior: no forced trailing newline unless present.

    bool ok = m_fs->writeFile(m_filePath, oss.str());
    if (ok) {
        m_dirty = false;
    }
    return ok;
}

std::string TextEditorApp::getDisplayName() const {
    if (m_filePath.empty()) return "Untitled";
    size_t pos = m_filePath.find_last_of('/');
    if (pos != std::string::npos && pos + 1 < m_filePath.size()) {
        return m_filePath.substr(pos + 1);
    }
    return m_filePath;
}

void TextEditorApp::updateTitleForPath() {
    if (m_filePath.empty()) return;
    if (auto* ctrl = getController()) {
        ctrl->setTitle("Editor - " + getDisplayName());
    }
}

void TextEditorApp::beginPathPrompt(PathPromptMode mode) {
    m_pathPromptMode = mode;
    if (mode == PathPromptMode::SaveAs) {
        m_pathPromptBuffer = m_filePath.empty() ? "/home/monolith/documents/note.txt" : m_filePath;
    } else if (mode == PathPromptMode::Open) {
        if (!m_filePath.empty()) {
            const size_t slash = m_filePath.find_last_of('/');
            m_pathPromptBuffer = (slash != std::string::npos) ? m_filePath.substr(0, slash + 1) : "/home/monolith/";
        } else {
            m_pathPromptBuffer = "/home/monolith/";
        }
    }
}

void TextEditorApp::finishPathPrompt(bool commit) {
    const PathPromptMode mode = m_pathPromptMode;
    const std::string buffer = m_pathPromptBuffer;
    m_pathPromptMode = PathPromptMode::None;
    m_pathPromptBuffer.clear();

    if (!commit) return;
    if (!m_fs || buffer.empty()) return;

    const std::string path = m_fs->normalize(buffer);

    if (mode == PathPromptMode::Open) {
        if (!m_fs->isFile(path)) return;

        if (auto* ctrl = getController()) {
            if (ctrl->focusEditorForFile(path)) return;
        }

        loadInitialFile(path);
        m_undoStack.clear();
        m_redoStack.clear();
        if (auto* ctrl = getController()) {
            ctrl->bindEditorFile(path);
        }
        updateTitleForPath();
    } else if (mode == PathPromptMode::SaveAs) {
        const size_t slash = path.find_last_of('/');
        if (slash != std::string::npos && slash > 0) {
            m_fs->createDirectory(path.substr(0, slash));
        }

        m_filePath = path;
        refreshSyntaxMode();
        saveCurrentFile();
        if (auto* ctrl = getController()) {
            ctrl->bindEditorFile(path);
        }
        updateTitleForPath();
    }
}

void TextEditorApp::completePathPrompt() {
    if (!m_fs || m_pathPromptBuffer.empty()) return;

    const size_t slash = m_pathPromptBuffer.find_last_of('/');
    const std::string dirPart = (slash == std::string::npos) ? "" : m_pathPromptBuffer.substr(0, slash);
    const std::string namePrefix = (slash == std::string::npos) ? m_pathPromptBuffer : m_pathPromptBuffer.substr(slash + 1);
    const std::string searchDir = m_fs->normalize(dirPart.empty() ? "/" : dirPart);
    const std::string completionBase = (slash == std::string::npos)
        ? ""
        : ((slash == 0) ? "/" : dirPart + "/");

    std::vector<std::string> matches;
    for (const auto& entry : m_fs->listEntries(searchDir)) {
        if (entry.name.size() < namePrefix.size()) continue;
        if (entry.name.compare(0, namePrefix.size(), namePrefix) != 0) continue;

        std::string completion = completionBase + entry.name;
        if (entry.isDirectory && !completion.empty() && completion.back() != '/') {
            completion += "/";
        }
        matches.push_back(std::move(completion));
    }

    if (matches.empty()) return;

    if (matches.size() == 1) {
        m_pathPromptBuffer = matches.front();
        return;
    }

    const std::string common = commonPrefix(matches);
    if (common.size() > m_pathPromptBuffer.size()) {
        m_pathPromptBuffer = common;
    }
}

void TextEditorApp::handlePathPromptKey(const SDL_Keysym& keysym) {
    switch (keysym.sym) {
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
            finishPathPrompt(true);
            break;
        case SDLK_ESCAPE:
            finishPathPrompt(false);
            break;
        case SDLK_BACKSPACE:
            if (!m_pathPromptBuffer.empty()) {
                m_pathPromptBuffer.pop_back();
            }
            break;
        case SDLK_TAB:
            completePathPrompt();
            break;
        default:
            break;
    }
}

void TextEditorApp::handlePathPromptText(const char* text) {
    if (!text) return;
    for (const char* p = text; *p; ++p) {
        const unsigned char c = static_cast<unsigned char>(*p);
        if (c >= 32 && c < 127) {
            m_pathPromptBuffer.push_back(static_cast<char>(c));
        }
    }
}

void TextEditorApp::insertChar(char c) {
    pushUndoState();

    if (m_cursorRow < 0 || m_cursorRow >= static_cast<int>(m_lines.size())) return;

    std::string& line = m_lines[m_cursorRow];
    if (m_cursorCol < 0) m_cursorCol = 0;
    if (m_cursorCol > static_cast<int>(line.size())) m_cursorCol = static_cast<int>(line.size());

    line.insert(line.begin() + m_cursorCol, c);
    m_cursorCol++;
    m_dirty = true;
    ensureCursorVisible();
}

void TextEditorApp::insertNewline() {
    pushUndoState();

    if (m_cursorRow < 0 || m_cursorRow >= static_cast<int>(m_lines.size())) return;

    std::string& line = m_lines[m_cursorRow];
    std::string remainder = line.substr(m_cursorCol);
    line.erase(m_cursorCol);

    m_lines.insert(m_lines.begin() + m_cursorRow + 1, remainder);

    m_cursorRow++;
    m_cursorCol = 0;
    m_dirty = true;
    ensureCursorVisible();
}

void TextEditorApp::deleteChar() {
    // Backspace
    if (m_cursorRow == 0 && m_cursorCol == 0) return;

    pushUndoState();

    if (m_cursorCol > 0) {
        std::string& line = m_lines[m_cursorRow];
        line.erase(line.begin() + m_cursorCol - 1);
        m_cursorCol--;
    } else {
        // Join with previous line
        std::string current = m_lines[m_cursorRow];
        std::string& prev = m_lines[m_cursorRow - 1];
        int newCol = static_cast<int>(prev.size());
        prev += current;
        m_lines.erase(m_lines.begin() + m_cursorRow);
        m_cursorRow--;
        m_cursorCol = newCol;
    }
    m_dirty = true;
    ensureCursorVisible();
}

void TextEditorApp::deleteForward() {
    if (m_cursorRow >= static_cast<int>(m_lines.size())) return;

    std::string& line = m_lines[m_cursorRow];
    if (m_cursorCol >= static_cast<int>(line.size()) &&
        m_cursorRow + 1 >= static_cast<int>(m_lines.size())) {
        return;
    }

    pushUndoState();

    if (m_cursorCol < static_cast<int>(line.size())) {
        line.erase(line.begin() + m_cursorCol);
    } else if (m_cursorRow + 1 < static_cast<int>(m_lines.size())) {
        // Join next line into this one
        line += m_lines[m_cursorRow + 1];
        m_lines.erase(m_lines.begin() + m_cursorRow + 1);
    }
    m_dirty = true;
    ensureCursorVisible();
}

void TextEditorApp::moveLeft() {
    if (m_cursorCol > 0) {
        m_cursorCol--;
    } else if (m_cursorRow > 0) {
        m_cursorRow--;
        m_cursorCol = static_cast<int>(m_lines[m_cursorRow].size());
    }
    ensureCursorVisible();
}

void TextEditorApp::moveRight() {
    if (m_cursorRow < static_cast<int>(m_lines.size()) &&
        m_cursorCol < static_cast<int>(m_lines[m_cursorRow].size())) {
        m_cursorCol++;
    } else if (m_cursorRow + 1 < static_cast<int>(m_lines.size())) {
        m_cursorRow++;
        m_cursorCol = 0;
    }
    ensureCursorVisible();
}

void TextEditorApp::moveUp() {
    if (m_cursorRow > 0) {
        m_cursorRow--;
        int lineLen = static_cast<int>(m_lines[m_cursorRow].size());
        if (m_cursorCol > lineLen) m_cursorCol = lineLen;
    }
    ensureCursorVisible();
}

void TextEditorApp::moveDown() {
    if (m_cursorRow + 1 < static_cast<int>(m_lines.size())) {
        m_cursorRow++;
        int lineLen = static_cast<int>(m_lines[m_cursorRow].size());
        if (m_cursorCol > lineLen) m_cursorCol = lineLen;
    }
    ensureCursorVisible();
}

void TextEditorApp::moveHome() {
    m_cursorCol = 0;
    ensureCursorVisible();
}

void TextEditorApp::moveEnd() {
    if (m_cursorRow < static_cast<int>(m_lines.size())) {
        m_cursorCol = static_cast<int>(m_lines[m_cursorRow].size());
    }
    ensureCursorVisible();
}

void TextEditorApp::clampCursor() {
    if (m_lines.empty()) m_lines = {""};

    if (m_cursorRow < 0) m_cursorRow = 0;
    if (m_cursorRow >= static_cast<int>(m_lines.size())) m_cursorRow = static_cast<int>(m_lines.size()) - 1;

    int len = static_cast<int>(m_lines[m_cursorRow].size());
    if (m_cursorCol < 0) m_cursorCol = 0;
    if (m_cursorCol > len) m_cursorCol = len;
}

void TextEditorApp::ensureCursorVisible() {
    clampCursor();

    int visible = getVisibleLineCount({0, 0, m_clientWidth, m_clientHeight});
    if (visible <= 0) visible = 10;

    if (m_cursorRow < m_scrollOffset) {
        m_scrollOffset = m_cursorRow;
    } else if (m_cursorRow >= m_scrollOffset + visible) {
        m_scrollOffset = m_cursorRow - visible + 1;
    }
    if (m_scrollOffset < 0) m_scrollOffset = 0;
}

void TextEditorApp::enterFindMode() {
    m_findMode = true;
    m_findQuery.clear();
    m_findMatches.clear();
    m_currentFindMatch = -1;
}

void TextEditorApp::exitFindMode() {
    m_findMode = false;
    m_findQuery.clear();
    m_findMatches.clear();
    m_currentFindMatch = -1;
}

void TextEditorApp::updateFindMatches() {
    m_findMatches.clear();
    m_currentFindMatch = -1;

    if (m_findQuery.empty()) {
        return;
    }

    for (int row = 0; row < static_cast<int>(m_lines.size()); ++row) {
        const std::string& line = m_lines[row];
        size_t pos = line.find(m_findQuery);
        while (pos != std::string::npos) {
            m_findMatches.push_back({row, static_cast<int>(pos)});
            pos = line.find(m_findQuery, pos + 1);
        }
    }

    if (m_findMatches.empty()) {
        return;
    }

    m_currentFindMatch = 0;
    for (int i = 0; i < static_cast<int>(m_findMatches.size()); ++i) {
        const auto& match = m_findMatches[i];
        if (match.first > m_cursorRow ||
            (match.first == m_cursorRow && match.second >= m_cursorCol)) {
            m_currentFindMatch = i;
            break;
        }
    }

    applyCurrentFindMatch();
}

void TextEditorApp::moveFindMatch(int direction) {
    if (m_findMatches.empty()) {
        return;
    }

    if (m_currentFindMatch < 0 || m_currentFindMatch >= static_cast<int>(m_findMatches.size())) {
        m_currentFindMatch = 0;
    } else {
        int count = static_cast<int>(m_findMatches.size());
        m_currentFindMatch = (m_currentFindMatch + direction + count) % count;
    }

    applyCurrentFindMatch();
}

void TextEditorApp::applyCurrentFindMatch() {
    if (m_currentFindMatch < 0 || m_currentFindMatch >= static_cast<int>(m_findMatches.size())) {
        return;
    }

    const auto& match = m_findMatches[m_currentFindMatch];
    m_cursorRow = match.first;
    m_cursorCol = match.second;
    ensureCursorVisible();
}

int TextEditorApp::findMatchIndexAt(int row, int col) const {
    for (int i = 0; i < static_cast<int>(m_findMatches.size()); ++i) {
        if (m_findMatches[i].first == row && m_findMatches[i].second == col) {
            return i;
        }
    }
    return -1;
}

int TextEditorApp::getLineHeight() const {
    if (!m_font) return 18;
    return TTF_FontHeight(m_font);
}

int TextEditorApp::getVisibleLineCount(const SDL_Rect& contentRect) const {
    int lh = getLineHeight();
    if (lh <= 0) return 8;
    const int reserved = 8 + kStatusBarHeight + 4;
    return std::max(3, (contentRect.h - reserved) / lh);
}

void TextEditorApp::render(SDL_Renderer* renderer, const SDL_Rect& contentRect) {
    m_clientWidth = contentRect.w;
    m_clientHeight = contentRect.h;

    if (!m_font) {
        SDL_SetRenderDrawColor(renderer, 20, 20, 25, 255);
        SDL_RenderFillRect(renderer, &contentRect);
        return;
    }

    // Background
    SDL_SetRenderDrawColor(renderer, 18, 18, 22, 255);
    SDL_RenderFillRect(renderer, &contentRect);

    const int lineHeight = getLineHeight();
    const int padding = 8;
    const int textStartY = contentRect.y + padding;

    SDL_Color cursorColor = {180, 200, 180, 230};
    SDL_Color lineNumColor = {100, 105, 115, 255};

    int visibleLines = getVisibleLineCount(contentRect);
    int y = textStartY;
    const int lineNumWidth = 40;  // space for line numbers

    for (int i = 0; i < visibleLines; ++i) {
        int lineIdx = m_scrollOffset + i;
        if (lineIdx >= static_cast<int>(m_lines.size())) break;

        const std::string& line = m_lines[lineIdx];

        // Draw line number
        std::string lineNumStr = std::to_string(lineIdx + 1);
        SDL_Surface* numSurf = TTF_RenderText_Blended(m_font, lineNumStr.c_str(), lineNumColor);
        if (numSurf) {
            SDL_Texture* numTex = SDL_CreateTextureFromSurface(renderer, numSurf);
            if (numTex) {
                SDL_Rect numDst = {
                    contentRect.x + padding,
                    y,
                    numSurf->w,
                    numSurf->h
                };
                SDL_RenderCopy(renderer, numTex, nullptr, &numDst);
                SDL_DestroyTexture(numTex);
            }
            SDL_FreeSurface(numSurf);
        }

        if (!m_findQuery.empty()) {
            for (const auto& match : m_findMatches) {
                if (match.first != lineIdx) {
                    continue;
                }

                std::string before = line.substr(0, match.second);
                std::string found = line.substr(match.second, m_findQuery.size());

                int beforeW = 0, beforeH = 0;
                int matchW = 0, matchH = 0;
                if (!before.empty()) {
                    TTF_SizeText(m_font, before.c_str(), &beforeW, &beforeH);
                }
                if (!found.empty()) {
                    TTF_SizeText(m_font, found.c_str(), &matchW, &matchH);
                }

                int matchIndex = findMatchIndexAt(match.first, match.second);
                if (matchIndex == m_currentFindMatch) {
                    SDL_SetRenderDrawColor(renderer, 54, 92, 116, 230);
                } else {
                    SDL_SetRenderDrawColor(renderer, 36, 48, 58, 190);
                }

                SDL_Rect highlightRect = {
                    contentRect.x + padding + lineNumWidth + beforeW,
                    y + 1,
                    std::max(2, matchW),
                    lineHeight - 2
                };
                SDL_RenderFillRect(renderer, &highlightRect);
            }
        }

        // Draw the line with syntax highlighting
        drawColoredLine(
            renderer,
            line,
            contentRect.x + padding + lineNumWidth,
            y,
            contentRect.w - padding * 2 - lineNumWidth
        );

        // Draw cursor if on this line
        if (lineIdx == m_cursorRow) {
            // Measure text width up to cursor column
            std::string before = (m_cursorCol <= static_cast<int>(line.size()))
                ? line.substr(0, m_cursorCol)
                : line;

            int textW = 0, textH = 0;
            if (!before.empty()) {
                TTF_SizeText(m_font, before.c_str(), &textW, &textH);
            }

            int cursorX = contentRect.x + padding + lineNumWidth + textW;
            int cursorY = y;

            // Simple vertical bar cursor
            SDL_SetRenderDrawColor(renderer, cursorColor.r, cursorColor.g, cursorColor.b, cursorColor.a);
            SDL_Rect cursorRect = {cursorX, cursorY + 2, 2, lineHeight - 4};
            SDL_RenderFillRect(renderer, &cursorRect);
        }

        y += lineHeight;
    }

    // Status bar (reserved height; must not overlap editor lines)
    {
        SDL_Rect statusBar = {
            contentRect.x,
            contentRect.y + contentRect.h - kStatusBarHeight,
            contentRect.w,
            kStatusBarHeight
        };
        SDL_SetRenderDrawColor(renderer, 24, 26, 30, 255);
        SDL_RenderFillRect(renderer, &statusBar);

        std::string status = getDisplayName();
        if (m_findMode) {
            status = "Find: " + m_findQuery;
            if (m_findQuery.empty()) {
                status += "   |  type to search";
            } else if (m_findMatches.empty()) {
                status += "   |  no matches";
            } else {
                status += "   |  " + std::to_string(m_currentFindMatch + 1) +
                          "/" + std::to_string(m_findMatches.size());
            }
            status += "   |  Enter next, Shift+Enter previous, Esc close";
        } else if (m_pathPromptMode != PathPromptMode::None) {
            status = (m_pathPromptMode == PathPromptMode::Open) ? "Open: " : "Save as: ";
            status += m_pathPromptBuffer + "_";
            status += "   |  Tab complete, Enter confirm, Esc cancel";
        } else {
            if (m_dirty) status += " *";
            status += "   |  Ctrl+S save   Ctrl+O open   Ctrl+Shift+S save as";
            status += "   |  Ctrl+F find   Ctrl+Z undo   Ctrl+Y redo";
        }

        SDL_Surface* surf = TTF_RenderText_Blended(m_font, status.c_str(), {150, 155, 160, 255});
        if (surf) {
            SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
            if (tex) {
                SDL_Rect dst = {
                    contentRect.x + padding,
                    statusBar.y + (kStatusBarHeight - surf->h) / 2,
                    std::min(surf->w, contentRect.w - padding * 2),
                    surf->h
                };
                SDL_RenderCopy(renderer, tex, nullptr, &dst);
                SDL_DestroyTexture(tex);
            }
            SDL_FreeSurface(surf);
        }
    }
}

void TextEditorApp::handleEvent(const SDL_Event& event) {
    if (m_pathPromptMode != PathPromptMode::None) {
        if (event.type == SDL_KEYDOWN) {
            handlePathPromptKey(event.key.keysym);
        } else if (event.type == SDL_TEXTINPUT) {
            handlePathPromptText(event.text.text);
        }
        return;
    }

    if (event.type == SDL_TEXTINPUT) {
        if (m_findMode) {
            const char* t = event.text.text;
            if (t && *t) {
                m_findQuery += t;
                updateFindMatches();
            }
            return;
        }

        // Insert the text (usually one char)
        const char* t = event.text.text;
        if (t && *t) {
            // For simplicity, insert first byte. Real UTF8 handling can come later.
            insertChar(t[0]);
        }
        return;
    }

    if (event.type == SDL_KEYDOWN) {
        const SDL_Keysym& key = event.key.keysym;

        // Ctrl+F find
        if ((key.mod & KMOD_CTRL) && key.sym == SDLK_f) {
            enterFindMode();
            return;
        }

        if (m_findMode) {
            switch (key.sym) {
                case SDLK_RETURN:
                case SDLK_KP_ENTER:
                    moveFindMatch((key.mod & KMOD_SHIFT) ? -1 : 1);
                    break;

                case SDLK_BACKSPACE:
                    if (!m_findQuery.empty()) {
                        m_findQuery.pop_back();
                        updateFindMatches();
                    }
                    break;

                case SDLK_DELETE:
                    m_findQuery.clear();
                    updateFindMatches();
                    break;

                case SDLK_ESCAPE:
                    exitFindMode();
                    break;

                default:
                    break;
            }
            return;
        }

        // Ctrl+O open
        if ((key.mod & KMOD_CTRL) && key.sym == SDLK_o) {
            beginPathPrompt(PathPromptMode::Open);
            return;
        }

        // Ctrl+S save (prompts for path when untitled)
        if ((key.mod & KMOD_CTRL) && key.sym == SDLK_s && !(key.mod & KMOD_SHIFT)) {
            if (m_filePath.empty()) {
                beginPathPrompt(PathPromptMode::SaveAs);
            } else {
                saveCurrentFile();
            }
            return;
        }

        // Ctrl+Shift+S save as
        if ((key.mod & KMOD_CTRL) && key.sym == SDLK_s && (key.mod & KMOD_SHIFT)) {
            beginPathPrompt(PathPromptMode::SaveAs);
            return;
        }

        // Ctrl+Z undo / Ctrl+Y redo
        if ((key.mod & KMOD_CTRL) && key.sym == SDLK_z) {
            if (key.mod & KMOD_SHIFT) {
                redo();
            } else {
                undo();
            }
            return;
        }

        if ((key.mod & KMOD_CTRL) && key.sym == SDLK_y) {
            redo();
            return;
        }

        switch (key.sym) {
            case SDLK_RETURN:
            case SDLK_KP_ENTER:
                insertNewline();
                break;

            case SDLK_BACKSPACE:
                deleteChar();
                break;

            case SDLK_DELETE:
                deleteForward();
                break;

            case SDLK_LEFT:
                moveLeft();
                break;

            case SDLK_RIGHT:
                moveRight();
                break;

            case SDLK_UP:
                moveUp();
                break;

            case SDLK_DOWN:
                moveDown();
                break;

            case SDLK_HOME:
                moveHome();
                break;

            case SDLK_END:
                moveEnd();
                break;

            case SDLK_ESCAPE:
                // Could be used for something later (command mode?)
                break;

            default:
                break;
        }
    }
}

void TextEditorApp::onResize(int clientWidth, int clientHeight) {
    m_clientWidth = clientWidth;
    m_clientHeight = clientHeight;
    ensureCursorVisible();
}

void TextEditorApp::pushUndoState() {
    m_redoStack.clear();

    if (m_undoStack.size() > 50) {
        m_undoStack.erase(m_undoStack.begin());
    }

    EditorState state;
    state.lines = m_lines;
    state.cursorRow = m_cursorRow;
    state.cursorCol = m_cursorCol;
    m_undoStack.push_back(state);
}

void TextEditorApp::applyEditorState(const EditorState& state) {
    m_lines = state.lines;
    m_cursorRow = state.cursorRow;
    m_cursorCol = state.cursorCol;
    m_dirty = true;
    clampCursor();
    ensureCursorVisible();
}

void TextEditorApp::undo() {
    if (m_undoStack.empty()) return;

    EditorState current;
    current.lines = m_lines;
    current.cursorRow = m_cursorRow;
    current.cursorCol = m_cursorCol;
    m_redoStack.push_back(current);

    EditorState state = m_undoStack.back();
    m_undoStack.pop_back();
    applyEditorState(state);
}

void TextEditorApp::redo() {
    if (m_redoStack.empty()) return;

    EditorState current;
    current.lines = m_lines;
    current.cursorRow = m_cursorRow;
    current.cursorCol = m_cursorCol;
    m_undoStack.push_back(current);

    EditorState state = m_redoStack.back();
    m_redoStack.pop_back();
    applyEditorState(state);
}

} // namespace monolith::app
