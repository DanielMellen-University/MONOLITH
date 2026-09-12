// Headless regression test for Minesweeper timer pause/resume precision.

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include <cstdint>
#include <iostream>

#include "../src/app/App.hpp"

#define private public
#include "../src/app/MinesweeperApp.hpp"
#undef private

using monolith::app::MinesweeperApp;

int main() {
    int failures = 0;
    auto check = [&](bool ok, const char* message) {
        if (!ok) {
            std::cerr << "FAIL: " << message << '\n';
            ++failures;
        } else {
            std::cout << "ok: " << message << '\n';
        }
    };

    check(TTF_Init() == 0, "Minesweeper state SDL_ttf initialize");
    TTF_Font* font = TTF_OpenFont("assets/fonts/DejaVuSans.ttf", 14);
    check(font != nullptr, "Minesweeper state loads test font");
    if (!font) {
        TTF_Quit();
        return 1;
    }

    MinesweeperApp game(font);
    const int baseButtonHeight = game.m_difficultyButtonHeight;
    check(TTF_SetFontSize(font, 16) == 0, "Minesweeper state scales test font");
    game.onUiScaleChanged();
    check(game.m_difficultyButtonHeight > baseButtonHeight,
          "difficulty buttons grow with the shared interface font");
    game.newGame(MinesweeperApp::Difficulty::Expert);
    game.onResize(120, 120);
    int boardX = 0;
    int boardY = 0;
    int cellPx = 0;
    int boardW = 0;
    int boardH = 0;
    game.clientBoardMetrics(boardX, boardY, cellPx, boardW, boardH);
    check(cellPx >= 1 && boardX >= 0 && boardY >= MinesweeperApp::kHudHeight
              && boardX + boardW <= 120
              && boardY + boardH <= 120 - MinesweeperApp::kFooterHeight,
          "Minesweeper keeps the complete expert board inside a tiny client area");
    game.m_minesPlaced = true;
    game.m_state = MinesweeperApp::State::Playing;
    game.m_focusPaused = false;
    game.m_timerStartMs = SDL_GetTicks() - 1234u;
    game.onFocusLost();

    const Uint32 pausedMs = game.m_elapsedMs;
    check(pausedMs >= 1200u && pausedMs < 1400u,
          "focus loss captures elapsed milliseconds");
    check(game.m_elapsedSec == 1,
          "HUD seconds remain the whole-second display of elapsed time");
    check(game.m_focusPaused, "focus loss pauses the timer");

    game.onFocusGained();
    const Uint32 resumedElapsed = SDL_GetTicks() - game.m_timerStartMs;
    check(!game.m_focusPaused, "focus gain resumes the timer");
    check(resumedElapsed >= pausedMs && resumedElapsed < pausedMs + 100u,
          "resume preserves the sub-second timer remainder");

    if (failures == 0) {
        std::cout << "ALL MINESWEEPER STATE TESTS PASSED\n";
        TTF_CloseFont(font);
        TTF_Quit();
        return 0;
    }
    std::cerr << failures << " test(s) failed\n";
    TTF_CloseFont(font);
    TTF_Quit();
    return 1;
}
