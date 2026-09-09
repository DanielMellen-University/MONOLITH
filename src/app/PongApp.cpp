#include "PongApp.hpp"

#include <algorithm>
#include <cstdio>
#include <string>

namespace monolith::app {

namespace {
constexpr SDL_Color kHud{210, 214, 220, 255};
constexpr SDL_Color kDim{150, 154, 160, 255};
constexpr SDL_Color kOverlay{245, 245, 250, 255};
} // namespace

PongApp::PongApp(TTF_Font* font) : m_font(font) {
    m_game.resetMatch();
    m_lastTickMs = SDL_GetTicks();
}

void PongApp::onResize(int clientWidth, int clientHeight) {
    m_clientWidth = clientWidth;
    m_clientHeight = clientHeight;
}

void PongApp::onFocusLost() {
    if (m_game.state == monolith::pong::State::Playing) {
        m_paused = true;
    }
    m_holdUp = false;
    m_holdDown = false;
}

void PongApp::onFocusGained() {}

void PongApp::update() {
    const Uint32 now = SDL_GetTicks();
    float dt = static_cast<float>(now - m_lastTickMs) / 1000.f;
    m_lastTickMs = now;
    if (dt < 0.f) dt = 0.f;
    if (dt > 0.05f) dt = 0.05f;

    if (m_game.state != monolith::pong::State::Playing || m_paused) return;

    if (m_holdUp) m_game.applyInput(monolith::pong::Input::Up, dt);
    if (m_holdDown) m_game.applyInput(monolith::pong::Input::Down, dt);
    m_game.tick(dt);
}

void PongApp::handleEvent(const SDL_Event& event) {
    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        if (m_game.state == monolith::pong::State::GameOver) {
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
        if (key == SDLK_UP || key == SDLK_w) m_holdUp = false;
        if (key == SDLK_DOWN || key == SDLK_s) m_holdDown = false;
        return;
    }

    if (event.type != SDL_KEYDOWN) return;
    const SDL_Keycode key = event.key.keysym.sym;
    switch (key) {
        case SDLK_UP:
        case SDLK_w:
            m_holdUp = true;
            break;
        case SDLK_DOWN:
        case SDLK_s:
            m_holdDown = true;
            break;
        case SDLK_SPACE:
        case SDLK_p:
            if (m_game.state == monolith::pong::State::Playing) {
                m_paused = !m_paused;
                if (!m_paused) m_lastTickMs = SDL_GetTicks();
            }
            break;
        case SDLK_r:
            m_game.applyInput(monolith::pong::Input::Restart, 0.f);
            m_paused = false;
            m_lastTickMs = SDL_GetTicks();
            break;
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
            if (m_game.state == monolith::pong::State::GameOver) {
                m_game.applyInput(monolith::pong::Input::Restart, 0.f);
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

void PongApp::drawText(SDL_Renderer* renderer, const char* text, int x, int y, SDL_Color color,
                       const SDL_Rect* clip) const {
    if (!m_font || !text || !*text) return;
    SDL_Surface* surf = TTF_RenderUTF8_Blended(m_font, text, color);
    if (!surf) return;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
    if (tex) {
        SDL_Rect dst{x, y, surf->w, surf->h};
        if (clip) {
            SDL_Rect previousClip{};
            SDL_RenderGetClipRect(renderer, &previousClip);
            const bool hadPreviousClip = previousClip.w > 0 && previousClip.h > 0;
            SDL_Rect effectiveClip = *clip;
            const bool hasEffectiveClip = !hadPreviousClip
                || SDL_IntersectRect(&previousClip, clip, &effectiveClip);
            if (hasEffectiveClip && effectiveClip.w > 0 && effectiveClip.h > 0) {
                SDL_RenderSetClipRect(renderer, &effectiveClip);
                SDL_RenderCopy(renderer, tex, nullptr, &dst);
            }
            if (hadPreviousClip) {
                SDL_RenderSetClipRect(renderer, &previousClip);
            } else {
                SDL_RenderSetClipRect(renderer, nullptr);
            }
        } else {
            SDL_RenderCopy(renderer, tex, nullptr, &dst);
        }
        SDL_DestroyTexture(tex);
    }
    SDL_FreeSurface(surf);
}

void PongApp::drawCentered(SDL_Renderer* renderer, const char* text, const SDL_Rect& area, SDL_Color color) const {
    if (!m_font || !text || !*text) return;
    SDL_Surface* surf = TTF_RenderUTF8_Blended(m_font, text, color);
    if (!surf) return;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
    if (tex) {
        SDL_Rect dst{
            area.x + (area.w - surf->w) / 2,
            area.y + (area.h - surf->h) / 2,
            surf->w,
            surf->h
        };
        SDL_RenderCopy(renderer, tex, nullptr, &dst);
        SDL_DestroyTexture(tex);
    }
    SDL_FreeSurface(surf);
}

void PongApp::fieldToScreen(const SDL_Rect& contentRect, float fx, float fy, int fw, int fh,
                            SDL_Rect& out) const {
    const int availW = contentRect.w;
    const int availH = std::max(1, contentRect.h - kHudHeight);
    const float scale = std::min(
        static_cast<float>(availW) / static_cast<float>(monolith::pong::Game::kFieldW),
        static_cast<float>(availH) / static_cast<float>(monolith::pong::Game::kFieldH));
    const int boardW = static_cast<int>(monolith::pong::Game::kFieldW * scale);
    const int boardH = static_cast<int>(monolith::pong::Game::kFieldH * scale);
    const int boardX = contentRect.x + (availW - boardW) / 2;
    const int boardY = contentRect.y + kHudHeight + (availH - boardH) / 2;
    out.x = boardX + static_cast<int>(fx * scale);
    out.y = boardY + static_cast<int>(fy * scale);
    out.w = std::max(1, static_cast<int>(static_cast<float>(fw) * scale));
    out.h = std::max(1, static_cast<int>(static_cast<float>(fh) * scale));
}

void PongApp::render(SDL_Renderer* renderer, const SDL_Rect& contentRect) {
    SDL_SetRenderDrawColor(renderer, 18, 20, 28, 255);
    SDL_RenderFillRect(renderer, &contentRect);

    char hud[64];
    std::snprintf(hud, sizeof(hud), "You %d   AI %d   first to %d",
                  m_game.playerScore, m_game.aiScore, monolith::pong::Game::kWinScore);
    const SDL_Rect hudClip = {contentRect.x, contentRect.y, contentRect.w, kHudHeight};
    drawText(renderer, hud, contentRect.x + 10, contentRect.y + 8, kHud, &hudClip);

    SDL_Rect field;
    fieldToScreen(contentRect, 0, 0, monolith::pong::Game::kFieldW, monolith::pong::Game::kFieldH, field);
    SDL_SetRenderDrawColor(renderer, 28, 32, 42, 255);
    SDL_RenderFillRect(renderer, &field);
    SDL_SetRenderDrawColor(renderer, 70, 78, 92, 255);
    SDL_RenderDrawRect(renderer, &field);
    SDL_RenderDrawLine(renderer, field.x + field.w / 2, field.y, field.x + field.w / 2, field.y + field.h);

    SDL_Rect player;
    fieldToScreen(contentRect, 12.f, m_game.playerY, monolith::pong::Game::kPaddleW,
                  monolith::pong::Game::kPaddleH, player);
    SDL_SetRenderDrawColor(renderer, 90, 180, 230, 255);
    SDL_RenderFillRect(renderer, &player);

    SDL_Rect ai;
    fieldToScreen(contentRect,
                  static_cast<float>(monolith::pong::Game::kFieldW - 12 - monolith::pong::Game::kPaddleW),
                  m_game.aiY, monolith::pong::Game::kPaddleW, monolith::pong::Game::kPaddleH, ai);
    SDL_SetRenderDrawColor(renderer, 230, 120, 110, 255);
    SDL_RenderFillRect(renderer, &ai);

    SDL_Rect ball;
    fieldToScreen(contentRect, m_game.ballX, m_game.ballY,
                  monolith::pong::Game::kBallSize, monolith::pong::Game::kBallSize, ball);
    SDL_SetRenderDrawColor(renderer, 240, 240, 245, 255);
    SDL_RenderFillRect(renderer, &ball);

    if (m_paused && m_game.state == monolith::pong::State::Playing) {
        drawCentered(renderer, "Paused — Space/click to resume", field, kDim);
    } else if (m_game.state == monolith::pong::State::GameOver) {
        const char* msg = (m_game.playerScore > m_game.aiScore) ? "You win — R or Enter" : "AI wins — R or Enter";
        drawCentered(renderer, msg, field, kOverlay);
    }
}

} // namespace monolith::app
