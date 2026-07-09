#include "MinesweeperApp.hpp"

#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <queue>
#include <string>

namespace monolith::app {

namespace {
constexpr SDL_Color kHudText{200, 200, 210, 255};
constexpr SDL_Color kDimText{140, 140, 150, 255};
constexpr SDL_Color kOverlayText{245, 245, 250, 255};
constexpr SDL_Color kBtnBg{55, 60, 75, 255};
constexpr SDL_Color kBtnActive{70, 95, 145, 255};
} // namespace

const MinesweeperApp::DifficultySpec& MinesweeperApp::specFor(Difficulty d) {
    static const DifficultySpec beginner{"Beginner", 9, 9, 10};
    static const DifficultySpec intermediate{"Intermediate", 16, 16, 40};
    static const DifficultySpec expert{"Expert", 30, 16, 99};
    switch (d) {
        case Difficulty::Beginner: return beginner;
        case Difficulty::Intermediate: return intermediate;
        case Difficulty::Expert: return expert;
    }
    return beginner;
}

MinesweeperApp::MinesweeperApp(TTF_Font* font) : m_font(font) {
    static bool seeded = false;
    if (!seeded) {
        std::srand(static_cast<unsigned>(std::time(nullptr)));
        seeded = true;
    }
    newGame(Difficulty::Beginner);
}

void MinesweeperApp::newGame(Difficulty d) {
    m_difficulty = d;
    const auto& s = specFor(d);
    m_width = s.width;
    m_height = s.height;
    m_mineCount = s.mines;
    resetBoard();
}

void MinesweeperApp::resetBoard() {
    m_cells.assign(static_cast<size_t>(m_width * m_height), Cell{});
    m_minesPlaced = false;
    m_state = State::Ready;
    m_revealedSafe = 0;
    m_timerStartMs = 0;
    m_elapsedSec = 0;
}

bool MinesweeperApp::inBounds(int x, int y) const {
    return x >= 0 && y >= 0 && x < m_width && y < m_height;
}

int MinesweeperApp::flagCount() const {
    int n = 0;
    for (const auto& c : m_cells) {
        if (c.flagged) ++n;
    }
    return n;
}

void MinesweeperApp::placeMines(int safeX, int safeY) {
    const bool clearNeighborhood = (m_difficulty == Difficulty::Beginner);
    auto isExcluded = [&](int x, int y) {
        if (x == safeX && y == safeY) return true;
        if (clearNeighborhood && std::abs(x - safeX) <= 1 && std::abs(y - safeY) <= 1) {
            return true;
        }
        return false;
    };

    int placed = 0;
    int attempts = 0;
    const int maxAttempts = m_width * m_height * 20;
    while (placed < m_mineCount && attempts < maxAttempts) {
        ++attempts;
        const int x = std::rand() % m_width;
        const int y = std::rand() % m_height;
        if (isExcluded(x, y)) continue;
        Cell& c = m_cells[static_cast<size_t>(index(x, y))];
        if (c.mine) continue;
        c.mine = true;
        ++placed;
    }

    // If random placement fell short (tiny boards), fill remaining linearly
    if (placed < m_mineCount) {
        for (int y = 0; y < m_height && placed < m_mineCount; ++y) {
            for (int x = 0; x < m_width && placed < m_mineCount; ++x) {
                if (isExcluded(x, y)) continue;
                Cell& c = m_cells[static_cast<size_t>(index(x, y))];
                if (c.mine) continue;
                c.mine = true;
                ++placed;
            }
        }
    }

    computeAdjacents();
    m_minesPlaced = true;
    m_state = State::Playing;
    m_timerStartMs = SDL_GetTicks();
    m_elapsedSec = 0;
}

void MinesweeperApp::computeAdjacents() {
    for (int y = 0; y < m_height; ++y) {
        for (int x = 0; x < m_width; ++x) {
            Cell& c = m_cells[static_cast<size_t>(index(x, y))];
            if (c.mine) {
                c.adjacent = 0;
                continue;
            }
            int count = 0;
            for (int dy = -1; dy <= 1; ++dy) {
                for (int dx = -1; dx <= 1; ++dx) {
                    if (dx == 0 && dy == 0) continue;
                    const int nx = x + dx;
                    const int ny = y + dy;
                    if (inBounds(nx, ny) && m_cells[static_cast<size_t>(index(nx, ny))].mine) {
                        ++count;
                    }
                }
            }
            c.adjacent = static_cast<uint8_t>(count);
        }
    }
}

void MinesweeperApp::floodReveal(int startX, int startY) {
    std::queue<std::pair<int, int>> q;
    q.push({startX, startY});
    while (!q.empty()) {
        auto [x, y] = q.front();
        q.pop();
        if (!inBounds(x, y)) continue;
        Cell& c = m_cells[static_cast<size_t>(index(x, y))];
        if (c.revealed || c.flagged || c.mine) continue;
        c.revealed = true;
        ++m_revealedSafe;
        if (c.adjacent != 0) continue;
        for (int dy = -1; dy <= 1; ++dy) {
            for (int dx = -1; dx <= 1; ++dx) {
                if (dx == 0 && dy == 0) continue;
                q.push({x + dx, y + dy});
            }
        }
    }
}

void MinesweeperApp::revealCell(int x, int y) {
    if (!inBounds(x, y)) return;
    if (m_state == State::Won || m_state == State::Lost) return;

    Cell& c = m_cells[static_cast<size_t>(index(x, y))];
    if (c.revealed || c.flagged) return;

    if (!m_minesPlaced) {
        placeMines(x, y);
    }

    if (c.mine) {
        lose();
        return;
    }

    if (c.adjacent == 0) {
        floodReveal(x, y);
    } else {
        c.revealed = true;
        ++m_revealedSafe;
    }
    checkWin();
}

void MinesweeperApp::lose() {
    m_state = State::Lost;
    for (auto& c : m_cells) {
        if (c.mine) c.revealed = true;
    }
}

void MinesweeperApp::checkWin() {
    const int safeTotal = m_width * m_height - m_mineCount;
    if (m_revealedSafe >= safeTotal) {
        m_state = State::Won;
        for (auto& c : m_cells) {
            if (c.mine) c.flagged = true;
        }
    }
}

void MinesweeperApp::update() {
    if (m_state != State::Playing || !m_minesPlaced) return;
    const Uint32 now = SDL_GetTicks();
    m_elapsedSec = static_cast<int>((now - m_timerStartMs) / 1000u);
    if (m_elapsedSec > 999) m_elapsedSec = 999;
}

void MinesweeperApp::onResize(int clientWidth, int clientHeight) {
    m_clientWidth = clientWidth;
    m_clientHeight = clientHeight;
}

void MinesweeperApp::layoutBoard(const SDL_Rect& contentRect) {
    const int availW = contentRect.w;
    const int availH = std::max(1, contentRect.h - kHudHeight - kFooterHeight);
    m_cellPx = std::max(kMinCellPx, std::min(availW / m_width, availH / m_height));
    m_boardPxW = m_cellPx * m_width;
    m_boardPxH = m_cellPx * m_height;
    m_boardX = contentRect.x + (availW - m_boardPxW) / 2;
    m_boardY = contentRect.y + kHudHeight + (availH - m_boardPxH) / 2;

    // Difficulty buttons in HUD
    const int btnW = 70;
    const int btnH = 18;
    const int btnY = contentRect.y + 28;
    const int gap = 6;
    const int startX = contentRect.x + 10;
    for (int i = 0; i < 3; ++i) {
        m_diffBtnRects[i] = {startX + i * (btnW + gap), btnY, btnW, btnH};
    }
}

void MinesweeperApp::drawText(SDL_Renderer* renderer, const char* text, int x, int y,
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

void MinesweeperApp::drawCenteredText(SDL_Renderer* renderer, const char* text,
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

SDL_Color MinesweeperApp::numberColor(int n) const {
    switch (n) {
        case 1: return {70, 120, 220, 255};
        case 2: return {60, 160, 80, 255};
        case 3: return {210, 70, 70, 255};
        case 4: return {50, 50, 160, 255};
        case 5: return {140, 50, 50, 255};
        case 6: return {40, 140, 150, 255};
        case 7: return {30, 30, 30, 255};
        case 8: return {120, 120, 120, 255};
        default: return kHudText;
    }
}

void MinesweeperApp::handleEvent(const SDL_Event& event) {
    if (event.type == SDL_KEYDOWN) {
        const SDL_Keycode key = event.key.keysym.sym;
        switch (key) {
            case SDLK_r:
            case SDLK_RETURN:
                newGame(m_difficulty);
                break;
            case SDLK_1:
                newGame(Difficulty::Beginner);
                break;
            case SDLK_2:
                newGame(Difficulty::Intermediate);
                break;
            case SDLK_3:
                newGame(Difficulty::Expert);
                break;
            default:
                break;
        }
        return;
    }

    if (event.type != SDL_MOUSEBUTTONDOWN) return;
    if (event.button.button != SDL_BUTTON_LEFT && event.button.button != SDL_BUTTON_RIGHT) {
        return;
    }

    const int mx = event.button.x;
    const int my = event.button.y;

    // Difficulty buttons (client-relative; button rects set in last render use screen coords)
    // Buttons are stored with contentRect offset. During handleEvent, coords are client-relative
    // (0,0 = content top-left). Recompute buttons in client space.
    {
        const int btnW = 70;
        const int btnH = 18;
        const int btnY = 28;
        const int gap = 6;
        const int startX = 10;
        for (int i = 0; i < 3; ++i) {
            SDL_Rect btn{startX + i * (btnW + gap), btnY, btnW, btnH};
            if (mx >= btn.x && mx < btn.x + btn.w && my >= btn.y && my < btn.y + btn.h) {
                if (i == 0) newGame(Difficulty::Beginner);
                else if (i == 1) newGame(Difficulty::Intermediate);
                else newGame(Difficulty::Expert);
                return;
            }
        }
    }

    if (m_state == State::Won || m_state == State::Lost) return;
    if (m_cellPx <= 0) return;

    // Board origin in client space (content-relative): layout uses contentRect screen offsets.
    // We need the same letterbox math with content origin at 0,0.
    const int availW = m_clientWidth > 0 ? m_clientWidth : 360;
    const int availH = std::max(1, (m_clientHeight > 0 ? m_clientHeight : 420) - kHudHeight - kFooterHeight);
    const int cellPx = std::max(kMinCellPx, std::min(availW / m_width, availH / m_height));
    const int boardPxW = cellPx * m_width;
    const int boardPxH = cellPx * m_height;
    const int boardX = (availW - boardPxW) / 2;
    const int boardY = kHudHeight + (availH - boardPxH) / 2;

    if (mx < boardX || my < boardY || mx >= boardX + boardPxW || my >= boardY + boardPxH) {
        return;
    }

    const int cx = (mx - boardX) / cellPx;
    const int cy = (my - boardY) / cellPx;
    if (!inBounds(cx, cy)) return;

    if (event.button.button == SDL_BUTTON_LEFT) {
        revealCell(cx, cy);
    } else if (event.button.button == SDL_BUTTON_RIGHT) {
        if (m_state == State::Won || m_state == State::Lost) return;
        Cell& c = m_cells[static_cast<size_t>(index(cx, cy))];
        if (!c.revealed) {
            c.flagged = !c.flagged;
        }
    }
}

void MinesweeperApp::render(SDL_Renderer* renderer, const SDL_Rect& contentRect) {
    // Keep client size in sync when first render happens before onResize
    if (m_clientWidth != contentRect.w || m_clientHeight != contentRect.h) {
        m_clientWidth = contentRect.w;
        m_clientHeight = contentRect.h;
    }
    layoutBoard(contentRect);

    // HUD
    SDL_SetRenderDrawColor(renderer, 38, 38, 44, 255);
    SDL_Rect hud{contentRect.x, contentRect.y, contentRect.w, kHudHeight};
    SDL_RenderFillRect(renderer, &hud);

    const int remaining = std::max(0, m_mineCount - flagCount());
    const std::string status = "Mines: " + std::to_string(remaining) +
                               "   Time: " + std::to_string(m_elapsedSec) +
                               "   " + specFor(m_difficulty).name;
    drawText(renderer, status.c_str(), contentRect.x + 10, contentRect.y + 6, kHudText);

    const char* labels[3] = {"1 Begin", "2 Inter", "3 Expert"};
    for (int i = 0; i < 3; ++i) {
        const bool active =
            (i == 0 && m_difficulty == Difficulty::Beginner) ||
            (i == 1 && m_difficulty == Difficulty::Intermediate) ||
            (i == 2 && m_difficulty == Difficulty::Expert);
        const SDL_Color bg = active ? kBtnActive : kBtnBg;
        SDL_SetRenderDrawColor(renderer, bg.r, bg.g, bg.b, 255);
        SDL_RenderFillRect(renderer, &m_diffBtnRects[i]);
        drawText(renderer, labels[i], m_diffBtnRects[i].x + 6, m_diffBtnRects[i].y + 1, kHudText);
    }

    // Board
    for (int y = 0; y < m_height; ++y) {
        for (int x = 0; x < m_width; ++x) {
            const Cell& c = m_cells[static_cast<size_t>(index(x, y))];
            SDL_Rect cell{
                m_boardX + x * m_cellPx,
                m_boardY + y * m_cellPx,
                m_cellPx - 1,
                m_cellPx - 1
            };

            if (!c.revealed) {
                SDL_SetRenderDrawColor(renderer, 70, 74, 88, 255);
                SDL_RenderFillRect(renderer, &cell);
                SDL_SetRenderDrawColor(renderer, 100, 105, 120, 255);
                SDL_RenderDrawRect(renderer, &cell);
                if (c.flagged) {
                    drawCenteredText(renderer, "!", cell, {220, 80, 70, 255});
                }
            } else {
                SDL_SetRenderDrawColor(renderer, 42, 44, 52, 255);
                SDL_RenderFillRect(renderer, &cell);
                if (c.mine) {
                    SDL_SetRenderDrawColor(renderer, 30, 30, 34, 255);
                    const int inset = std::max(2, m_cellPx / 4);
                    SDL_Rect mine{
                        cell.x + inset, cell.y + inset,
                        cell.w - 2 * inset, cell.h - 2 * inset
                    };
                    SDL_RenderFillRect(renderer, &mine);
                    if (m_state == State::Lost) {
                        drawCenteredText(renderer, "X", cell, {220, 70, 70, 255});
                    }
                } else if (c.adjacent > 0) {
                    const char digit[2] = {static_cast<char>('0' + c.adjacent), '\0'};
                    drawCenteredText(renderer, digit, cell, numberColor(c.adjacent));
                }
            }
        }
    }

    // Footer
    SDL_SetRenderDrawColor(renderer, 38, 38, 44, 255);
    SDL_Rect footer{
        contentRect.x,
        contentRect.y + contentRect.h - kFooterHeight,
        contentRect.w,
        kFooterHeight
    };
    SDL_RenderFillRect(renderer, &footer);
    drawText(renderer, "L-click open  R-click flag  R new game", contentRect.x + 10,
             footer.y + 4, kDimText);

    // End overlays
    if (m_state == State::Won || m_state == State::Lost) {
        SDL_Rect boardRect{m_boardX, m_boardY, m_boardPxW, m_boardPxH};
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 130);
        SDL_RenderFillRect(renderer, &boardRect);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        const char* msg = (m_state == State::Won) ? "YOU WIN!" : "BOOM!";
        drawCenteredText(renderer, msg, boardRect, kOverlayText);
    }
}

} // namespace monolith::app
