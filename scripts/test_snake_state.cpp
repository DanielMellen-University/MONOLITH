// Headless test of shipped Snake movement rules.
// Compiles against src/app/SnakeApp.cpp and does not render a window.

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include <cstdint>
#include <deque>
#include <iostream>
#include <string>
#include <utility>

#include "../src/app/App.hpp"

#define private public
#include "../src/app/SnakeApp.hpp"
#undef private

using monolith::app::SnakeApp;

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

    SnakeApp game(nullptr);
    const auto tailTurnBody = [] {
        return std::deque<std::pair<int, int>>{
            {2, 1}, // head
            {2, 2},
            {1, 2},
            {1, 1}, // tail and next destination
        };
    };

    game.m_body = tailTurnBody();
    game.m_dir = SnakeApp::Dir::Left;
    game.m_dirQueue.clear();
    game.m_foodX = 10;
    game.m_foodY = 10;
    game.m_state = SnakeApp::State::Playing;
    game.m_paused = false;
    game.step();

    check(game.m_state == SnakeApp::State::Playing,
          "moving into a vacating tail square stays alive");
    check(game.m_body.size() == 4 && game.m_body.front() == std::make_pair(1, 1),
          "tail entry advances the body without changing length");

    game.m_body = tailTurnBody();
    game.m_dir = SnakeApp::Dir::Left;
    game.m_dirQueue.clear();
    game.m_foodX = 1;
    game.m_foodY = 1;
    game.m_state = SnakeApp::State::Playing;
    game.m_paused = false;
    game.step();

    check(game.m_state == SnakeApp::State::GameOver,
          "moving into the tail while eating is a collision");
    check(game.m_body.size() == 4 && game.m_body.front() == std::make_pair(2, 1),
          "food collision leaves the body unchanged");

    if (failures == 0) {
        std::cout << "ALL SNAKE STATE TESTS PASSED\n";
        return 0;
    }
    std::cerr << failures << " test(s) failed\n";
    return 1;
}
