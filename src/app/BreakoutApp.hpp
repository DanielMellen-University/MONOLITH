#pragma once

#include "App.hpp"
#include "BreakoutLogic.hpp"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

namespace monolith::app {

/**
 * Breakout for Monolith. Move the paddle, clear all bricks, three lives.
 */
class BreakoutApp : public App {
public:
    explicit BreakoutApp(TTF_Font* font);
    ~BreakoutApp() override = default;

    void render(SDL_Renderer* renderer, const SDL_Rect& contentRect) override;
    void handleEvent(const SDL_Event& event) override;
    void update() override;
    void onFocusGained() override;
    void onFocusLost() override;
    void onResize(int clientWidth, int clientHeight) override;

private:
    static constexpr int kHudHeight = 32;

    void drawText(SDL_Renderer* renderer, const char* text, int x, int y, SDL_Color color,
                  const SDL_Rect* clip = nullptr) const;
    void drawCentered(SDL_Renderer* renderer, const char* text, const SDL_Rect& area,
                      SDL_Color color) const;
    void fieldToScreen(const SDL_Rect& contentRect, float fx, float fy, int fw, int fh,
                       SDL_Rect& out) const;
    int hudHeight() const;

    TTF_Font* m_font = nullptr;
    monolith::breakout::Game m_game;
    bool m_paused = false;
    bool m_holdLeft = false;
    bool m_holdRight = false;
    Uint32 m_lastTickMs = 0;
    int m_clientWidth = 0;
    int m_clientHeight = 0;
};

} // namespace monolith::app
