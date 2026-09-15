// Headless regression test for Minesweeper timer pause/resume precision.

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <unistd.h>

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

    const char* originalHomeValue = std::getenv("HOME");
    const bool hadOriginalHome = originalHomeValue != nullptr;
    const std::string originalHome = originalHomeValue ? originalHomeValue : "";
    const std::filesystem::path testHome =
        std::filesystem::temp_directory_path()
        / ("monolith-minesweeper-state-"
           + std::to_string(static_cast<long long>(getpid())));
    std::error_code cleanupError;
    std::filesystem::remove_all(testHome, cleanupError);
    const bool homeReady = std::filesystem::create_directories(testHome);
    const bool homeConfigured = homeReady && setenv("HOME", testHome.c_str(), 1) == 0;
    check(homeConfigured,
          "Minesweeper state isolates host best-time persistence");
    if (!homeConfigured) {
        std::filesystem::remove_all(testHome, cleanupError);
        TTF_CloseFont(font);
        TTF_Quit();
        return 1;
    }

    MinesweeperApp game(font);
    const std::filesystem::path bestPath =
        testHome / ".monolith/minesweeper_best.txt";
    game.m_bestBeginner = 12;
    game.m_bestExpert = 48;
    game.saveBestTimes();
    std::ifstream bestFile(bestPath);
    const std::string bestText(std::istreambuf_iterator<char>(bestFile), {});
    check(bestText == "beginner 12\nexpert 48\n",
          "Minesweeper saves complete best-time records");
    check(!std::filesystem::exists(bestPath.string() + ".tmp"),
          "Minesweeper removes the temporary best-time record after replacement");

    MinesweeperApp reloaded(font);
    check(reloaded.m_bestBeginner == 12 && reloaded.m_bestExpert == 48,
          "Minesweeper reloads the replaced best times");

    std::filesystem::remove(bestPath, cleanupError);
    check(std::filesystem::create_directory(bestPath),
          "create blocked Minesweeper best-time target");
    game.m_bestBeginner = 21;
    game.saveBestTimes();
    check(std::filesystem::is_directory(bestPath)
              && !std::filesystem::exists(bestPath.string() + ".tmp"),
          "failed Minesweeper replacement preserves the target and cleans up");

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
    const SDL_Rect faceAtNarrowWidth = game.clientFaceButtonRect();
    for (int i = 0; i < 3; ++i) {
        const SDL_Rect button = game.clientDifficultyButtonRect(i);
        check(button.x >= 0 && button.x + button.w <= faceAtNarrowWidth.x - 6,
              "Minesweeper difficulty controls stay before the face button");
    }
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
    const SDL_Rect faceAtTinyWidth = game.clientFaceButtonRect();
    for (int i = 0; i < 3; ++i) {
        const SDL_Rect button = game.clientDifficultyButtonRect(i);
        check(button.x >= 0 && button.x + button.w <= faceAtTinyWidth.x - 6,
              "Minesweeper tiny-client difficulty controls stay contained");
    }
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

    SDL_Event keypadEnter{};
    keypadEnter.type = SDL_KEYDOWN;
    keypadEnter.key.keysym.sym = SDLK_KP_ENTER;
    game.handleEvent(keypadEnter);
    check(game.m_state == MinesweeperApp::State::Ready && !game.m_minesPlaced,
          "keypad Enter starts a new Minesweeper game");

    std::filesystem::remove_all(testHome, cleanupError);
    if (hadOriginalHome) {
        setenv("HOME", originalHome.c_str(), 1);
    } else {
        unsetenv("HOME");
    }

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
