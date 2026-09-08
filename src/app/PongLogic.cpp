#include "PongLogic.hpp"

#include <algorithm>
#include <cmath>

namespace monolith::pong {

void Game::resetMatch() {
    state = State::Playing;
    playerScore = 0;
    aiScore = 0;
    playerY = paddleMaxY() * 0.5f;
    aiY = paddleMaxY() * 0.5f;
    serve(true);
}

void Game::serve(bool towardAi) {
    ballX = static_cast<float>(kFieldW) * 0.5f - static_cast<float>(kBallSize) * 0.5f;
    ballY = static_cast<float>(kFieldH) * 0.5f - static_cast<float>(kBallSize) * 0.5f;
    ballVX = towardAi ? kBallSpeed : -kBallSpeed;
    ballVY = 70.f;
}

void Game::applyInput(Input in, float dtSeconds) {
    if (in == Input::Restart) {
        resetMatch();
        return;
    }
    if (state != State::Playing) return;
    if (dtSeconds <= 0.f) return;
    if (in == Input::Up) {
        playerY = std::max(0.f, playerY - kPaddleSpeed * dtSeconds);
    } else if (in == Input::Down) {
        playerY = std::min(paddleMaxY(), playerY + kPaddleSpeed * dtSeconds);
    }
}

void Game::tick(float dtSeconds) {
    if (state != State::Playing) return;
    if (dtSeconds <= 0.f) return;
    if (dtSeconds > 0.05f) dtSeconds = 0.05f;

    // Simple AI: chase the ball, capped to paddle speed.
    const float aiCenter = aiY + static_cast<float>(kPaddleH) * 0.5f;
    const float ballCenter = ballY + static_cast<float>(kBallSize) * 0.5f;
    if (ballCenter < aiCenter - 2.f) {
        aiY = std::max(0.f, aiY - kPaddleSpeed * 0.85f * dtSeconds);
    } else if (ballCenter > aiCenter + 2.f) {
        aiY = std::min(paddleMaxY(), aiY + kPaddleSpeed * 0.85f * dtSeconds);
    }

    ballX += ballVX * dtSeconds;
    ballY += ballVY * dtSeconds;

    if (ballY < 0.f) {
        ballY = 0.f;
        ballVY = std::abs(ballVY);
    } else if (ballY + static_cast<float>(kBallSize) > static_cast<float>(kFieldH)) {
        ballY = static_cast<float>(kFieldH - kBallSize);
        ballVY = -std::abs(ballVY);
    }

    auto overlapsPaddle = [&](float paddleX, float paddleY) {
        const float bx0 = ballX;
        const float by0 = ballY;
        const float bx1 = ballX + static_cast<float>(kBallSize);
        const float by1 = ballY + static_cast<float>(kBallSize);
        const float px0 = paddleX;
        const float py0 = paddleY;
        const float px1 = paddleX + static_cast<float>(kPaddleW);
        const float py1 = paddleY + static_cast<float>(kPaddleH);
        return bx0 < px1 && bx1 > px0 && by0 < py1 && by1 > py0;
    };

    const float playerX = 12.f;
    const float aiX = static_cast<float>(kFieldW - 12 - kPaddleW);

    if (ballVX < 0.f && overlapsPaddle(playerX, playerY)) {
        ballX = playerX + static_cast<float>(kPaddleW);
        ballVX = std::abs(ballVX);
        const float hit = (ballY + static_cast<float>(kBallSize) * 0.5f)
            - (playerY + static_cast<float>(kPaddleH) * 0.5f);
        ballVY += hit * 4.f;
    } else if (ballVX > 0.f && overlapsPaddle(aiX, aiY)) {
        ballX = aiX - static_cast<float>(kBallSize);
        ballVX = -std::abs(ballVX);
        const float hit = (ballY + static_cast<float>(kBallSize) * 0.5f)
            - (aiY + static_cast<float>(kPaddleH) * 0.5f);
        ballVY += hit * 4.f;
    }

    if (ballX + static_cast<float>(kBallSize) < 0.f) {
        ++aiScore;
        if (aiScore >= kWinScore) {
            state = State::GameOver;
            return;
        }
        serve(true);
    } else if (ballX > static_cast<float>(kFieldW)) {
        ++playerScore;
        if (playerScore >= kWinScore) {
            state = State::GameOver;
            return;
        }
        serve(false);
    }
}

} // namespace monolith::pong
