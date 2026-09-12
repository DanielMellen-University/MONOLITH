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
    const int baseHudHeight = game.hudHeight();
    const int baseFooterHeight = game.footerHeight();
    check(TTF_SetFontSize(font, 16) == 0, "Minesweeper state scales test font");
    game.onUiScaleChanged();
    check(game.m_difficultyButtonHeight > baseButtonHeight,
          "difficulty buttons grow with the shared interface font");
    check(game.hudHeight() >= game.m_difficultyButtonHeight + MinesweeperApp::kDifficultyButtonY + 4,
          "Minesweeper HUD grows with scaled difficulty controls");
    check(game.footerHeight() >= baseFooterHeight,
          "Minesweeper footer keeps a stable readable height");
    game.onResize(260, 220);
    const SDL_Rect windowContent{40, 30, 260, 220};
    game.layoutBoard(windowContent);
    const SDL_Rect faceRect = game.clientFaceButtonRect();
    check(game.m_faceBtnRect.x == windowContent.x + faceRect.x
              && game.m_faceBtnRect.y == windowContent.y + faceRect.y
              && game.m_faceBtnRect.w == faceRect.w
              && game.m_faceBtnRect.h == faceRect.h,
          "Minesweeper draws the face button from its client hitbox");
    game.newGame(MinesweeperApp::Difficulty::Expert);
    SDL_Event difficultyClick{};
    difficultyClick.type = SDL_MOUSEBUTTONDOWN;
    difficultyClick.button.button = SDL_BUTTON_LEFT;
    const SDL_Rect beginner = game.clientDifficultyButtonRect(0);
    difficultyClick.button.x = beginner.x + beginner.w / 2;
    difficultyClick.button.y = beginner.y + beginner.h / 2;
    game.handleEvent(difficultyClick);
    check(game.m_difficulty == MinesweeperApp::Difficulty::Beginner,
          "Minesweeper input uses the same difficulty button geometry as rendering");
    game.newGame(MinesweeperApp::Difficulty::Expert);
    game.onResize(120, 120);
    int boardX = 0;
    int boardY = 0;
    int cellPx = 0;
    int boardW = 0;
    int boardH = 0;
    game.clientBoardMetrics(boardX, boardY, cellPx, boardW, boardH);
    check(cellPx >= 1 && boardX >= 0 && boardY >= game.hudHeight()
              && boardX + boardW <= 120
              && boardY + boardH <= 120 - game.footerHeight(),
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
