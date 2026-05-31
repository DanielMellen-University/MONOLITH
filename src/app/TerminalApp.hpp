#pragma once

#include "App.hpp"
#include "../fs/Filesystem.hpp"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <string>
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

private:
    void addOutput(const std::string& line);
    void submitInput();
    void executeCommand(const std::string& commandLine);
    void processTextInput(const char* text);
    void handleKeyDown(const SDL_Keysym& keysym);
    void handleMouseWheel(const SDL_MouseWheelEvent& e);

    // Scrolling helpers
    void scrollHistory(int delta);

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

    std::string getInputPrompt() const;  // includes cwd for better UX

    // Filesystem helpers
    std::string resolvePath(const std::string& path) const;

    // Tab completion helpers
    std::vector<std::string> getCommandCompletions(const std::string& prefix) const;
    std::vector<std::string> getPathCompletions(const std::string& partial) const;

    // Recursive remove helper for rm -r
    bool removeRecursive(const std::string& virtualPath);

    TTF_Font* m_font = nullptr;

    monolith::fs::Filesystem* m_fs = nullptr;
    std::string m_cwd = "/home/monolith";

    std::vector<std::string> m_history;          // Output history (what is displayed)
    std::vector<std::string> m_commandHistory;   // Commands the user has entered (for 'history' cmd)
    std::string m_inputBuffer;
    std::string m_prompt = "> ";

    // Command history navigation
    int m_historyIndex = -1;           // -1 means not navigating history
    std::string m_savedInputBuffer;    // original input when starting history nav

    // Input line cursor
    int m_inputCursorPos = 0;

    // Scrollback support
    int m_scrollOffset = 0;   // 0 = showing newest (bottom). Higher values = scrolled upward.

    // Cached size for scroll calculations
    int m_clientWidth = 0;
    int m_clientHeight = 0;

    // Ctrl+R reverse history search
    bool m_searchMode = false;
    std::string m_searchBuffer;
    int m_searchMatchIndex = -1;          // index in m_commandHistory, or -1
    std::string m_searchSavedInput;       // input buffer saved when entering search

    // Persistent history
    static constexpr const char* HISTORY_FILE = "/home/monolith/.terminal_history";
    void loadCommandHistory();
    void saveCommandHistory();

    // Simple auto-scroll: we always try to show the newest content
    // For v1 we keep all history and draw the bottom portion + input line
};

} // namespace monolith::app
