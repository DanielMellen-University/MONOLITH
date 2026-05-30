#include "TerminalApp.hpp"
#include <algorithm>
#include <cctype>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace monolith::app {

TerminalApp::TerminalApp(TTF_Font* font)
    : m_font(font)
{
    // Welcome message
    addOutput("Monolith Terminal");
    addOutput("Type 'help' for a list of commands.");
    addOutput("");
}

void TerminalApp::addOutput(const std::string& line) {
    m_history.push_back(line);
}

void TerminalApp::submitInput() {
    std::string command = m_inputBuffer;
    m_inputBuffer.clear();

    // Echo the command as the user typed it
    addOutput(m_prompt + command);

    if (!command.empty()) {
        m_commandHistory.push_back(command);
        executeCommand(command);
    } else {
        addOutput(""); // blank line for empty input
    }
}

void TerminalApp::executeCommand(const std::string& commandLine) {
    // Very simple splitting: command + rest of line as argument
    std::istringstream iss(commandLine);
    std::string cmd;
    iss >> cmd;

    std::string rest;
    std::getline(iss, rest);
    if (!rest.empty() && rest[0] == ' ') {
        rest = rest.substr(1);
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
        addOutput("  ls              - List directory contents");
        addOutput("  pwd             - Print working directory");
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
        addOutput("Documents");
        addOutput("Desktop");
        addOutput("Downloads");
        addOutput("Pictures");
        addOutput("Music");
        addOutput("Projects");
    }
    else if (cmd == "pwd") {
        addOutput("/home/monolith");
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
    if (text) {
        m_inputBuffer += text;
    }
}

void TerminalApp::handleKeyDown(const SDL_Keysym& keysym) {
    switch (keysym.sym) {
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
            submitInput();
            break;

        case SDLK_BACKSPACE:
            if (!m_inputBuffer.empty()) {
                m_inputBuffer.pop_back();
            }
            break;

        case SDLK_ESCAPE:
            // Could clear input in future
            m_inputBuffer.clear();
            break;

        default:
            break;
    }
}

void TerminalApp::handleEvent(const SDL_Event& event) {
    if (event.type == SDL_TEXTINPUT) {
        processTextInput(event.text.text);
    }
    else if (event.type == SDL_KEYDOWN) {
        // Ignore key repeats for now or handle as needed
        handleKeyDown(event.key.keysym);
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

    // Draw the prompt + current input
    {
        std::string inputLine = m_prompt + m_inputBuffer;
        SDL_Surface* surf = TTF_RenderText_Blended(m_font, inputLine.c_str(), textColor);
        if (surf) {
            SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
            if (tex) {
                SDL_Rect dst = {
                    contentRect.x + padding,
                    inputLineY,
                    surf->w,
                    surf->h
                };
                SDL_RenderCopy(renderer, tex, nullptr, &dst);
                SDL_DestroyTexture(tex);
            }
            SDL_FreeSurface(surf);
        }
    }

    // Draw history (newest lines toward the bottom of the history area)
    int y = historyRect.y + historyRect.h - lineHeight;

    for (auto it = m_history.rbegin(); it != m_history.rend() && y >= historyRect.y; ++it) {
        const std::string& line = *it;

        SDL_Surface* surf = TTF_RenderText_Blended(m_font, line.c_str(), textColor);
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

    // Cursor
    {
        std::string beforeCursor = m_prompt + m_inputBuffer;
        int textW = 0, textH = 0;
        TTF_SizeText(m_font, beforeCursor.c_str(), &textW, &textH);

        int cursorX = contentRect.x + padding + textW;
        int cursorY = inputLineY;

        SDL_SetRenderDrawColor(renderer, 180, 200, 180, 230);
        SDL_Rect cursor = {cursorX, cursorY + 2, 7, textH - 4};
        SDL_RenderFillRect(renderer, &cursor);
    }
}

void TerminalApp::onResize(int /*clientWidth*/, int /*clientHeight*/) {
    // For v1 we don't do complex reflow. We can add smart scrolling later.
}

} // namespace monolith::app
