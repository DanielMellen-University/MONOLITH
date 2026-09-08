#pragma once

namespace monolith::pong {

enum class State { Playing, GameOver };
enum class Input { None, Up, Down, Restart };

/**
 * Headless Pong rules. PongApp drives this from input/update;
 * tests call the same methods with no SDL.
 */
struct Game {
    static constexpr int kFieldW = 320;
    static constexpr int kFieldH = 200;
    static constexpr int kPaddleW = 8;
    static constexpr int kPaddleH = 40;
    static constexpr int kBallSize = 6;
    static constexpr int kWinScore = 5;
    static constexpr float kPaddleSpeed = 220.f;
    static constexpr float kBallSpeed = 160.f;

    State state = State::Playing;
    float playerY = 80.f;
    float aiY = 80.f;
    float ballX = 157.f;
    float ballY = 97.f;
    float ballVX = kBallSpeed;
    float ballVY = 70.f;
    int playerScore = 0;
    int aiScore = 0;

    void resetMatch();
    void serve(bool towardAi);
    void applyInput(Input in, float dtSeconds);
    void tick(float dtSeconds);

    float paddleMaxY() const { return static_cast<float>(kFieldH - kPaddleH); }
};

} // namespace monolith::pong
