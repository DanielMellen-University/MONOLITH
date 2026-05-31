#include "TextEditorApp.hpp"
#include <algorithm>
#include <sstream>

namespace monolith::app {

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
        m_lines.emplace_back("  - Ctrl+S to save (if a file path is set)");
        m_lines.emplace_back("");
        m_lines.emplace_back("This editor is very early. More features coming.");
        m_cursorRow = 0;
        m_cursorCol = 0;
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
    pushUndoState();

    // Backspace
    if (m_cursorRow == 0 && m_cursorCol == 0) return;

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

int TextEditorApp::getLineHeight() const {
    if (!m_font) return 18;
    return TTF_FontHeight(m_font);
}

int TextEditorApp::getVisibleLineCount(const SDL_Rect& contentRect) const {
    int lh = getLineHeight();
    if (lh <= 0) return 8;
    // Leave a little padding at top and bottom
    return std::max(3, (contentRect.h - 16) / lh);
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

    SDL_Color textColor = {200, 205, 210, 255};
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

        // Draw the actual line text, shifted right for line numbers
        SDL_Surface* surf = TTF_RenderText_Blended(m_font, line.c_str(), textColor);
        if (surf) {
            SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
            if (tex) {
                SDL_Rect dst = {
                    contentRect.x + padding + lineNumWidth,
                    y,
                    std::min(surf->w, contentRect.w - padding * 2 - lineNumWidth),
                    surf->h
                };
                SDL_RenderCopy(renderer, tex, nullptr, &dst);
                SDL_DestroyTexture(tex);
            }
            SDL_FreeSurface(surf);
        }

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

    // Very minimal status line at the very bottom (optional polish)
    {
        std::string status = getDisplayName();
        if (m_dirty) status += " *";
        if (m_fs && !m_filePath.empty()) {
            status += "   |  Ctrl+S to save";
        }

        SDL_Surface* surf = TTF_RenderText_Blended(m_font, status.c_str(), {150, 155, 160, 255});
        if (surf) {
            SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
            if (tex) {
                SDL_Rect dst = {
                    contentRect.x + padding,
                    contentRect.y + contentRect.h - surf->h - 4,
                    surf->w,
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
    if (event.type == SDL_TEXTINPUT) {
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

        // Ctrl+S save
        if ((key.mod & KMOD_CTRL) && key.sym == SDLK_s) {
            if (saveCurrentFile()) {
                // Could add a flash later; for now silent success is fine
            }
            return;
        }

        // Ctrl+Z undo
        if ((key.mod & KMOD_CTRL) && key.sym == SDLK_z) {
            undo();
            return;
        }

        // Ctrl+F find
        if ((key.mod & KMOD_CTRL) && key.sym == SDLK_f) {
            m_findMode = true;
            m_findQuery.clear();
            m_findMatches.clear();
            m_currentFindMatch = -1;
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
    // Limit undo stack size
    if (m_undoStack.size() > 50) {
        m_undoStack.erase(m_undoStack.begin());
    }

    EditorState state;
    state.lines = m_lines;
    state.cursorRow = m_cursorRow;
    state.cursorCol = m_cursorCol;
    m_undoStack.push_back(state);
}

void TextEditorApp::undo() {
    if (m_undoStack.empty()) return;

    EditorState state = m_undoStack.back();
    m_undoStack.pop_back();

    m_lines = state.lines;
    m_cursorRow = state.cursorRow;
    m_cursorCol = state.cursorCol;
    m_dirty = true;
    ensureCursorVisible();
}

} // namespace monolith::app
