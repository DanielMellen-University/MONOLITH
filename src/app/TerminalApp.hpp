#pragma once

#include "App.hpp"
#include "../detail/TextTextureCache.hpp"
#include "../fs/Filesystem.hpp"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

namespace monolith::app {

/**
 * A basic terminal emulator app.
 * Supports a scrollback history, command input, and a small set of built-in commands.
 */
class TerminalApp : public App {
public:
    TerminalApp(TTF_Font* font, monolith::fs::Filesystem* fs = nullptr);
    ~TerminalApp() override = default;

    void render(SDL_Renderer* renderer, const SDL_Rect& contentRect) override;
    void handleEvent(const SDL_Event& event) override;
    void onResize(int clientWidth, int clientHeight) override;
    void onUiScaleChanged() override;
    void onVirtualPathMoved(const std::string& oldPath,
                            const std::string& newPath) override;
    void onVirtualPathRemoved(const std::string& path) override;

private:
    void addOutput(const std::string& line, bool lineWasTruncated = false);
    void submitInput();
    void executeCommand(const std::string& commandLine);
    void processTextInput(const char* text);
    void handleKeyDown(const SDL_Keysym& keysym);
    void handleMouseWheel(const SDL_MouseWheelEvent& e);
    void leaveHistoryNavigationOnEdit();
    bool hasInputSelection() const;
    std::pair<std::size_t, std::size_t> inputSelectionRange() const;
    void clearInputSelection();
    void deleteInputSelection();
    void insertInputText(const std::string& text);
    void moveInputCursor(int position, bool extendSelection);
    bool copyInputSelection();
    void cutInputSelection();
    void pasteInputClipboard();
    int inputCursorAtX(int x, const SDL_Rect& contentRect) const;

    // Scrolling helpers
    void scrollHistory(int delta);
    void scrollHistoryHorizontally(int direction);
    int getMaxVisibleHistoryScrollPx();

    // Tab completion
    void handleTabCompletion();

    // Reverse search helpers
    void enterReverseSearch();
    void exitReverseSearch(bool accept);
    void updateReverseSearchMatch();
    void searchPreviousMatch();  // called on Ctrl+R while in search mode


    // Drawing helpers
    int getLineHeight() const;
    int getMaxVisibleLines(const SDL_Rect& contentRect) const;
    int getMaxScrollOffset() const;
    bool getVisibleHistoryRangeBytes(std::size_t rowIndex,
                                     int pixelWidth,
                                     int horizontalOffsetPx,
                                     std::size_t& startBytes,
                                     std::size_t& visibleBytes);
    SDL_Rect getInputBarRect(const SDL_Rect& contentRect) const;
    SDL_Rect getHistoryRect(const SDL_Rect& contentRect) const;
    int getInputLineY(const SDL_Rect& contentRect, const SDL_Rect& inputBar) const;

    std::string getInputPrompt() const;  // includes cwd for better UX

    // Filesystem helpers
    std::string resolvePath(const std::string& path) const;

    // Path helper (centralized join using normalize; used by commands, remove, and tab)
    std::string joinPath(const std::string& base, const std::string& name) const;

    // Tab completion helpers
    std::vector<std::string> getCommandCompletions(const std::string& prefix) const;
    std::vector<std::string> getPathCompletions(const std::string& partial) const;

    TTF_Font* m_font = nullptr;

    monolith::fs::Filesystem* m_fs = nullptr;
    mutable monolith::detail::TextTextureCache m_textTextureCache;
    std::string m_cwd = "/home/monolith";

    struct HistoryViewportMeasure {
        int pixelWidth = -1;
        int horizontalOffsetPx = -1;
        int linePixelWidth = 0;
        std::size_t startBytes = 0;
        std::size_t visibleBytes = 0;
        bool lineWidthValid = false;
        bool valid = false;
    };

    std::vector<std::string> m_history;          // Output history (what is displayed)
    std::vector<HistoryViewportMeasure> m_historyViewportMeasures;
    std::vector<std::string> m_commandHistory;   // Commands the user has entered (for 'history' cmd)

    static constexpr size_t kMaxScrollbackLines = 2000;
    static constexpr size_t kMaxScrollbackBytes = 8 * 1024 * 1024;
    static constexpr size_t kMaxScrollbackLineBytes = 64 * 1024;
    static constexpr size_t kMaxCommandHistory = 500;
    static constexpr size_t kMaxCommandHistoryBytes = 2 * 1024 * 1024;
    static constexpr size_t kMaxCommandHistoryEntryBytes = 64 * 1024;
    static constexpr size_t kMaxCommandHistoryReadBytes =
        kMaxCommandHistoryBytes + kMaxCommandHistoryEntryBytes + 1;
    static constexpr size_t kMaxCommandHistoryRecoveryReadBytes = 16 * 1024 * 1024;
    static constexpr size_t kMaxCatLines = 5000;
    size_t m_historyBytes = 0;
    std::string m_inputBuffer;
    std::string m_prompt = "> ";

    // Command history navigation
    int m_historyIndex = -1;           // -1 means not navigating history
    std::string m_savedInputBuffer;    // original input when starting history nav
    int m_savedInputCursorPos = 0;     // original caret when starting history nav

    // Input line cursor
    int m_inputCursorPos = 0;
    int m_inputHorizontalScrollPx = 0;
    int m_inputSelectionAnchor = -1;
    bool m_selectingInputWithMouse = false;

    // Scrollback support
    int m_scrollOffset = 0;   // 0 = showing newest (bottom). Higher values = scrolled upward.
    int m_historyHorizontalScrollPx = 0;

    // Cached size for scroll calculations
    int m_clientWidth = 0;
    int m_clientHeight = 0;

    // Ctrl+R reverse history search
    bool m_searchMode = false;
    std::string m_searchBuffer;
    std::size_t m_searchCursorPos = 0;
    int m_searchHorizontalScrollPx = 0;
    int m_searchMatchIndex = -1;          // index in m_commandHistory, or -1
    std::string m_searchSavedInput;       // input buffer saved when entering search
    int m_searchSavedCursorPos = 0;       // input caret saved when entering search

    // Persistent history
    static constexpr const char* HISTORY_FILE = "/home/monolith/.terminal_history";
    void loadCommandHistory();
    void saveCommandHistory();
    void trimCommandHistory();

    // Scrollback offset 0 shows the newest retained output.
};

} // namespace monolith::app
