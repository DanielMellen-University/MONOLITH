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

    MinesweeperApp game(nullptr);
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
        return 0;
    }
    std::cerr << failures << " test(s) failed\n";
    return 1;
}
