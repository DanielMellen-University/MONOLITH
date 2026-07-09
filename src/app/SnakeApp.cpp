#include "SnakeApp.hpp"

#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <string>

namespace monolith::app {

namespace {
constexpr SDL_Color kHudText{200, 200, 210, 255};
constexpr SDL_Color kDimText{140, 140, 150, 255};
constexpr SDL_Color kOverlayText{245, 245, 250, 255};
} // namespace

SnakeApp::SnakeApp(TTF_Font* font) : m_font(font) {
    static bool seeded = false;
    if (!seeded) {
        std::srand(static_cast<unsigned>(std::time(nullptr)));
        seeded = true;
    }
    resetGame();
}

void SnakeApp::resetGame() {
    m_body.clear();
    const int cx = kGridW / 2;
    const int cy = kGridH / 2;
    m_body.push_back({cx, cy});
    m_body.push_back({cx - 1, cy});
    m_body.push_back({cx - 2, cy});
    m_dir = Dir::Right;
    m_pendingDir = Dir::Right;
    m_score = 0;
    m_state = State::Playing;
    m_paused = false;
    m_stepIntervalMs = kInitialStepMs;
    m_lastStepMs = SDL_GetTicks();
    spawnFood();
}

void SnakeApp::spawnFood() {
    if (static_cast<int>(m_body.size()) >= kGridW * kGridH) {
        m_state = State::Won;
        return;
    }
    for (int attempt = 0; attempt < 500; ++attempt) {
        const int x = std::rand() % kGridW;
        const int y = std::rand() % kGridH;
        bool occupied = false;
        for (const auto& seg : m_body) {
            if (seg.first == x && seg.second == y) {
                occupied = true;
                break;
            }
        }
        if (!occupied) {
            m_foodX = x;
            m_foodY = y;
            return;
        }
    }
    // Fallback linear scan
    for (int y = 0; y < kGridH; ++y) {
        for (int x = 0; x < kGridW; ++x) {
            bool occupied = false;
            for (const auto& seg : m_body) {
                if (seg.first == x && seg.second == y) {
                    occupied = true;
                    break;
                }
            }
            if (!occupied) {
                m_foodX = x;
                m_foodY = y;
                return;
            }
        }
    }
    m_state = State::Won;
}

bool SnakeApp::isOpposite(Dir a, Dir b) const {
    return (a == Dir::Up && b == Dir::Down) || (a == Dir::Down && b == Dir::Up) ||
           (a == Dir::Left && b == Dir::Right) || (a == Dir::Right && b == Dir::Left);
}

void SnakeApp::setDirection(Dir dir) {
    if (m_state != State::Playing || m_paused) return;
    if (isOpposite(dir, m_dir)) return;
    m_pendingDir = dir;
}

void SnakeApp::applySpeedForScore() {
    // Mild speed-up every 5 points
    const int steps = m_score / 5;
    m_stepIntervalMs = std::max(kMinStepMs, kInitialStepMs - steps * 5);
}

void SnakeApp::step() {
    m_dir = m_pendingDir;
    auto [hx, hy] = m_body.front();
    int nx = hx;
    int ny = hy;
    switch (m_dir) {
        case Dir::Up:    --ny; break;
        case Dir::Down:  ++ny; break;
        case Dir::Left:  --nx; break;
        case Dir::Right: ++nx; break;
    }

    if (nx < 0 || nx >= kGridW || ny < 0 || ny >= kGridH) {
        m_state = State::GameOver;
        return;
    }
    for (const auto& seg : m_body) {
        if (seg.first == nx && seg.second == ny) {
            m_state = State::GameOver;
            return;
        }
    }

    m_body.push_front({nx, ny});
    if (nx == m_foodX && ny == m_foodY) {
        ++m_score;
        applySpeedForScore();
        spawnFood();
    } else {
        m_body.pop_back();
    }
}

void SnakeApp::update() {
    if (m_state != State::Playing || m_paused) return;
    const Uint32 now = SDL_GetTicks();
    if (now - m_lastStepMs < static_cast<Uint32>(m_stepIntervalMs)) return;
    m_lastStepMs = now;
    step();
}

void SnakeApp::onFocusLost() {
    if (m_state == State::Playing) {
        m_paused = true;
    }
}

void SnakeApp::onFocusGained() {
    // Stay paused until the user explicitly resumes.
}

void SnakeApp::onResize(int clientWidth, int clientHeight) {
    m_clientWidth = clientWidth;
    m_clientHeight = clientHeight;
}

void SnakeApp::layoutBoard(const SDL_Rect& contentRect) {
    const int availW = contentRect.w;
    const int availH = std::max(1, contentRect.h - kHudHeight);
    m_cellPx = std::max(6, std::min(availW / kGridW, availH / kGridH));
    m_boardPxW = m_cellPx * kGridW;
    m_boardPxH = m_cellPx * kGridH;
    m_boardX = contentRect.x + (availW - m_boardPxW) / 2;
    m_boardY = contentRect.y + kHudHeight + (availH - m_boardPxH) / 2;
}

void SnakeApp::drawText(SDL_Renderer* renderer, const char* text, int x, int y,
                        SDL_Color color) const {
    if (!m_font || !text || !*text) return;
    SDL_Surface* surf = TTF_RenderUTF8_Blended(m_font, text, color);
    if (!surf) return;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
    if (tex) {
        SDL_Rect dst{x, y, surf->w, surf->h};
        SDL_RenderCopy(renderer, tex, nullptr, &dst);
        SDL_DestroyTexture(tex);
    }
    SDL_FreeSurface(surf);
}

void SnakeApp::drawCenteredText(SDL_Renderer* renderer, const char* text,
                                const SDL_Rect& area, SDL_Color color) const {
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

void SnakeApp::handleEvent(const SDL_Event& event) {
    if (event.type != SDL_KEYDOWN) return;

    const SDL_Keycode key = event.key.keysym.sym;
    switch (key) {
        case SDLK_UP:
        case SDLK_w:
            setDirection(Dir::Up);
            break;
        case SDLK_DOWN:
        case SDLK_s:
            setDirection(Dir::Down);
            break;
        case SDLK_LEFT:
        case SDLK_a:
            setDirection(Dir::Left);
            break;
        case SDLK_RIGHT:
        case SDLK_d:
            setDirection(Dir::Right);
            break;
        case SDLK_SPACE:
        case SDLK_p:
            if (m_state == State::Playing) {
                m_paused = !m_paused;
                if (!m_paused) {
                    m_lastStepMs = SDL_GetTicks();
                }
            }
            break;
        case SDLK_r:
        case SDLK_RETURN:
            resetGame();
            break;
        default:
            break;
    }
}

void SnakeApp::render(SDL_Renderer* renderer, const SDL_Rect& contentRect) {
    layoutBoard(contentRect);

    // HUD background strip
    SDL_SetRenderDrawColor(renderer, 38, 38, 44, 255);
    SDL_Rect hud{contentRect.x, contentRect.y, contentRect.w, kHudHeight};
    SDL_RenderFillRect(renderer, &hud);

    const std::string scoreText = "Score: " + std::to_string(m_score);
    drawText(renderer, scoreText.c_str(), contentRect.x + 10, contentRect.y + 6, kHudText);
    drawText(renderer, "P pause  R restart  WASD/arrows", contentRect.x + 110,
             contentRect.y + 6, kDimText);

    // Board background
    SDL_SetRenderDrawColor(renderer, 28, 30, 36, 255);
    SDL_Rect boardRect{m_boardX, m_boardY, m_boardPxW, m_boardPxH};
    SDL_RenderFillRect(renderer, &boardRect);

    // Subtle grid
    SDL_SetRenderDrawColor(renderer, 40, 42, 50, 255);
    for (int x = 0; x <= kGridW; ++x) {
        const int px = m_boardX + x * m_cellPx;
        SDL_RenderDrawLine(renderer, px, m_boardY, px, m_boardY + m_boardPxH);
    }
    for (int y = 0; y <= kGridH; ++y) {
        const int py = m_boardY + y * m_cellPx;
        SDL_RenderDrawLine(renderer, m_boardX, py, m_boardX + m_boardPxW, py);
    }

    // Food
    {
        const int inset = std::max(1, m_cellPx / 6);
        SDL_Rect fr{
            m_boardX + m_foodX * m_cellPx + inset,
            m_boardY + m_foodY * m_cellPx + inset,
            m_cellPx - 2 * inset,
            m_cellPx - 2 * inset
        };
        SDL_SetRenderDrawColor(renderer, 220, 90, 70, 255);
        SDL_RenderFillRect(renderer, &fr);
    }

    // Snake body (tail → head so head draws last)
    for (size_t i = 0; i < m_body.size(); ++i) {
        const auto& seg = m_body[m_body.size() - 1 - i];
        const bool isHead = (i == m_body.size() - 1);
        const int inset = std::max(1, m_cellPx / 8);
        SDL_Rect cr{
            m_boardX + seg.first * m_cellPx + inset,
            m_boardY + seg.second * m_cellPx + inset,
            m_cellPx - 2 * inset,
            m_cellPx - 2 * inset
        };
        if (isHead) {
            SDL_SetRenderDrawColor(renderer, 100, 220, 120, 255);
        } else {
            SDL_SetRenderDrawColor(renderer, 60, 170, 90, 255);
        }
        SDL_RenderFillRect(renderer, &cr);
    }

    // Overlays
    if (m_paused && m_state == State::Playing) {
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 140);
        SDL_RenderFillRect(renderer, &boardRect);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        drawCenteredText(renderer, "PAUSED", boardRect, kOverlayText);
        drawText(renderer, "Space / P to resume", boardRect.x + 12,
                 boardRect.y + boardRect.h / 2 + 14, kDimText);
    } else if (m_state == State::GameOver) {
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 150);
        SDL_RenderFillRect(renderer, &boardRect);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        drawCenteredText(renderer, "GAME OVER", boardRect, kOverlayText);
        const std::string finalScore = "Score " + std::to_string(m_score) + "  ·  R to restart";
        drawText(renderer, finalScore.c_str(), boardRect.x + 12,
                 boardRect.y + boardRect.h / 2 + 14, kDimText);
    } else if (m_state == State::Won) {
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 150);
        SDL_RenderFillRect(renderer, &boardRect);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        drawCenteredText(renderer, "YOU WIN!", boardRect, kOverlayText);
        drawText(renderer, "Board filled  ·  R to restart", boardRect.x + 12,
                 boardRect.y + boardRect.h / 2 + 14, kDimText);
    }
}

} // namespace monolith::app
