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
    m_searchCursorPos = 0;
    m_searchHorizontalScrollPx = 0;
    m_searchSavedInput = m_inputBuffer;
    m_searchSavedCursorPos = std::clamp(
        m_inputCursorPos,
        0,
        static_cast<int>(m_inputBuffer.size()));
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
        m_inputCursorPos = std::clamp(
            m_searchSavedCursorPos,
            0,
            static_cast<int>(m_inputBuffer.size()));
    }

    m_searchMode = false;
    m_searchBuffer.clear();
    m_searchCursorPos = 0;
    m_searchHorizontalScrollPx = 0;
    m_searchMatchIndex = -1;
    m_searchSavedInput.clear();
    m_searchSavedCursorPos = 0;
    m_historyIndex = -1;
    m_savedInputBuffer.clear();
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

    std::string content;
    if (!m_fs->readFile(HISTORY_FILE, content) || content.empty()) return;

    std::istringstream iss(content);
    std::string line;
    while (std::getline(iss, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
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
    const CompletionContext context = completionContextAt(m_inputBuffer, m_inputCursorPos);
    if (!context.hasToken || context.prefix.empty()) return;

    std::vector<std::string> matches;

    if (context.firstWord) {
        matches = getCommandCompletions(context.prefix);
    } else {
        matches = getPathCompletions(context.prefix);
    }

    if (matches.empty()) return;

    auto replacePrefix = [&](const std::string& completion) {
        const std::string encoded = escapeCompletion(completion, context.quote);
        const std::size_t replaceLength = static_cast<std::size_t>(m_inputCursorPos)
                                         - context.replacementStart;
        m_inputBuffer.replace(context.replacementStart, replaceLength, encoded);
        m_inputCursorPos = static_cast<int>(context.replacementStart + encoded.size());
    };

    if (matches.size() == 1) {
        // Single match - complete it
        std::string completion = matches[0];
        std::string newWord = completion;
        bool directoryCompletion = false;

        // For paths, append / if it's a directory
        if (!context.firstWord) {
            std::string fullPath = resolvePath(newWord);
            if (m_fs && m_fs->isDirectory(fullPath) && !newWord.empty() && newWord.back() != '/') {
                newWord += "/";
                directoryCompletion = true;
            }
        }

        const bool cursorAtLineEnd = m_inputCursorPos == static_cast<int>(m_inputBuffer.size());
        replacePrefix(newWord);
        if (context.quote == '"' && !directoryCompletion && cursorAtLineEnd) {
            m_inputBuffer.insert(static_cast<std::size_t>(m_inputCursorPos), 1, '"');
            ++m_inputCursorPos;
        }
    } else {
        // Multiple matches - complete common prefix if possible
        std::string common = context.prefix;
        for (const auto& m : matches) {
            std::size_t len = std::min(common.size(), m.size());
            while (len > 0 && common.compare(0, len, m, 0, len) != 0) --len;
            common = common.substr(0, len);
        }

        if (common.size() > context.prefix.size()) {
            replacePrefix(common);
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
    const bool absolutePath = !partial.empty() && partial.front() == '/';
    if (lastSlash != std::string::npos) {
        dirPart = partial.substr(0, lastSlash);
        filePrefix = partial.substr(lastSlash + 1);
    }

    std::string searchDir = dirPart.empty()
        ? (absolutePath ? "/" : m_cwd)
        : resolvePath(dirPart);

    auto entries = m_fs->listEntries(searchDir);
    std::vector<std::string> matches;

    std::string base = dirPart.empty()
        ? (absolutePath ? "/" : "")
        : dirPart + "/";

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
    SDL_RenderSetClipRect(renderer, &inputBar);
    if (m_searchMode) {
        m_searchCursorPos = std::min(m_searchCursorPos, m_searchBuffer.size());
        // Reverse-i-search UI
        const std::string beforeCursor = m_searchBuffer.substr(0, m_searchCursorPos);
        const std::string afterCursor = m_searchBuffer.substr(m_searchCursorPos);
        std::string searchPrompt = "(reverse-i-search)`" + beforeCursor + afterCursor + "': ";
        std::string matchText;

        if (m_searchMatchIndex >= 0 && m_searchMatchIndex < static_cast<int>(m_commandHistory.size())) {
            matchText = m_commandHistory[m_searchMatchIndex];
        } else if (!m_searchBuffer.empty()) {
            matchText = "(no match)";
        }

        std::string full = searchPrompt + matchText;
        const std::string cursorPrefix = "(reverse-i-search)`" + beforeCursor;
        int cursorPrefixWidth = 0;
        int cursorPrefixHeight = 0;
        TTF_SizeUTF8(m_font, cursorPrefix.c_str(), &cursorPrefixWidth, &cursorPrefixHeight);
        const int inputViewportWidth = std::max(1, inputBar.w - padding * 2);
        const int cursorWidth = 8;

        SDL_Surface* surf = TTF_RenderUTF8_Blended(m_font, full.c_str(), textColor);
        if (surf) {
            SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
            if (tex) {
                if (cursorPrefixWidth + cursorWidth
                    > m_searchHorizontalScrollPx + inputViewportWidth) {
                    m_searchHorizontalScrollPx = cursorPrefixWidth + cursorWidth - inputViewportWidth;
                } else if (cursorPrefixWidth < m_searchHorizontalScrollPx) {
                    m_searchHorizontalScrollPx = cursorPrefixWidth;
                }
                const int maxScroll = std::max(0, surf->w - inputViewportWidth);
                m_searchHorizontalScrollPx = std::clamp(
                    m_searchHorizontalScrollPx, 0, maxScroll);
                SDL_Rect dst = {
                    contentRect.x + padding - m_searchHorizontalScrollPx,
                    inputLineY,
                    surf->w,
                    surf->h
                };
                SDL_RenderCopy(renderer, tex, nullptr, &dst);
                SDL_DestroyTexture(tex);
            }
            SDL_FreeSurface(surf);
        }

        // Cursor on the search buffer part.
        const int cursorX = contentRect.x + padding
            + cursorPrefixWidth - m_searchHorizontalScrollPx;

        SDL_SetRenderDrawColor(renderer, 180, 200, 180, 230);
        SDL_Rect cur = {cursorX, inputLineY + 2, 8, lineHeight - 4};
        SDL_RenderFillRect(renderer, &cur);
    } else {
        // Normal prompt + input with cursor
        std::string prompt = getInputPrompt();
        std::string beforeCursor = prompt + m_inputBuffer.substr(0, m_inputCursorPos);
        std::string afterCursor  = m_inputBuffer.substr(m_inputCursorPos);
        int prefixWidth = 0;
        int prefixHeight = 0;
        TTF_SizeUTF8(m_font, beforeCursor.c_str(), &prefixWidth, &prefixHeight);
        const int cursorWidth = 8;
        const int inputViewportWidth = std::max(1, inputBar.w - padding * 2);
        if (prefixWidth + cursorWidth > m_inputHorizontalScrollPx + inputViewportWidth) {
            m_inputHorizontalScrollPx = prefixWidth + cursorWidth - inputViewportWidth;
        } else if (prefixWidth < m_inputHorizontalScrollPx) {
            m_inputHorizontalScrollPx = prefixWidth;
        }
        if (m_inputHorizontalScrollPx < 0) m_inputHorizontalScrollPx = 0;

        const int textOriginX = contentRect.x + padding - m_inputHorizontalScrollPx;

        // Draw text before cursor
        SDL_Surface* surf = TTF_RenderUTF8_Blended(m_font, beforeCursor.c_str(), textColor);
        int cursorX = textOriginX;
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
        SDL_SetRenderDrawColor(renderer, 180, 200, 180, 230);
        SDL_Rect cursorRect = {cursorX, inputLineY + 2, cursorWidth, lineHeight - 4};
        SDL_RenderFillRect(renderer, &cursorRect);

        // Draw text after cursor
        if (!afterCursor.empty()) {
            SDL_Surface* surf2 = TTF_RenderUTF8_Blended(m_font, afterCursor.c_str(), textColor);
            if (surf2) {
                SDL_Texture* tex2 = SDL_CreateTextureFromSurface(renderer, surf2);
                if (tex2) {
                    SDL_Rect dst2 = {cursorX + cursorWidth, inputLineY, surf2->w, surf2->h};
                    SDL_RenderCopy(renderer, tex2, nullptr, &dst2);
                    SDL_DestroyTexture(tex2);
                }
                SDL_FreeSurface(surf2);
            }
        }
    }
    SDL_RenderSetClipRect(renderer, nullptr);

    // Draw history, respecting scroll offset (0 = bottom/newest)
    SDL_RenderSetClipRect(renderer, &historyRect);
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
                        surf->w,
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
    SDL_RenderSetClipRect(renderer, nullptr);

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

void TerminalApp::onUiScaleChanged() {
    const int maxScroll = std::max(
        0,
        static_cast<int>(m_history.size())
            - getMaxVisibleLines({0, 0, m_clientWidth, m_clientHeight})
            + 2);
    m_scrollOffset = std::clamp(m_scrollOffset, 0, maxScroll);

    // Font metrics changed, so pixel-based horizontal offsets must be measured
    // again by the next render pass.
    m_inputHorizontalScrollPx = 0;
    m_searchHorizontalScrollPx = 0;
}

} // namespace monolith::app
