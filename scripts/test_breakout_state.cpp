// Headless test of shipped Breakout rules (input -> state; running vs ended).
// Compiles against src/app/BreakoutLogic.cpp (no SDL).

#include "../src/app/BreakoutLogic.hpp"

#include <iostream>

using monolith::breakout::Game;
using monolith::breakout::Input;
using monolith::breakout::State;

int main() {
    int failures = 0;
    auto check = [&](bool ok, const char* msg) {
        if (!ok) {
            std::cerr << "FAIL: " << msg << "\n";
            ++failures;
        } else {
            std::cout << "ok: " << msg << "\n";
        }
    };

    Game g;
    g.resetMatch();
    check(g.state == State::Playing, "reset starts in Playing");
    check(g.lives == Game::kStartLives, "reset lives are full");
    check(g.bricksLeft == Game::kBrickCols * Game::kBrickRows, "reset fills all bricks");
    check(g.score == 0, "reset score is zero");

    const float x0 = g.paddleX;
    g.applyInput(Input::Left, 0.2f);
    check(g.paddleX < x0, "Left input moves paddle");
    const float x1 = g.paddleX;
    g.applyInput(Input::Right, 0.2f);
    check(g.paddleX > x1, "Right input moves paddle");

    g.ballY = static_cast<float>(Game::kFieldH + 20);
    g.ballVY = 200.f;
    const int livesBefore = g.lives;
    g.tick(0.05f);
    check(g.lives == livesBefore - 1, "ball past bottom costs a life");
    check(g.state == State::Playing, "match still running after one life lost");

    g.lives = 1;
    g.ballY = static_cast<float>(Game::kFieldH + 20);
    g.ballVY = 200.f;
    g.tick(0.05f);
    check(g.lives == 0, "final life reaches zero");
    check(g.state == State::GameOver, "no lives left ends the match");

    const float xBefore = g.paddleX;
    g.applyInput(Input::Left, 0.2f);
    check(g.paddleX == xBefore, "input ignored after GameOver");

    g.applyInput(Input::Restart, 0.f);
    check(g.state == State::Playing, "Restart returns to Playing");
    check(g.lives == Game::kStartLives, "Restart restores lives");

    g.bricks.fill(false);
    g.bricksLeft = 1;
    g.setBrickAlive(0, 0, true);
    float bx, by, bw, bh;
    g.brickRect(0, 0, bx, by, bw, bh);
    g.ballX = bx + 1.f;
    g.ballY = by + 1.f;
    g.ballVX = 10.f;
    g.ballVY = -10.f;
    g.tick(0.01f);
    check(g.bricksLeft == 0, "last brick increments clear");
    check(g.state == State::Won, "clearing all bricks wins");

    if (failures == 0) {
        std::cout << "ALL BREAKOUT STATE TESTS PASSED\n";
        return 0;
    }
    std::cerr << failures << " test(s) failed\n";
    return 1;
}
