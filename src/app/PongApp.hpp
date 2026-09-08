#pragma once

#include "App.hpp"
#include "PongLogic.hpp"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

namespace monolith::app {

/**
 * Pong for Monolith. Left paddle is the player; right paddle is a simple AI.
 * First to 5 wins.
 */
class PongApp : public App {
public:
    explicit PongApp(TTF_Font* font);
    ~PongApp() override = default;

    void render(SDL_Renderer* renderer, const SDL_Rect& contentRect) override;
    void handleEvent(const SDL_Event& event) override;
    void update() override;
    void onFocusGained() override;
    void onFocusLost() override;
    void onResize(int clientWidth, int clientHeight) override;

private:
    static constexpr int kHudHeight = 32;

    void drawText(SDL_Renderer* renderer, const char* text, int x, int y, SDL_Color color) const;
    void drawCentered(SDL_Renderer* renderer, const char* text, const SDL_Rect& area, SDL_Color color) const;
    void fieldToScreen(const SDL_Rect& contentRect, float fx, float fy, int fw, int fh,
                       SDL_Rect& out) const;

    TTF_Font* m_font = nullptr;
    monolith::pong::Game m_game;
    bool m_paused = false;
    bool m_holdUp = false;
    bool m_holdDown = false;
    Uint32 m_lastTickMs = 0;
    int m_clientWidth = 0;
    int m_clientHeight = 0;
};

} // namespace monolith::app
