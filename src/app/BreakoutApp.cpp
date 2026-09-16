#include "BreakoutApp.hpp"
#include "../detail/RendererClip.hpp"
#include "../detail/TickMath.hpp"

#include <algorithm>
#include <cstdio>
#include <string>

namespace monolith::app {

namespace {
constexpr SDL_Color kHud{210, 214, 220, 255};
constexpr SDL_Color kDim{150, 154, 160, 255};
constexpr SDL_Color kOverlay{245, 245, 250, 255};

SDL_Color brickColor(int row) {
    switch (row) {
        case 0: return SDL_Color{230, 90, 90, 255};
        case 1: return SDL_Color{230, 150, 70, 255};
        case 2: return SDL_Color{230, 210, 80, 255};
        case 3: return SDL_Color{90, 190, 120, 255};
        default: return SDL_Color{90, 160, 230, 255};
    }
}
} // namespace

using monolith::detail::RendererClipState;
using monolith::detail::captureRendererClip;
using monolith::detail::restoreRendererClip;
using monolith::detail::intersectRendererClip;

BreakoutApp::BreakoutApp(TTF_Font* font) : m_font(font) {
    m_game.resetMatch();
    m_lastTickMs = SDL_GetTicks();
}

void BreakoutApp::onResize(int clientWidth, int clientHeight) {
    m_clientWidth = clientWidth;
    m_clientHeight = clientHeight;
}

void BreakoutApp::onUiScaleChanged() {
    m_textSurfaceCache.clear();
}

void BreakoutApp::onFocusLost() {
    if (m_game.state == monolith::breakout::State::Playing) {
        m_paused = true;
    }
    m_holdLeft = false;
    m_holdRight = false;
}

void BreakoutApp::onFocusGained() {}

int BreakoutApp::hudHeight() const {
    const int fontHeight = m_font ? TTF_FontHeight(m_font) : 16;
    return std::max(kHudHeight, fontHeight + 12);
}

void BreakoutApp::update() {
    const Uint32 now = SDL_GetTicks();
    const float dt = monolith::detail::tickDeltaSeconds(now, m_lastTickMs);
    m_lastTickMs = now;

    if (m_game.state != monolith::breakout::State::Playing || m_paused) return;

    if (m_holdLeft) m_game.applyInput(monolith::breakout::Input::Left, dt);
    if (m_holdRight) m_game.applyInput(monolith::breakout::Input::Right, dt);
    m_game.tick(dt);
}

void BreakoutApp::handleEvent(const SDL_Event& event) {
    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        if (m_game.state == monolith::breakout::State::GameOver
            || m_game.state == monolith::breakout::State::Won) {
            m_game.resetMatch();
            m_paused = false;
            m_lastTickMs = SDL_GetTicks();
            return;
        }
        m_paused = !m_paused;
        if (!m_paused) m_lastTickMs = SDL_GetTicks();
        return;
    }

    if (event.type == SDL_KEYUP) {
        const SDL_Keycode key = event.key.keysym.sym;
        if (key == SDLK_LEFT || key == SDLK_a) m_holdLeft = false;
        if (key == SDLK_RIGHT || key == SDLK_d) m_holdRight = false;
        return;
    }

    if (event.type != SDL_KEYDOWN) return;
    const SDL_Keycode key = event.key.keysym.sym;
    switch (key) {
        case SDLK_LEFT:
        case SDLK_a:
            m_holdLeft = true;
            break;
        case SDLK_RIGHT:
        case SDLK_d:
            m_holdRight = true;
            break;
        case SDLK_SPACE:
        case SDLK_p:
            if (m_game.state == monolith::breakout::State::Playing) {
                m_paused = !m_paused;
                if (!m_paused) m_lastTickMs = SDL_GetTicks();
            }
            break;
        case SDLK_r:
            m_game.applyInput(monolith::breakout::Input::Restart, 0.f);
            m_paused = false;
            m_lastTickMs = SDL_GetTicks();
            break;
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
            if (m_game.state == monolith::breakout::State::GameOver
                || m_game.state == monolith::breakout::State::Won) {
                m_game.applyInput(monolith::breakout::Input::Restart, 0.f);
                m_paused = false;
                m_lastTickMs = SDL_GetTicks();
            } else if (m_paused) {
                m_paused = false;
                m_lastTickMs = SDL_GetTicks();
            }
            break;
        default:
            break;
    }
}

#include "BreakoutApp_body.inc"

} // namespace monolith::app
