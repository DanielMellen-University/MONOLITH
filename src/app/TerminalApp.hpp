#pragma once

#include "App.hpp"
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
    explicit TerminalApp(TTF_Font* font);
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

    // Drawing helpers
    int getLineHeight() const;
    int getMaxVisibleLines(const SDL_Rect& contentRect) const;

    TTF_Font* m_font = nullptr;

    std::vector<std::string> m_history;          // Output history (what is displayed)
    std::vector<std::string> m_commandHistory;   // Commands the user has entered (for 'history' cmd)
    std::string m_inputBuffer;
    std::string m_prompt = "> ";

    // Simple auto-scroll: we always try to show the newest content
    // For v1 we keep all history and draw the bottom portion + input line
};

} // namespace monolith::app
