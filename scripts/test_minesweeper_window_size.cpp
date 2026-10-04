// Headless integration test for difficulty-driven Minesweeper window sizing.

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>

#define private public
#include "../src/app/MinesweeperApp.hpp"
#include "../src/window/WindowManager.hpp"
#undef private

#include "TestTempDir.hpp"

namespace {

void keyDown(monolith::window::WindowManager& wm, SDL_Keycode key) {
    SDL_Event event{};
    event.type = SDL_KEYDOWN;
    event.key.keysym.sym = key;
    wm.handleEvent(event);
}

} // namespace

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

    check(TTF_Init() == 0, "Minesweeper window-size SDL_ttf initialize");
    TTF_Font* font = TTF_OpenFont("assets/fonts/DejaVuSans.ttf", 14);
    check(font != nullptr, "Minesweeper window-size loads test font");
    if (!font) {
        TTF_Quit();
        return 1;
    }

    const char* originalHomeValue = std::getenv("HOME");
    const bool hadOriginalHome = originalHomeValue != nullptr;
    const std::string originalHome = originalHomeValue ? originalHomeValue : "";
    monolith::test::ScopedTempDirectory tempHome("monolith-minesweeper-window");
    check(tempHome && setenv("HOME", tempHome.path().c_str(), 1) == 0,
          "Minesweeper window-size test isolates best-time persistence");
    if (!tempHome || !std::getenv("HOME")
        || std::filesystem::path(std::getenv("HOME")) != tempHome.path()) {
        TTF_CloseFont(font);
        TTF_Quit();
        return 1;
    }

    {
        monolith::window::WindowManager wm;
        wm.setLogicalDesktopSize(1000, 700);
        wm.setAppResources(font, nullptr);
        wm.launchMinesweeper();
        check(wm.m_windows.size() == 1, "WindowManager launches Minesweeper");
        if (wm.m_windows.empty()) {
            TTF_CloseFont(font);
            TTF_Quit();
            return 1;
        }

        auto* window = wm.m_windows.back().get();
        auto* game = dynamic_cast<monolith::app::MinesweeperApp*>(window->app.get());
        check(game != nullptr, "launched window owns Minesweeper");
        if (!game) return 1;
        const int expertOuterHeight = std::max(
            game->kPreferredClientHeight,
            game->hudHeight() + 16 * game->kPreferredCellPx + game->footerHeight())
            + monolith::window::Window::TITLE_BAR_HEIGHT;
        check(window->rect.w == 360 && window->rect.h == 420,
              "Beginner keeps the established launch size");

        keyDown(wm, SDLK_2);
        check(window->rect.w == 408 && window->rect.h == expertOuterHeight
                  && game->m_clientWidth == 408
                  && game->m_clientHeight == expertOuterHeight
                      - monolith::window::Window::TITLE_BAR_HEIGHT,
              "Intermediate grows to fit readable cells and reports the new client size");

        keyDown(wm, SDLK_3);
        check(window->rect.w == 744 && window->rect.h == expertOuterHeight,
              "Expert grows to the preferred board size");

        keyDown(wm, SDLK_1);
        check(window->rect.w == 360 && window->rect.h == 420,
              "returning to Beginner restores its compact preferred size");

        wm.requestClientSize(window, 300, 200);
        const SDL_Rect userSized = window->rect;
        keyDown(wm, SDLK_r);
        check(window->rect.w == userSized.w && window->rect.h == userSized.h,
              "restarting the same difficulty preserves a manual window size");

        window->previousRect = window->rect;
        window->maximized = true;
        window->rect = wm.getUsableDesktopRect();
        const SDL_Rect maximizedRect = window->rect;
        keyDown(wm, SDLK_3);
        check(window->maximized
                  && window->rect.x == maximizedRect.x
                  && window->rect.y == maximizedRect.y
                  && window->rect.w == maximizedRect.w
                  && window->rect.h == maximizedRect.h
                  && window->previousRect.w == 744
                  && window->previousRect.h == expertOuterHeight,
              "difficulty changes preserve maximized geometry and update restore size");

        window->maximized = false;
        const SDL_Rect beforeRestore = window->rect;
        window->rect = window->previousRect;
        wm.clampSingleWindow(*window);
        wm.notifyAppResizeIfGeometryChanged(*window, beforeRestore);
        check(window->rect.w == 744 && window->rect.h == expertOuterHeight,
              "restoring after a maximized difficulty change uses the new preferred size");

        keyDown(wm, SDLK_1);
        wm.setLogicalDesktopSize(420, 300);
        keyDown(wm, SDLK_3);
        int boardX = 0;
        int boardY = 0;
        int cellPx = 0;
        int boardW = 0;
        int boardH = 0;
        game->clientBoardMetrics(boardX, boardY, cellPx, boardW, boardH);
        check(window->rect.w == 420 && window->rect.h <= 272
                  && window->rect.x >= 0 && window->rect.y >= 0,
              "small desktops clamp requested game dimensions to usable bounds");
        check(cellPx >= 1 && boardX >= 0 && boardY >= game->hudHeight()
                  && boardX + boardW <= game->m_clientWidth
                  && boardY + boardH <= game->m_clientHeight - game->footerHeight(),
              "the complete Expert board remains playable after desktop clamping");
    }

    if (hadOriginalHome) setenv("HOME", originalHome.c_str(), 1);
    else unsetenv("HOME");
    TTF_CloseFont(font);
    TTF_Quit();
    return failures == 0 ? 0 : 1;
}
