#pragma once

#include <array>
#include <cstddef>

namespace monolith::breakout {

enum class State { Playing, Won, GameOver };
enum class Input { None, Left, Right, Restart };

/**
 * Headless Breakout rules. BreakoutApp drives this from input/update;
 * tests call the same methods with no SDL.
 */
struct Game {
    static constexpr int kFieldW = 320;
    static constexpr int kFieldH = 220;
    static constexpr int kPaddleW = 48;
    static constexpr int kPaddleH = 8;
    static constexpr int kBallSize = 6;
    static constexpr int kBrickCols = 10;
    static constexpr int kBrickRows = 5;
    static constexpr int kBrickW = 28;
    static constexpr int kBrickH = 10;
    static constexpr int kBrickGap = 2;
    static constexpr int kBrickOriginX = 11;
    static constexpr int kBrickOriginY = 24;
    static constexpr int kStartLives = 3;
    static constexpr float kPaddleSpeed = 260.f;
    static constexpr float kBallSpeed = 150.f;

    State state = State::Playing;
    float paddleX = 0.f;
    float ballX = 0.f;
    float ballY = 0.f;
    float ballVX = 0.f;
    float ballVY = 0.f;
    int lives = kStartLives;
    int score = 0;
    int bricksLeft = 0;
    std::array<bool, kBrickCols * kBrickRows> bricks{};

    void resetMatch();
    void serve();
    void applyInput(Input in, float dtSeconds);
    void tick(float dtSeconds);

    float paddleMaxX() const {
        return static_cast<float>(kFieldW - kPaddleW);
    }

    bool brickAlive(int col, int row) const {
        if (col < 0 || col >= kBrickCols || row < 0 || row >= kBrickRows) return false;
        return bricks[static_cast<size_t>(row * kBrickCols + col)];
    }

    void setBrickAlive(int col, int row, bool alive) {
        if (col < 0 || col >= kBrickCols || row < 0 || row >= kBrickRows) return;
        bricks[static_cast<size_t>(row * kBrickCols + col)] = alive;
    }

    void brickRect(int col, int row, float& x, float& y, float& w, float& h) const {
        x = static_cast<float>(kBrickOriginX + col * (kBrickW + kBrickGap));
        y = static_cast<float>(kBrickOriginY + row * (kBrickH + kBrickGap));
        w = static_cast<float>(kBrickW);
        h = static_cast<float>(kBrickH);
    }
};

} // namespace monolith::breakout
