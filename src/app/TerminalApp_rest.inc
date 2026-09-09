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
    while (m_commandHistory.size() > kMaxCommandHistory) {
        m_commandHistory.erase(m_commandHistory.begin());
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
    return m_fs->join(base, name);
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
