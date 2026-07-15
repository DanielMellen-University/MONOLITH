#include "MinesweeperApp.hpp"

#include <algorithm>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <queue>
#include <sstream>
#include <string>

namespace monolith::app {

namespace {
constexpr SDL_Color kHudText{200, 200, 210, 255};
constexpr SDL_Color kDimText{140, 140, 150, 255};
constexpr SDL_Color kOverlayText{245, 245, 250, 255};
constexpr SDL_Color kBtnBg{55, 60, 75, 255};
constexpr SDL_Color kBtnActive{70, 95, 145, 255};
constexpr SDL_Color kGold{230, 190, 70, 255};
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

std::string MinesweeperApp::bestTimesHostPath() {
    const char* home = std::getenv("HOME");
    if (home && *home) {
        return std::string(home) + "/.monolith/minesweeper_best.txt";
    }
    return "./minesweeper_best.txt";
}

void MinesweeperApp::loadBestTimes() {
    m_bestBeginner = m_bestIntermediate = m_bestExpert = 0;
    std::ifstream in(bestTimesHostPath());
    if (!in) return;
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream iss(line);
        std::string key;
        int value = 0;
        if (!(iss >> key >> value) || value <= 0) continue;
        if (key == "beginner") m_bestBeginner = value;
        else if (key == "intermediate") m_bestIntermediate = value;
        else if (key == "expert") m_bestExpert = value;
    }
}

void MinesweeperApp::saveBestTimes() const {
    const std::string path = bestTimesHostPath();
    std::error_code ec;
    std::filesystem::path p(path);
    if (p.has_parent_path()) {
        std::filesystem::create_directories(p.parent_path(), ec);
    }
    std::ofstream out(path, std::ios::trunc);
    if (!out) return;
    if (m_bestBeginner > 0) out << "beginner " << m_bestBeginner << '\n';
    if (m_bestIntermediate > 0) out << "intermediate " << m_bestIntermediate << '\n';
    if (m_bestExpert > 0) out << "expert " << m_bestExpert << '\n';
}

int MinesweeperApp::bestTimeFor(Difficulty d) const {
    switch (d) {
        case Difficulty::Beginner: return m_bestBeginner;
        case Difficulty::Intermediate: return m_bestIntermediate;
        case Difficulty::Expert: return m_bestExpert;
    }
    return 0;
}

void MinesweeperApp::setBestTime(Difficulty d, int seconds) {
    switch (d) {
        case Difficulty::Beginner: m_bestBeginner = seconds; break;
        case Difficulty::Intermediate: m_bestIntermediate = seconds; break;
        case Difficulty::Expert: m_bestExpert = seconds; break;
    }
}

void MinesweeperApp::recordBestTimeIfNeeded() {
    if (m_state != State::Won) return;
    // 0 means "no record" on disk; treat sub-second clears as 1s so the slot stays distinct.
    const int current = std::max(1, std::min(999, m_elapsedSec));
    const int best = bestTimeFor(m_difficulty);
    if (best <= 0 || current < best) {
        setBestTime(m_difficulty, current);
        m_newBest = true;
        saveBestTimes();
    }
}

MinesweeperApp::MinesweeperApp(TTF_Font* font) : m_font(font) {
    static bool seeded = false;
    if (!seeded) {
        std::srand(static_cast<unsigned>(std::time(nullptr)));
        seeded = true;
    }
    loadBestTimes();
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
    m_newBest = false;
    m_hitX = -1;
    m_hitY = -1;
    m_pressing = false;
    m_pressX = -1;
    m_pressY = -1;
}

bool MinesweeperApp::inBounds(int x, int y) const {
    return x >= 0 && y >= 0 && x < m_width && y < m_height;
}

int MinesweeperApp::flagCount() const {
    int n = 0;
    for (const auto& c : m_cells) {
        if (c.mark == Mark::Flag) ++n;
    }
    return n;
}

int MinesweeperApp::neighborFlagCount(int x, int y) const {
    int n = 0;
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            if (dx == 0 && dy == 0) continue;
            const int nx = x + dx;
            const int ny = y + dy;
            if (!inBounds(nx, ny)) continue;
            if (m_cells[static_cast<size_t>(index(nx, ny))].mark == Mark::Flag) ++n;
        }
    }
    return n;
}

void MinesweeperApp::placeMines(int safeX, int safeY) {
    // Clear neighborhood on Beginner and Intermediate for a friendlier first open.
    const bool clearNeighborhood =
        (m_difficulty == Difficulty::Beginner || m_difficulty == Difficulty::Intermediate);
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
        if (c.revealed || c.mark == Mark::Flag || c.mine) continue;
        c.revealed = true;
        c.mark = Mark::None;
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

void MinesweeperApp::loseAt(int x, int y) {
    m_state = State::Lost;
    m_hitX = x;
    m_hitY = y;
    for (auto& c : m_cells) {
        if (c.mine) c.revealed = true;
    }
}

void MinesweeperApp::revealCell(int x, int y) {
    if (!inBounds(x, y)) return;
    if (m_state == State::Won || m_state == State::Lost) return;

    Cell& c = m_cells[static_cast<size_t>(index(x, y))];
    if (c.revealed || c.mark == Mark::Flag) return;

    if (!m_minesPlaced) {
        placeMines(x, y);
    }

    if (c.mine) {
        loseAt(x, y);
        return;
    }

    c.mark = Mark::None;
    if (c.adjacent == 0) {
        floodReveal(x, y);
    } else {
        c.revealed = true;
        ++m_revealedSafe;
    }
    checkWin();
}

void MinesweeperApp::chord(int x, int y) {
    if (!inBounds(x, y)) return;
    if (m_state == State::Won || m_state == State::Lost) return;

    Cell& c = m_cells[static_cast<size_t>(index(x, y))];
    if (!c.revealed || c.mine || c.adjacent == 0) return;
    if (neighborFlagCount(x, y) != static_cast<int>(c.adjacent)) return;

    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            if (dx == 0 && dy == 0) continue;
            const int nx = x + dx;
            const int ny = y + dy;
            if (!inBounds(nx, ny)) continue;
            Cell& n = m_cells[static_cast<size_t>(index(nx, ny))];
            if (n.revealed || n.mark == Mark::Flag) continue;
            revealCell(nx, ny);
            if (m_state == State::Lost || m_state == State::Won) return;
        }
    }
}

void MinesweeperApp::cycleMark(int x, int y) {
    if (!inBounds(x, y)) return;
    if (m_state == State::Won || m_state == State::Lost) return;
    Cell& c = m_cells[static_cast<size_t>(index(x, y))];
    if (c.revealed) return;
    switch (c.mark) {
        case Mark::None:     c.mark = Mark::Flag; break;
        case Mark::Flag:     c.mark = Mark::Question; break;
        case Mark::Question: c.mark = Mark::None; break;
    }
}

void MinesweeperApp::checkWin() {
    const int safeTotal = m_width * m_height - m_mineCount;
    if (m_revealedSafe >= safeTotal) {
        m_state = State::Won;
        for (auto& c : m_cells) {
            if (c.mine) c.mark = Mark::Flag;
        }
        // Freeze displayed time at the win moment.
        if (m_minesPlaced && m_timerStartMs != 0) {
            m_elapsedSec = static_cast<int>((SDL_GetTicks() - m_timerStartMs) / 1000u);
            if (m_elapsedSec > 999) m_elapsedSec = 999;
        }
        recordBestTimeIfNeeded();
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

void MinesweeperApp::clientBoardMetrics(int& boardX, int& boardY, int& cellPx,
                                        int& boardPxW, int& boardPxH) const {
    const int availW = m_clientWidth > 0 ? m_clientWidth : 360;
    const int availH = std::max(1, (m_clientHeight > 0 ? m_clientHeight : 420) - kHudHeight - kFooterHeight);
    cellPx = std::max(kMinCellPx, std::min(availW / m_width, availH / m_height));
    boardPxW = cellPx * m_width;
    boardPxH = cellPx * m_height;
    boardX = (availW - boardPxW) / 2;
    boardY = kHudHeight + (availH - boardPxH) / 2;
}

bool MinesweeperApp::cellAtClient(int mx, int my, int& outX, int& outY) const {
    int boardX = 0, boardY = 0, cellPx = 0, boardPxW = 0, boardPxH = 0;
    clientBoardMetrics(boardX, boardY, cellPx, boardPxW, boardPxH);
    if (cellPx <= 0) return false;
    if (mx < boardX || my < boardY || mx >= boardX + boardPxW || my >= boardY + boardPxH) {
        return false;
    }
    outX = (mx - boardX) / cellPx;
    outY = (my - boardY) / cellPx;
    return inBounds(outX, outY);
}

void MinesweeperApp::layoutBoard(const SDL_Rect& contentRect) {
    const int availW = contentRect.w;
    const int availH = std::max(1, contentRect.h - kHudHeight - kFooterHeight);
    m_cellPx = std::max(kMinCellPx, std::min(availW / m_width, availH / m_height));
    m_boardPxW = m_cellPx * m_width;
    m_boardPxH = m_cellPx * m_height;
    m_boardX = contentRect.x + (availW - m_boardPxW) / 2;
    m_boardY = contentRect.y + kHudHeight + (availH - m_boardPxH) / 2;

    const int btnW = 70;
    const int btnH = 18;
    const int btnY = contentRect.y + 30;
    const int gap = 6;
    const int startX = contentRect.x + 10;
    for (int i = 0; i < 3; ++i) {
        m_diffBtnRects[i] = {startX + i * (btnW + gap), btnY, btnW, btnH};
    }

    // Face / new-game button on the right side of the HUD
    m_faceBtnRect = {
        contentRect.x + contentRect.w - 42,
        contentRect.y + 8,
        32,
        40
    };
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

void MinesweeperApp::drawCenteredLine(SDL_Renderer* renderer, const char* text,
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

    if (event.type == SDL_MOUSEBUTTONUP) {
        if (event.button.button == SDL_BUTTON_LEFT || event.button.button == SDL_BUTTON_MIDDLE) {
            m_pressing = false;
            m_pressX = -1;
            m_pressY = -1;
        }
        return;
    }

    if (event.type != SDL_MOUSEBUTTONDOWN) return;

    const int mx = event.button.x;
    const int my = event.button.y;

    // Face / new game button (client space)
    {
        const int faceX = (m_clientWidth > 0 ? m_clientWidth : 360) - 42;
        const int faceY = 8;
        if (mx >= faceX && mx < faceX + 32 && my >= faceY && my < faceY + 40) {
            newGame(m_difficulty);
            return;
        }
    }

    // Difficulty buttons in client space
    {
        const int btnW = 70;
        const int btnH = 18;
        const int btnY = 30;
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

    // Click end-of-game overlay → new game
    if (m_state == State::Won || m_state == State::Lost) {
        if (event.button.button == SDL_BUTTON_LEFT) {
            newGame(m_difficulty);
        }
        return;
    }

    int cx = 0;
    int cy = 0;
    if (!cellAtClient(mx, my, cx, cy)) return;

    if (event.button.button == SDL_BUTTON_LEFT) {
        Cell& c = m_cells[static_cast<size_t>(index(cx, cy))];
        if (c.revealed && c.adjacent > 0) {
            // Left-click on a numbered cell chords (classic convenience).
            chord(cx, cy);
        } else {
            m_pressing = true;
            m_pressX = cx;
            m_pressY = cy;
            revealCell(cx, cy);
        }
    } else if (event.button.button == SDL_BUTTON_RIGHT) {
        cycleMark(cx, cy);
    } else if (event.button.button == SDL_BUTTON_MIDDLE) {
        m_pressing = true;
        m_pressX = cx;
        m_pressY = cy;
        chord(cx, cy);
    }
}

void MinesweeperApp::render(SDL_Renderer* renderer, const SDL_Rect& contentRect) {
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
    const int best = bestTimeFor(m_difficulty);
    std::string status = "Mines: " + std::to_string(remaining) +
                         "   Time: " + std::to_string(m_elapsedSec);
    if (best > 0) {
        status += "   Best: " + std::to_string(best) + "s";
    }
    status += "   ";
    status += specFor(m_difficulty).name;
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

    // Face button
    {
        SDL_Color faceBg = kBtnBg;
        if (m_state == State::Won) faceBg = {60, 120, 70, 255};
        else if (m_state == State::Lost) faceBg = {140, 60, 60, 255};
        else if (m_pressing) faceBg = {80, 85, 100, 255};
        SDL_SetRenderDrawColor(renderer, faceBg.r, faceBg.g, faceBg.b, 255);
        SDL_RenderFillRect(renderer, &m_faceBtnRect);
        SDL_SetRenderDrawColor(renderer, 100, 105, 120, 255);
        SDL_RenderDrawRect(renderer, &m_faceBtnRect);
        const char* face = ":)";
        if (m_state == State::Won) face = "B)";
        else if (m_state == State::Lost) face = "X(";
        else if (m_pressing) face = ":O";
        drawCenteredText(renderer, face, m_faceBtnRect, kHudText);
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

            const bool isHitMine = (m_state == State::Lost && x == m_hitX && y == m_hitY);
            const bool wrongFlag = (m_state == State::Lost && c.mark == Mark::Flag && !c.mine);
            const bool pressPreview =
                m_pressing && x == m_pressX && y == m_pressY && !c.revealed && c.mark != Mark::Flag;

            if (!c.revealed) {
                if (pressPreview) {
                    SDL_SetRenderDrawColor(renderer, 50, 52, 62, 255);
                } else {
                    SDL_SetRenderDrawColor(renderer, 70, 74, 88, 255);
                }
                SDL_RenderFillRect(renderer, &cell);
                SDL_SetRenderDrawColor(renderer, 100, 105, 120, 255);
                SDL_RenderDrawRect(renderer, &cell);

                if (c.mark == Mark::Flag) {
                    if (wrongFlag) {
                        drawCenteredText(renderer, "X", cell, {220, 80, 70, 255});
                    } else {
                        drawCenteredText(renderer, "!", cell, {220, 80, 70, 255});
                    }
                } else if (c.mark == Mark::Question) {
                    drawCenteredText(renderer, "?", cell, {200, 200, 120, 255});
                }
            } else {
                if (isHitMine) {
                    SDL_SetRenderDrawColor(renderer, 160, 50, 50, 255);
                } else {
                    SDL_SetRenderDrawColor(renderer, 42, 44, 52, 255);
                }
                SDL_RenderFillRect(renderer, &cell);

                if (c.mine) {
                    SDL_SetRenderDrawColor(renderer, 20, 20, 24, 255);
                    const int inset = std::max(2, m_cellPx / 4);
                    SDL_Rect mine{
                        cell.x + inset, cell.y + inset,
                        cell.w - 2 * inset, cell.h - 2 * inset
                    };
                    SDL_RenderFillRect(renderer, &mine);
                    if (m_state == State::Lost) {
                        drawCenteredText(renderer, "*", cell,
                                         isHitMine ? SDL_Color{255, 220, 80, 255}
                                                   : SDL_Color{200, 70, 70, 255});
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
    drawText(renderer, "L open  R flag/?  M/chord  face=new", contentRect.x + 10,
             footer.y + 4, kDimText);

    // End overlays — centered vertical stack
    if (m_state == State::Won || m_state == State::Lost) {
        SDL_Rect boardRect{m_boardX, m_boardY, m_boardPxW, m_boardPxH};
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 130);
        SDL_RenderFillRect(renderer, &boardRect);
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

        const char* title = (m_state == State::Won) ? "YOU WIN!" : "BOOM!";
        std::string line2;
        std::string line3;
        SDL_Color line3Color = kDimText;

        if (m_state == State::Won) {
            line2 = "Time " + std::to_string(m_elapsedSec) + "s";
            const int bestNow = bestTimeFor(m_difficulty);
            if (bestNow > 0) {
                line2 += "  ·  Best " + std::to_string(bestNow) + "s";
            }
            if (m_newBest) {
                line3 = "NEW BEST!";
                line3Color = kGold;
            } else {
                line3 = "Click or R for new game";
            }
        } else {
            line2 = "Click or R for new game";
        }

        const int lineH = 18;
        const int gap = 4;
        const int lines = 1 + (!line2.empty() ? 1 : 0) + (!line3.empty() ? 1 : 0);
        const int blockH = lines * lineH + (lines - 1) * gap;
        int y = boardRect.y + (boardRect.h - blockH) / 2;

        drawCenteredLine(renderer, title, boardRect, y, kOverlayText);
        y += lineH + gap;
        if (!line2.empty()) {
            drawCenteredLine(renderer, line2.c_str(), boardRect, y, kDimText);
            y += lineH + gap;
        }
        if (!line3.empty()) {
            drawCenteredLine(renderer, line3.c_str(), boardRect, y, line3Color);
        }
    }
}

} // namespace monolith::app
