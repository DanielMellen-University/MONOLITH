void TerminalApp::scrollHistory(int delta) {
    const int nextOffset = static_cast<int>(std::clamp(
        static_cast<long long>(m_scrollOffset) + delta,
        0LL,
        static_cast<long long>(getMaxScrollOffset())));
    if (nextOffset == m_scrollOffset) return;
    m_scrollOffset = nextOffset;
    if (m_historyHorizontalScrollPx > 0) {
        m_historyHorizontalScrollPx = std::min(
            m_historyHorizontalScrollPx, getMaxVisibleHistoryScrollPx());
    }
}

void TerminalApp::scrollHistoryHorizontally(int direction) {
    const SDL_Rect contentRect{0, 0, m_clientWidth, m_clientHeight};
    const int stepPx = std::max(32, getHistoryRect(contentRect).w * 3 / 4);
    const int maxOffset = getMaxVisibleHistoryScrollPx();
    const long long nextOffset = std::clamp(
        static_cast<long long>(m_historyHorizontalScrollPx)
            + static_cast<long long>(direction) * stepPx,
        0LL,
        static_cast<long long>(maxOffset));
    m_historyHorizontalScrollPx = static_cast<int>(nextOffset);
}

int TerminalApp::getMaxVisibleHistoryScrollPx() {
    if (!m_font || m_history.empty()) return 0;
    if (m_historyViewportMeasures.size() != m_history.size()) {
        m_historyViewportMeasures.assign(m_history.size(), HistoryViewportMeasure{});
    }

    const SDL_Rect contentRect{0, 0, m_clientWidth, m_clientHeight};
    const SDL_Rect historyRect = getHistoryRect(contentRect);
    const int visibleRows = getMaxVisibleLines(contentRect);
    if (historyRect.w <= 0 || visibleRows <= 0) return 0;

    const int startIndex = static_cast<int>(m_history.size()) - 1 - m_scrollOffset;
    int maxLineWidth = 0;
    for (int row = 0; row < visibleRows; ++row) {
        const int index = startIndex - row;
        if (index < 0) break;
        auto& measure = m_historyViewportMeasures[static_cast<std::size_t>(index)];
        if (!measure.lineWidthValid) {
            int height = 0;
            if (TTF_SizeUTF8(m_font, m_history[static_cast<std::size_t>(index)].c_str(),
                             &measure.linePixelWidth, &height) != 0) {
                continue;
            }
            measure.lineWidthValid = true;
        }
        maxLineWidth = std::max(maxLineWidth, measure.linePixelWidth);
    }
    return std::max(0, maxLineWidth - historyRect.w);
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

    clearInputSelection();
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
    m_savedInputCursorPos = 0;
    clearInputSelection();
    m_scrollOffset = 0;
}

void TerminalApp::updateReverseSearchMatch() {
    m_searchMatchIndex = -1;

    if (m_searchBuffer.empty()) {
        return;
    }

    // Query edits restart at the newest history entry. Ctrl+R is the explicit
    // gesture for walking older matches, so refining or shortening a query
    // must not skip the entry that already matched it.
    for (int i = static_cast<int>(m_commandHistory.size()) - 1; i >= 0; --i) {
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

    std::deque<std::string> newestCommands;
    size_t retainedBytes = 0;
    std::string pendingLine;
    bool skipOversizedLine = false;
    bool skipLeadingPartialLine = false;
    bool prefixWasSkipped = false;
    bool firstChunk = true;
    bool leadingFragmentHadBytes = false;
    bool sawOversizedLine = false;
    std::uint64_t historyFileBytes = 0;
    bool historyNeedsSave = m_fs->fileSize(HISTORY_FILE, historyFileBytes)
        && historyFileBytes > kMaxCommandHistoryBytes;

    auto retainLine = [&](std::string line) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) return;
        if (line.size() > kMaxCommandHistoryEntryBytes) {
            historyNeedsSave = true;
            sawOversizedLine = true;
            return;
        }

        const size_t entryBytes = line.size() + 1;
        while (!newestCommands.empty()
               && (newestCommands.size() >= kMaxCommandHistory
                   || entryBytes > kMaxCommandHistoryBytes - retainedBytes)) {
            historyNeedsSave = true;
            retainedBytes -= newestCommands.front().size() + 1;
            newestCommands.pop_front();
        }
        retainedBytes += entryBytes;
        newestCommands.push_back(std::move(line));
    };

    auto consumeChunk = [&](std::string_view chunk, bool prefixSkipped) {
        if (firstChunk) {
            skipLeadingPartialLine = prefixSkipped;
            firstChunk = false;
        }

        for (const char ch : chunk) {
            if (skipLeadingPartialLine) {
                if (ch == '\n') {
                    skipLeadingPartialLine = false;
                } else if (ch != '\r') {
                    leadingFragmentHadBytes = true;
                }
                continue;
            }

            if (ch == '\n') {
                if (!skipOversizedLine) retainLine(std::move(pendingLine));
                pendingLine.clear();
                skipOversizedLine = false;
            } else if (!skipOversizedLine) {
                if (pendingLine.size() > kMaxCommandHistoryEntryBytes) {
                    pendingLine.clear();
                    skipOversizedLine = true;
                    historyNeedsSave = true;
                    sawOversizedLine = true;
                } else {
                    pendingLine.push_back(ch);
                }
            }
        }
        return true;
    };

    const bool read = m_fs->readFileTailChunks(
        HISTORY_FILE, kMaxCommandHistoryReadBytes,
        [&](std::string_view chunk) { return consumeChunk(chunk, prefixWasSkipped); },
        prefixWasSkipped);
    if (!read) return;
    historyNeedsSave = historyNeedsSave || prefixWasSkipped;

    if (prefixWasSkipped && newestCommands.empty()
        && (leadingFragmentHadBytes || sawOversizedLine)) {
        newestCommands.clear();
        retainedBytes = 0;
        pendingLine.clear();
        skipOversizedLine = false;
        skipLeadingPartialLine = false;
        prefixWasSkipped = false;
        firstChunk = true;
        leadingFragmentHadBytes = false;
        sawOversizedLine = false;
        historyNeedsSave = true;
        bool recoveryPrefixSkipped = false;
        const bool recoveryRead = m_fs->readFileTailChunks(
            HISTORY_FILE, kMaxCommandHistoryRecoveryReadBytes,
            [&](std::string_view chunk) { return consumeChunk(chunk, recoveryPrefixSkipped); },
            recoveryPrefixSkipped);
        if (!recoveryRead) return;
        historyNeedsSave = historyNeedsSave || recoveryPrefixSkipped;
    }

    if (!skipOversizedLine && !pendingLine.empty()) retainLine(std::move(pendingLine));

    m_commandHistory.assign(newestCommands.begin(), newestCommands.end());
    if (historyNeedsSave) saveCommandHistory();
}

bool TerminalApp::saveCommandHistory() {
    if (!m_fs) return true;

    trimCommandHistory();
    const bool saved = m_fs->writeFileWithProducer(
        HISTORY_FILE, [this](std::ostream& output) {
            monolith::detail::BufferedStreamWriter writer(output);
            for (const auto& command : m_commandHistory) {
                if (!writer.append(command) || !writer.append("\n")) {
                    return false;
                }
            }
            return writer.finish();
        });
    m_commandHistorySaveFailed = !saved;
    return !m_commandHistorySaveFailed;
}

void TerminalApp::trimCommandHistory() {
    m_commandHistory.erase(
        std::remove_if(m_commandHistory.begin(), m_commandHistory.end(), [](const auto& command) {
            return command.empty() || command.size() > kMaxCommandHistoryEntryBytes;
        }),
        m_commandHistory.end());

    while (!m_commandHistory.empty()) {
        size_t bytes = 0;
        for (const auto& command : m_commandHistory) {
            const size_t entryBytes = command.size() + 1;
            if (entryBytes > kMaxCommandHistoryBytes - bytes) {
                bytes = kMaxCommandHistoryBytes + 1;
                break;
            }
            bytes += entryBytes;
        }
        if (m_commandHistory.size() <= kMaxCommandHistory
            && bytes <= kMaxCommandHistoryBytes) {
            break;
        }
        m_commandHistory.erase(m_commandHistory.begin());
    }
}

void TerminalApp::handleTabCompletion() {
    const CompletionContext context = completionContextAt(m_inputBuffer, m_inputCursorPos);
    if (!context.hasToken) return;

    std::vector<std::string> matches;

    if (context.firstWord) {
        matches = getCommandCompletions(context.prefix);
    } else {
        matches = getPathCompletions(context.prefix);
    }

    if (matches.empty()) return;

    const std::string inputBeforeCompletion = m_inputBuffer;

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
        if ((context.quote == '"' || context.quote == '\'')
            && !directoryCompletion && cursorAtLineEnd) {
            m_inputBuffer.insert(
                static_cast<std::size_t>(m_inputCursorPos), 1, context.quote);
            ++m_inputCursorPos;
        }
    } else {
        // Multiple matches - complete common prefix if possible
        const std::string common = utf8CommonPrefix(matches);

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

    if (m_inputBuffer != inputBeforeCompletion) {
        leaveHistoryNavigationOnEdit();
        clearInputSelection();
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
    else if (event.type == SDL_MOUSEBUTTONDOWN
             && event.button.button == SDL_BUTTON_LEFT && !m_searchMode) {
        const SDL_Rect contentRect{0, 0, m_clientWidth, m_clientHeight};
        const SDL_Rect inputBar = getInputBarRect(contentRect);
        if (event.button.y < inputBar.y
            || event.button.y >= inputBar.y + inputBar.h) return;
        m_inputCursorPos = inputCursorAtX(event.button.x, contentRect);
        m_inputSelectionAnchor = m_inputCursorPos;
        m_selectingInputWithMouse = true;
    }
    else if (event.type == SDL_MOUSEMOTION && m_selectingInputWithMouse) {
        const SDL_Rect contentRect{0, 0, m_clientWidth, m_clientHeight};
        m_inputCursorPos = inputCursorAtX(event.motion.x, contentRect);
    }
    else if (event.type == SDL_MOUSEBUTTONUP
             && event.button.button == SDL_BUTTON_LEFT && m_selectingInputWithMouse) {
        const SDL_Rect contentRect{0, 0, m_clientWidth, m_clientHeight};
        m_inputCursorPos = inputCursorAtX(event.button.x, contentRect);
        m_selectingInputWithMouse = false;
        if (m_inputSelectionAnchor == m_inputCursorPos) clearInputSelection();
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
    if (lh <= 0) return 0;
    return getHistoryRect(contentRect).h / lh;
}

int TerminalApp::getMaxScrollOffset() const {
    const int visible = getMaxVisibleLines({0, 0, m_clientWidth, m_clientHeight});
    return std::max(0, static_cast<int>(m_history.size()) - visible);
}

SDL_Rect TerminalApp::getInputBarRect(const SDL_Rect& contentRect) const {
    const int stripHeight = getLineHeight() + 14;
    const int clientHeight = std::max(0, contentRect.h);
    return {
        contentRect.x + 4,
        contentRect.y + std::max(0, clientHeight - stripHeight),
        std::max(0, contentRect.w - 8),
        std::min(stripHeight, clientHeight)
    };
}

SDL_Rect TerminalApp::getHistoryRect(const SDL_Rect& contentRect) const {
    constexpr int padding = 8;
    constexpr int historyBottomGap = 4;
    const int inputStripHeight = getLineHeight() + 14;
    return {
        contentRect.x + padding,
        contentRect.y + padding,
        std::max(0, contentRect.w - padding * 2),
        std::max(0, contentRect.h - padding * 2 - inputStripHeight - historyBottomGap)
    };
}

int TerminalApp::getInputLineY(const SDL_Rect& contentRect, const SDL_Rect& inputBar) const {
    return std::max(inputBar.y, contentRect.y + contentRect.h - 6 - getLineHeight());
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

const std::string& TerminalApp::getInputPrompt() const {
    if (!m_inputPromptCacheValid || m_promptCacheCwd != m_cwd) {
        m_promptCacheCwd = m_cwd;
        m_cachedInputPrompt.clear();
        if (m_cwd == "/home/monolith") {
            m_cachedInputPrompt = "~";
        } else if (m_cwd.rfind("/home/monolith/", 0) == 0) {
            m_cachedInputPrompt = "~";
            m_cachedInputPrompt.append(m_cwd, 14, std::string::npos);
        } else {
            m_cachedInputPrompt = m_cwd;
        }
        m_cachedInputPrompt += "> ";
        m_inputPromptCacheValid = true;
    }
    return m_cachedInputPrompt;
}

std::string TerminalApp::joinPath(const std::string& base, const std::string& name) const {
    if (!m_fs) return name;
    return m_fs->join(base, name);
}

bool TerminalApp::getVisibleHistoryRangeBytes(std::size_t rowIndex,
                                              int pixelWidth,
                                              int horizontalOffsetPx,
                                              std::size_t& startBytes,
                                              std::size_t& visibleBytes) {
    if (!m_font || pixelWidth <= 0 || rowIndex >= m_history.size()) return false;
    if (m_historyViewportMeasures.size() != m_history.size()) {
        m_historyViewportMeasures.assign(m_history.size(), HistoryViewportMeasure{});
    }

    auto& cached = m_historyViewportMeasures[rowIndex];
    if (!cached.valid || cached.pixelWidth != pixelWidth
        || cached.horizontalOffsetPx != horizontalOffsetPx) {
        const std::string& line = m_history[rowIndex];
        std::size_t firstByte = 0;
        if (horizontalOffsetPx > 0) {
            int prefixWidth = 0;
            int characterCount = 0;
            if (TTF_MeasureUTF8(m_font, line.c_str(), horizontalOffsetPx,
                                &prefixWidth, &characterCount) != 0) {
                cached.valid = false;
                return false;
            }
            for (int character = 0;
                 character < characterCount && firstByte < line.size();
                 ++character) {
                firstByte = utf8NextCodepointStart(line, firstByte);
            }
            if (firstByte < line.size() && prefixWidth < horizontalOffsetPx) {
                firstByte = utf8NextCodepointStart(line, firstByte);
            }
        }

        std::size_t endByte = firstByte;
        if (firstByte < line.size()) {
            int ignoredWidth = 0;
            int characterCount = 0;
            if (TTF_MeasureUTF8(m_font, line.c_str() + firstByte, pixelWidth,
                                &ignoredWidth, &characterCount) != 0) {
                cached.valid = false;
                return false;
            }
            for (int character = 0;
                 character < characterCount && endByte < line.size();
                 ++character) {
                endByte = utf8NextCodepointStart(line, endByte);
            }
        }

        cached.pixelWidth = pixelWidth;
        cached.horizontalOffsetPx = horizontalOffsetPx;
        cached.startBytes = firstByte;
        cached.visibleBytes = endByte - firstByte;
        cached.valid = true;
    }

    startBytes = cached.startBytes;
    visibleBytes = cached.visibleBytes;
    return true;
}

void TerminalApp::render(SDL_Renderer* renderer, const SDL_Rect& contentRect) {
    if (m_clientWidth != contentRect.w || m_clientHeight != contentRect.h) {
        onResize(contentRect.w, contentRect.h);
    }

    if (!m_font) {
        // Fallback: just draw dark background
        SDL_SetRenderDrawColor(renderer, 15, 15, 18, 255);
        SDL_RenderFillRect(renderer, &contentRect);
        return;
    }

    const RendererClipState previousClip = captureRendererClip(renderer);

    // Terminal background
    SDL_SetRenderDrawColor(renderer, 18, 18, 22, 255);
    SDL_RenderFillRect(renderer, &contentRect);

    const int lineHeight = getLineHeight();
    const int padding = 8;

    // === Robust input area reservation (critical for maximized windows) ===
    // We reserve a fixed strip at the very bottom of the content area for the input.
    // This must never be eaten into by history, even when the window is very tall.
    const SDL_Rect inputBar = getInputBarRect(contentRect);
    const int inputLineY = getInputLineY(contentRect, inputBar);

    // History gets everything above the input strip
    const SDL_Rect historyRect = getHistoryRect(contentRect);

    // Draw input background strip (clearly separated)
    SDL_SetRenderDrawColor(renderer, 28, 28, 34, 255);
    SDL_RenderFillRect(renderer, &inputBar);

    SDL_Color textColor = {200, 205, 210, 255};

    // Draw input line (normal or reverse-search mode)
    const SDL_Rect inputClip = intersectRendererClip(inputBar, previousClip);
    SDL_RenderSetClipRect(renderer, &inputClip);
    if (m_searchMode) {
        m_searchCursorPos = std::min(m_searchCursorPos, m_searchBuffer.size());
        const std::string* matchText = nullptr;

        if (m_searchMatchIndex >= 0 && m_searchMatchIndex < static_cast<int>(m_commandHistory.size())) {
            matchText = &m_commandHistory[m_searchMatchIndex];
        }

        constexpr char kSearchPrefix[] = "(reverse-i-search)`";
        m_renderInputText.assign(kSearchPrefix);
        m_renderInputText.append(m_searchBuffer);
        m_renderInputText.append("': ");
        if (matchText) {
            m_renderInputText.append(*matchText);
        } else if (!m_searchBuffer.empty()) {
            m_renderInputText.append("(no match)");
        }
        m_renderCursorPrefix.assign(kSearchPrefix);
        m_renderCursorPrefix.append(m_searchBuffer.data(), m_searchCursorPos);

        const auto cursorPrefixTexture = m_textTextureCache.get(
            renderer, m_font, m_renderCursorPrefix.c_str(), textColor);
        int cursorPrefixWidth = cursorPrefixTexture ? cursorPrefixTexture.width : 0;
        if (!cursorPrefixTexture) {
            int cursorPrefixHeight = 0;
            TTF_SizeUTF8(m_font, cursorPrefix.c_str(),
                         &cursorPrefixWidth, &cursorPrefixHeight);
        }
        const int inputViewportWidth = std::max(1, inputBar.w - padding * 2);
        const int cursorWidth = 8;

        const auto texture = m_textTextureCache.get(
            renderer, m_font, m_renderInputText.c_str(), textColor);
        if (texture) {
            if (cursorPrefixWidth + cursorWidth
                > m_searchHorizontalScrollPx + inputViewportWidth) {
                m_searchHorizontalScrollPx = cursorPrefixWidth + cursorWidth - inputViewportWidth;
            } else if (cursorPrefixWidth < m_searchHorizontalScrollPx) {
                m_searchHorizontalScrollPx = cursorPrefixWidth;
            }
            const int maxScroll = std::max(0, texture.width - inputViewportWidth);
            m_searchHorizontalScrollPx = std::clamp(
                m_searchHorizontalScrollPx, 0, maxScroll);
            SDL_Rect dst = {
                contentRect.x + padding - m_searchHorizontalScrollPx,
                inputLineY,
                texture.width,
                texture.height
            };
            SDL_RenderCopy(renderer, texture.handle, nullptr, &dst);
        }

        // Cursor on the search buffer part.
        const int cursorX = contentRect.x + padding
            + cursorPrefixWidth - m_searchHorizontalScrollPx;

        SDL_SetRenderDrawColor(renderer, 180, 200, 180, 230);
        SDL_Rect cur = {cursorX, inputLineY + 2, 8, lineHeight - 4};
        SDL_RenderFillRect(renderer, &cur);
    } else {
        // Normal prompt, editable text, and an optional selected range.
        const std::string& prompt = getInputPrompt();
        m_inputCursorPos = static_cast<int>(utf8ClampToCodepointBoundary(
            m_inputBuffer,
            static_cast<std::size_t>(std::clamp(
                m_inputCursorPos, 0, static_cast<int>(m_inputBuffer.size())))));
        m_renderInputText.assign(prompt);
        m_renderInputText.append(m_inputBuffer);
        m_renderCursorPrefix.assign(prompt);
        m_renderCursorPrefix.append(
            m_inputBuffer.data(), static_cast<std::size_t>(m_inputCursorPos));
        const auto fullTexture = m_textTextureCache.get(
            renderer, m_font, m_renderInputText.c_str(), textColor);
        const auto cursorPrefixTexture = m_textTextureCache.get(
            renderer, m_font, m_renderCursorPrefix.c_str(), textColor);
        int fullWidth = fullTexture ? fullTexture.width : 0;
        int prefixWidth = cursorPrefixTexture ? cursorPrefixTexture.width : 0;
        if (!fullTexture) {
            int fullHeight = 0;
            TTF_SizeUTF8(m_font, fullText.c_str(), &fullWidth, &fullHeight);
        }
        if (!cursorPrefixTexture) {
            int prefixHeight = 0;
            TTF_SizeUTF8(m_font, cursorPrefix.c_str(), &prefixWidth, &prefixHeight);
        }
        constexpr int cursorWidth = 2;
        const int inputViewportWidth = std::max(1, inputBar.w - padding * 2);
        if (prefixWidth + cursorWidth > m_inputHorizontalScrollPx + inputViewportWidth) {
            m_inputHorizontalScrollPx = prefixWidth + cursorWidth - inputViewportWidth;
        } else if (prefixWidth < m_inputHorizontalScrollPx) {
            m_inputHorizontalScrollPx = prefixWidth;
        }
        m_inputHorizontalScrollPx = std::clamp(
            m_inputHorizontalScrollPx, 0, std::max(0, fullWidth - inputViewportWidth));

        const int textOriginX = contentRect.x + padding - m_inputHorizontalScrollPx;
        const int cursorX = textOriginX + prefixWidth;
        if (fullTexture) {
            const SDL_Rect dst{
                textOriginX, inputLineY, fullTexture.width, fullTexture.height
            };
            SDL_RenderCopy(renderer, fullTexture.handle, nullptr, &dst);
        }

        if (hasInputSelection()) {
            const auto [start, end] = inputSelectionRange();
            m_renderSelectionStartText.assign(prompt);
            m_renderSelectionStartText.append(m_inputBuffer.data(), start);
            m_renderSelectionEndText.assign(prompt);
            m_renderSelectionEndText.append(m_inputBuffer.data(), end);
            const auto selectionStartTexture = m_textTextureCache.get(
                renderer, m_font, m_renderSelectionStartText.c_str(), textColor);
            const auto selectionEndTexture = m_textTextureCache.get(
                renderer, m_font, m_renderSelectionEndText.c_str(), textColor);
            int selectionStartWidth = selectionStartTexture ? selectionStartTexture.width : 0;
            int selectionEndWidth = selectionEndTexture ? selectionEndTexture.width : 0;
            if (!selectionStartTexture) {
                int ignoredHeight = 0;
                TTF_SizeUTF8(m_font, selectionStartText.c_str(),
                             &selectionStartWidth, &ignoredHeight);
            }
            if (!selectionEndTexture) {
                int ignoredHeight = 0;
                TTF_SizeUTF8(m_font, selectionEndText.c_str(),
                             &selectionEndWidth, &ignoredHeight);
            }
            if (selectionEndWidth > selectionStartWidth) {
                SDL_SetRenderDrawColor(renderer, 58, 112, 160, 255);
                const SDL_Rect highlight{
                    textOriginX + selectionStartWidth,
                    inputLineY,
                    selectionEndWidth - selectionStartWidth,
                    lineHeight
                };
                SDL_RenderFillRect(renderer, &highlight);
                SDL_Rect selectionClip{};
                if (SDL_IntersectRect(&highlight, &inputClip, &selectionClip)) {
                    SDL_RenderSetClipRect(renderer, &selectionClip);
                    if (fullTexture) {
                        const SDL_Rect dst{
                            textOriginX, inputLineY, fullTexture.width, fullTexture.height
                        };
                        SDL_RenderCopy(renderer, fullTexture.handle, nullptr, &dst);
                    }
                    SDL_RenderSetClipRect(renderer, &inputClip);
                }
            }
        }

        SDL_SetRenderDrawColor(renderer, 180, 200, 180, 255);
        const SDL_Rect cursorRect{cursorX, inputLineY + 2, cursorWidth, lineHeight - 4};
        SDL_RenderFillRect(renderer, &cursorRect);
    }
    restoreRendererClip(renderer, previousClip);

    // Draw history, respecting scroll offset (0 = bottom/newest)
    const SDL_Rect historyClip = intersectRendererClip(historyRect, previousClip);
    SDL_RenderSetClipRect(renderer, &historyClip);
    if (!m_history.empty()) {
        int total = static_cast<int>(m_history.size());
        int startIdx = total - 1 - m_scrollOffset;
        if (startIdx < 0) startIdx = 0;

        int y = historyRect.y + historyRect.h - lineHeight;

        for (int i = startIdx; i >= 0 && y >= historyRect.y; --i) {
            const std::string& line = m_history[i];
            std::string visibleLine;
            const char* displayText = line.c_str();
            std::size_t startBytes = 0;
            std::size_t visibleBytes = 0;
            if (!getVisibleHistoryRangeBytes(
                    static_cast<std::size_t>(i), historyRect.w,
                    m_historyHorizontalScrollPx, startBytes, visibleBytes)) {
                y -= lineHeight;
                continue;
            }

            if (startBytes > 0 || visibleBytes < line.size()) {
                visibleLine.assign(line.data() + startBytes, visibleBytes);
                displayText = visibleLine.c_str();
            }

            const auto texture = m_textTextureCache.get(
                renderer, m_font, displayText, textColor);
            if (texture) {
                SDL_Rect dst = {
                    historyRect.x,
                    y,
                    texture.width,
                    texture.height
                };
                SDL_RenderCopy(renderer, texture.handle, nullptr, &dst);
            }

            y -= lineHeight;
        }
    }
    restoreRendererClip(renderer, previousClip);

    // Note: Cursor for input is drawn earlier in the input strip section above.
    // The old duplicate cursor block that used full-buffer calculation was removed.
}

void TerminalApp::onResize(int clientWidth, int clientHeight) {
    m_clientWidth = clientWidth;
    m_clientHeight = clientHeight;
    // Keep scroll within bounds after resize
    m_scrollOffset = std::clamp(m_scrollOffset, 0, getMaxScrollOffset());
    if (m_historyHorizontalScrollPx > 0) {
        m_historyHorizontalScrollPx = std::min(
            m_historyHorizontalScrollPx, getMaxVisibleHistoryScrollPx());
    }
}

void TerminalApp::onUiScaleChanged() {
    m_textTextureCache.clear();
    std::fill(m_historyViewportMeasures.begin(), m_historyViewportMeasures.end(),
              HistoryViewportMeasure{});
    m_scrollOffset = std::clamp(m_scrollOffset, 0, getMaxScrollOffset());
    m_historyHorizontalScrollPx = 0;

    // Font metrics changed, so pixel-based horizontal offsets must be measured
    // again by the next render pass.
    m_inputHorizontalScrollPx = 0;
    m_searchHorizontalScrollPx = 0;
}

} // namespace monolith::app
