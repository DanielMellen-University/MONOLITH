#include "SnakeApp.hpp"

#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <string>

namespace monolith::app {

namespace {
constexpr SDL_Color kHudText{200, 200, 210, 255};
constexpr SDL_Color kDimText{140, 140, 150, 255};
constexpr SDL_Color kOverlayText{245, 245, 250, 255};
constexpr SDL_Color kGold{230, 190, 70, 255};
constexpr SDL_Color kBestText{180, 200, 140, 255};
} // namespace

std::string SnakeApp::highScoreHostPath() {
    const char* home = std::getenv("HOME");
    if (home && *home) {
        return std::string(home) + "/.monolith/snake_highscore.txt";
    }
    return "./snake_highscore.txt";
}

void SnakeApp::loadHighScore() {
    m_highScore = 0;
    std::ifstream in(highScoreHostPath());
    if (!in) return;
    int value = 0;
    if (in >> value && value >= 0) {
        m_highScore = value;
    }
}

void SnakeApp::saveHighScore() const {
    const std::string path = highScoreHostPath();
    std::error_code ec;
    std::filesystem::path p(path);
    if (p.has_parent_path()) {
        std::filesystem::create_directories(p.parent_path(), ec);
    }
    std::ofstream out(path, std::ios::trunc);
    if (!out) return;
    out << m_highScore << '\n';
}

void SnakeApp::maybeUpdateHighScore() {
    if (m_score > m_highScore) {
        m_highScore = m_score;
        m_newHighScore = true;
        saveHighScore();
    }
}

SnakeApp::SnakeApp(TTF_Font* font) : m_font(font) {
    static bool seeded = false;
    if (!seeded) {
        std::srand(static_cast<unsigned>(std::time(nullptr)));
        seeded = true;
    }
    loadHighScore();
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
    m_dirQueue.clear();
    m_score = 0;
    m_newHighScore = false;
    m_state = State::Playing;
    m_paused = false;
    m_stepIntervalMs = kInitialStepMs;
    m_lastStepMs = SDL_GetTicks();
    m_eatFlashUntilMs = 0;
    spawnFood();
}

void SnakeApp::spawnFood() {
    if (static_cast<int>(m_body.size()) >= kGridW * kGridH) {
        m_state = State::Won;
        maybeUpdateHighScore();
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
    maybeUpdateHighScore();
}

bool SnakeApp::isOpposite(Dir a, Dir b) const {
    return (a == Dir::Up && b == Dir::Down) || (a == Dir::Down && b == Dir::Up) ||
           (a == Dir::Left && b == Dir::Right) || (a == Dir::Right && b == Dir::Left);
}

void SnakeApp::setDirection(Dir dir) {
    if (m_state != State::Playing || m_paused) return;

    // Effective facing: last queued turn, else current movement direction.
    const Dir facing = m_dirQueue.empty() ? m_dir : m_dirQueue.back();
    if (isOpposite(dir, facing) || dir == facing) return;

    // Queue up to a few rapid turns so input is not lost between steps.
    if (static_cast<int>(m_dirQueue.size()) >= kMaxQueuedDirs) {
        m_dirQueue.back() = dir;
    } else {
        m_dirQueue.push_back(dir);
    }
}

void SnakeApp::applySpeedForScore() {
    // Mild speed-up every 4 points; floor stays playable.
    const int steps = m_score / 4;
    m_stepIntervalMs = std::max(kMinStepMs, kInitialStepMs - steps * 6);
}

void SnakeApp::step() {
    if (!m_dirQueue.empty()) {
        m_dir = m_dirQueue.front();
        m_dirQueue.pop_front();
    }

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
        maybeUpdateHighScore();
        return;
    }
    for (const auto& seg : m_body) {
        if (seg.first == nx && seg.second == ny) {
            m_state = State::GameOver;
            maybeUpdateHighScore();
            return;
        }
    }

    m_body.push_front({nx, ny});
    if (nx == m_foodX && ny == m_foodY) {
        ++m_score;
        applySpeedForScore();
        m_eatFlashUntilMs = SDL_GetTicks() + kEatFlashMs;
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

void SnakeApp::drawCenteredLine(SDL_Renderer* renderer, const char* text,
                                const SDL_Rect& area, int topY, SDL_Color color) const {
    if (!m_font || !text || !*text) return;
    SDL_Surface* surf = TTF_RenderUTF8_Blended(m_font, text, color);
    if (!surf) return;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
    if (tex) {
        SDL_Rect dst{
            area.x + (area.w - surf->w) / 2,
            topY,
            surf->w,
            surf->h
        };
        SDL_RenderCopy(renderer, tex, nullptr, &dst);
        SDL_DestroyTexture(tex);
    }
    SDL_FreeSurface(surf);
}

void SnakeApp::handleEvent(const SDL_Event& event) {
    if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT) {
        if (m_state == State::GameOver || m_state == State::Won) {
            resetGame();
            return;
        }
        if (m_state == State::Playing) {
            // Click anywhere in the client to toggle pause.
            m_paused = !m_paused;
            if (!m_paused) {
                m_lastStepMs = SDL_GetTicks();
            }
        }
        return;
    }

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
            resetGame();
            break;
        case SDLK_RETURN:
            if (m_state == State::GameOver || m_state == State::Won || m_paused) {
                if (m_paused && m_state == State::Playing) {
                    m_paused = false;
                    m_lastStepMs = SDL_GetTicks();
                } else {
                    resetGame();
                }
            }
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
    const std::string bestText = "Best: " + std::to_string(m_highScore);
    const std::string lenText = "Len: " + std::to_string(m_body.size());
    drawText(renderer, scoreText.c_str(), contentRect.x + 10, contentRect.y + 8, kHudText);
    drawText(renderer, bestText.c_str(), contentRect.x + 100, contentRect.y + 8, kBestText);
    drawText(renderer, lenText.c_str(), contentRect.x + 190, contentRect.y + 8, kDimText);
    drawText(renderer, "WASD  P pause  R restart", contentRect.x + 260,
             contentRect.y + 8, kDimText);

    // Board background
    SDL_SetRenderDrawColor(renderer, 28, 30, 36, 255);
    SDL_Rect boardRect{m_boardX, m_boardY, m_boardPxW, m_boardPxH};
    SDL_RenderFillRect(renderer, &boardRect);

    // Checkerboard cells
    for (int y = 0; y < kGridH; ++y) {
        for (int x = 0; x < kGridW; ++x) {
            if (((x + y) & 1) == 0) continue;
            SDL_Rect cell{
                m_boardX + x * m_cellPx,
                m_boardY + y * m_cellPx,
                m_cellPx,
                m_cellPx
            };
            SDL_SetRenderDrawColor(renderer, 34, 36, 44, 255);
            SDL_RenderFillRect(renderer, &cell);
        }
    }

    // Board border
    SDL_SetRenderDrawColor(renderer, 55, 58, 70, 255);
    SDL_RenderDrawRect(renderer, &boardRect);

    // Food (with brief flash after eating via larger highlight)
    {
        const bool flash = SDL_GetTicks() < m_eatFlashUntilMs;
        const int inset = std::max(1, m_cellPx / (flash ? 5 : 6));
        SDL_Rect fr{
            m_boardX + m_foodX * m_cellPx + inset,
            m_boardY + m_foodY * m_cellPx + inset,
            m_cellPx - 2 * inset,
            m_cellPx - 2 * inset
        };
        SDL_SetRenderDrawColor(renderer, flash ? 255 : 220, flash ? 120 : 90, flash ? 90 : 70, 255);
        SDL_RenderFillRect(renderer, &fr);
        // Small highlight
        if (m_cellPx >= 10) {
            const int hx = fr.x + std::max(1, fr.w / 5);
            const int hy = fr.y + std::max(1, fr.h / 5);
            SDL_Rect hi{hx, hy, std::max(1, fr.w / 4), std::max(1, fr.h / 4)};
            SDL_SetRenderDrawColor(renderer, 255, 200, 180, 255);
            SDL_RenderFillRect(renderer, &hi);
        }
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
            SDL_SetRenderDrawColor(renderer, 110, 230, 130, 255);
        } else {
            // Slight gradient from head color toward darker tail
            const float t = static_cast<float>(i) / static_cast<float>(std::max<size_t>(1, m_body.size() - 1));
            const int g = static_cast<int>(170 - t * 40);
            const int r = static_cast<int>(55 + t * 10);
            const int b = static_cast<int>(85 + t * 10);
            SDL_SetRenderDrawColor(renderer, r, g, b, 255);
        }
        SDL_RenderFillRect(renderer, &cr);

        // Head eyes facing movement direction
        if (isHead && m_cellPx >= 10) {
            const int eye = std::max(1, m_cellPx / 7);
            int e1x = cr.x + cr.w / 3 - eye / 2;
            int e1y = cr.y + cr.h / 3 - eye / 2;
            int e2x = cr.x + (2 * cr.w) / 3 - eye / 2;
            int e2y = cr.y + cr.h / 3 - eye / 2;
            switch (m_dir) {
                case Dir::Up:
                    e1x = cr.x + cr.w / 4 - eye / 2;
                    e2x = cr.x + (3 * cr.w) / 4 - eye / 2;
                    e1y = e2y = cr.y + cr.h / 4 - eye / 2;
                    break;
                case Dir::Down:
                    e1x = cr.x + cr.w / 4 - eye / 2;
                    e2x = cr.x + (3 * cr.w) / 4 - eye / 2;
                    e1y = e2y = cr.y + (3 * cr.h) / 4 - eye / 2;
                    break;
                case Dir::Left:
                    e1x = e2x = cr.x + cr.w / 4 - eye / 2;
                    e1y = cr.y + cr.h / 4 - eye / 2;
                    e2y = cr.y + (3 * cr.h) / 4 - eye / 2;
                    break;
                case Dir::Right:
                    e1x = e2x = cr.x + (3 * cr.w) / 4 - eye / 2;
                    e1y = cr.y + cr.h / 4 - eye / 2;
                    e2y = cr.y + (3 * cr.h) / 4 - eye / 2;
                    break;
            }
            SDL_SetRenderDrawColor(renderer, 20, 30, 25, 255);
            SDL_Rect eye1{e1x, e1y, eye, eye};
            SDL_Rect eye2{e2x, e2y, eye, eye};
            SDL_RenderFillRect(renderer, &eye1);
            SDL_RenderFillRect(renderer, &eye2);
        }
    }

    // Overlays — all lines horizontally centered as a vertical stack
    auto drawOverlayStack = [&](const char* title, const std::string& line2,
                                const char* line3, SDL_Color line3Color) {
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 150);
        SDL_RenderFillRect(renderer, &boardRect);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

        const int lineH = 18;
        const int gap = 4;
        int lines = 1 + (!line2.empty() ? 1 : 0) + (line3 && *line3 ? 1 : 0);
        const int blockH = lines * lineH + (lines - 1) * gap;
        int y = boardRect.y + (boardRect.h - blockH) / 2;

        drawCenteredLine(renderer, title, boardRect, y, kOverlayText);
        y += lineH + gap;
        if (!line2.empty()) {
            drawCenteredLine(renderer, line2.c_str(), boardRect, y, kDimText);
            y += lineH + gap;
        }
        if (line3 && *line3) {
            drawCenteredLine(renderer, line3, boardRect, y, line3Color);
        }
    };

    if (m_paused && m_state == State::Playing) {
        drawOverlayStack("PAUSED", "Space / P / click to resume", nullptr, kDimText);
    } else if (m_state == State::GameOver) {
        const std::string finalScore =
            "Score " + std::to_string(m_score) + "  ·  Best " + std::to_string(m_highScore);
        if (m_newHighScore) {
            drawOverlayStack("GAME OVER", finalScore, "NEW BEST!", kGold);
        } else {
            drawOverlayStack("GAME OVER", finalScore, "R or click to restart", kDimText);
        }
    } else if (m_state == State::Won) {
        if (m_newHighScore) {
            drawOverlayStack("YOU WIN!", "Board filled  ·  R or click to restart",
                             "NEW BEST!", kGold);
        } else {
            drawOverlayStack("YOU WIN!", "Board filled  ·  R or click to restart",
                             nullptr, kDimText);
        }
    }
}

} // namespace monolith::app
