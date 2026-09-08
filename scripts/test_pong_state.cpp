// Headless test of shipped Pong rules (input → state; running vs ended).
// Compiles against src/app/PongLogic.cpp (no SDL).

#include "../src/app/PongLogic.hpp"

#include <iostream>

using monolith::pong::Game;
using monolith::pong::Input;
using monolith::pong::State;

int main() {
    int failures = 0;
    auto check = [&](bool ok, const char* msg) {
        if (!ok) {
            std::cerr << "FAIL: " << msg << '\n';
            ++failures;
        } else {
            std::cout << "ok: " << msg << '\n';
        }
    };

    Game g;
    g.resetMatch();
    check(g.state == State::Playing, "reset starts in Playing");
    check(g.playerScore == 0 && g.aiScore == 0, "reset scores are zero");

    const float y0 = g.playerY;
    g.applyInput(Input::Up, 0.2f);
    check(g.playerY < y0, "Up input moves player paddle");
    const float y1 = g.playerY;
    g.applyInput(Input::Down, 0.2f);
    check(g.playerY > y1, "Down input moves player paddle");

    g.ballX = -20.f;
    g.ballVX = -200.f;
    g.tick(0.05f);
    check(g.aiScore == 1, "ball past left edge scores for AI");
    check(g.state == State::Playing, "match still running after one point");

    g.playerScore = Game::kWinScore - 1;
    g.ballX = static_cast<float>(Game::kFieldW + 20);
    g.ballVX = 200.f;
    g.tick(0.05f);
    check(g.playerScore == Game::kWinScore, "winning point increments player score");
    check(g.state == State::GameOver, "reaching win score ends the match");

    const float yBefore = g.playerY;
    g.applyInput(Input::Up, 0.2f);
    check(g.playerY == yBefore, "input ignored after GameOver");

    g.applyInput(Input::Restart, 0.f);
    check(g.state == State::Playing, "Restart returns to Playing");
    check(g.playerScore == 0 && g.aiScore == 0, "Restart clears scores");

    if (failures == 0) {
        std::cout << "ALL PONG STATE TESTS PASSED\n";
        return 0;
    }
    std::cerr << failures << " test(s) failed\n";
    return 1;
}
