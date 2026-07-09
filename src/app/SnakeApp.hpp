#pragma once

#include "App.hpp"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <cstdint>
#include <deque>
#include <utility>

namespace monolith::app {

/**
 * Classic Snake game for Monolith.
 * Arrow keys / WASD to move, Space/P pause, R restart.
 */
class SnakeApp : public App {
public:
    explicit SnakeApp(TTF_Font* font);
    ~SnakeApp() override = default;

    void render(SDL_Renderer* renderer, const SDL_Rect& contentRect) override;
    void handleEvent(const SDL_Event& event) override;
    void update() override;
    void onFocusGained() override;
    void onFocusLost() override;
    void onResize(int clientWidth, int clientHeight) override;

private:
    enum class Dir { Up, Down, Left, Right };
    enum class State { Playing, Paused, GameOver, Won };

    static constexpr int kGridW = 20;
    static constexpr int kGridH = 20;
    static constexpr int kHudHeight = 28;
    static constexpr int kInitialStepMs = 120;
    static constexpr int kMinStepMs = 70;

    void resetGame();
    void step();
    void spawnFood();
    void setDirection(Dir dir);
    bool isOpposite(Dir a, Dir b) const;
    void applySpeedForScore();
    void layoutBoard(const SDL_Rect& contentRect);
    void drawText(SDL_Renderer* renderer, const char* text, int x, int y,
                  SDL_Color color) const;
    void drawCenteredText(SDL_Renderer* renderer, const char* text,
                          const SDL_Rect& area, SDL_Color color) const;

    TTF_Font* m_font = nullptr;

    std::deque<std::pair<int, int>> m_body; // front = head
    Dir m_dir = Dir::Right;
    Dir m_pendingDir = Dir::Right;
    int m_foodX = 0;
    int m_foodY = 0;
    int m_score = 0;
    State m_state = State::Playing;
    bool m_paused = false;
    Uint32 m_lastStepMs = 0;
    int m_stepIntervalMs = kInitialStepMs;

    int m_clientWidth = 0;
    int m_clientHeight = 0;
    int m_cellPx = 16;
    int m_boardX = 0;
    int m_boardY = 0;
    int m_boardPxW = 0;
    int m_boardPxH = 0;
};

} // namespace monolith::app
