#include "BreakoutLogic.hpp"

#include <algorithm>
#include <cmath>

namespace monolith::breakout {

void Game::resetMatch() {
    state = State::Playing;
    lives = kStartLives;
    score = 0;
    bricks.fill(true);
    bricksLeft = kBrickCols * kBrickRows;
    paddleX = paddleMaxX() * 0.5f;
    serve();
}

void Game::serve() {
    ballX = paddleX + static_cast<float>(kPaddleW) * 0.5f
        - static_cast<float>(kBallSize) * 0.5f;
    ballY = static_cast<float>(kFieldH - 28 - kBallSize);
    ballVX = kBallSpeed * 0.55f;
    ballVY = -kBallSpeed;
}

void Game::applyInput(Input in, float dtSeconds) {
    if (in == Input::Restart) {
        resetMatch();
        return;
    }
    if (state != State::Playing) return;
    if (dtSeconds <= 0.f) return;
    if (in == Input::Left) {
        paddleX = std::max(0.f, paddleX - kPaddleSpeed * dtSeconds);
    } else if (in == Input::Right) {
        paddleX = std::min(paddleMaxX(), paddleX + kPaddleSpeed * dtSeconds);
    }
}

void Game::tick(float dtSeconds) {
    if (state != State::Playing) return;
    if (dtSeconds <= 0.f) return;
    if (dtSeconds > 0.05f) dtSeconds = 0.05f;

    ballX += ballVX * dtSeconds;
    ballY += ballVY * dtSeconds;

    if (ballX < 0.f) {
        ballX = 0.f;
        ballVX = std::abs(ballVX);
    } else if (ballX + static_cast<float>(kBallSize) > static_cast<float>(kFieldW)) {
        ballX = static_cast<float>(kFieldW - kBallSize);
        ballVX = -std::abs(ballVX);
    }

    if (ballY < 0.f) {
        ballY = 0.f;
        ballVY = std::abs(ballVY);
    }

    const float paddleY = static_cast<float>(kFieldH - 20 - kPaddleH);
    auto overlaps = [](float ax0, float ay0, float ax1, float ay1,
                       float bx0, float by0, float bx1, float by1) {
        return ax0 < bx1 && ax1 > bx0 && ay0 < by1 && ay1 > by0;
    };

    const float bx0 = ballX;
    const float by0 = ballY;
    const float bx1 = ballX + static_cast<float>(kBallSize);
    const float by1 = ballY + static_cast<float>(kBallSize);

    if (ballVY > 0.f
        && overlaps(bx0, by0, bx1, by1,
                    paddleX, paddleY,
                    paddleX + static_cast<float>(kPaddleW),
                    paddleY + static_cast<float>(kPaddleH))) {
        ballY = paddleY - static_cast<float>(kBallSize);
        ballVY = -std::abs(ballVY);
        const float hit = (ballX + static_cast<float>(kBallSize) * 0.5f)
            - (paddleX + static_cast<float>(kPaddleW) * 0.5f);
        ballVX += hit * 3.5f;
        const float speed = std::sqrt(ballVX * ballVX + ballVY * ballVY);
        if (speed > 1.f) {
            const float target = kBallSpeed * 1.05f;
            ballVX = ballVX / speed * target;
            ballVY = ballVY / speed * target;
            if (ballVY > -40.f) ballVY = -40.f;
        }
    }

    for (int row = 0; row < kBrickRows; ++row) {
        for (int col = 0; col < kBrickCols; ++col) {
            if (!brickAlive(col, row)) continue;
            float rx, ry, rw, rh;
            brickRect(col, row, rx, ry, rw, rh);
            if (!overlaps(bx0, by0, bx1, by1, rx, ry, rx + rw, ry + rh)) continue;

            setBrickAlive(col, row, false);
            --bricksLeft;
            score += (kBrickRows - row) * 10;

            const float overlapL = bx1 - rx;
            const float overlapR = (rx + rw) - bx0;
            const float overlapT = by1 - ry;
            const float overlapB = (ry + rh) - by0;
            const float minX = std::min(overlapL, overlapR);
            const float minY = std::min(overlapT, overlapB);
            if (minX < minY) {
                ballVX = -ballVX;
                if (overlapL < overlapR) ballX = rx - static_cast<float>(kBallSize);
                else ballX = rx + rw;
            } else {
                ballVY = -ballVY;
                if (overlapT < overlapB) ballY = ry - static_cast<float>(kBallSize);
                else ballY = ry + rh;
            }

            if (bricksLeft <= 0) {
                state = State::Won;
                return;
            }
            goto bricks_done;
        }
    }
bricks_done:

    if (ballY > static_cast<float>(kFieldH)) {
        --lives;
        if (lives <= 0) {
            state = State::GameOver;
            return;
        }
        serve();
    }
}

} // namespace monolith::breakout
