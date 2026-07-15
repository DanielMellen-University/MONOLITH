#pragma once

#include "App.hpp"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <cstdint>
#include <string>
#include <vector>

namespace monolith::app {

/**
 * Classic Minesweeper for Monolith.
 * Left-click reveal, right-click flag/? cycle, middle/chord open neighbors.
 * Best times per difficulty under ~/.monolith/.
 */
class MinesweeperApp : public App {
public:
    explicit MinesweeperApp(TTF_Font* font);
    ~MinesweeperApp() override = default;

    void render(SDL_Renderer* renderer, const SDL_Rect& contentRect) override;
    void handleEvent(const SDL_Event& event) override;
    void update() override;
    void onResize(int clientWidth, int clientHeight) override;

private:
    enum class Difficulty { Beginner, Intermediate, Expert };
    enum class State { Ready, Playing, Won, Lost };
    enum class Mark { None, Flag, Question };

    struct Cell {
        bool mine = false;
        bool revealed = false;
        Mark mark = Mark::None;
        uint8_t adjacent = 0;
    };

    struct DifficultySpec {
        const char* name;
        int width;
        int height;
        int mines;
    };

    static constexpr int kHudHeight = 56;
    static constexpr int kFooterHeight = 24;
    static constexpr int kMinCellPx = 8;

    static const DifficultySpec& specFor(Difficulty d);
    void newGame(Difficulty d);
    void resetBoard();
    void placeMines(int safeX, int safeY);
    void computeAdjacents();
    void revealCell(int x, int y);
    void floodReveal(int x, int y);
    void chord(int x, int y);
    void cycleMark(int x, int y);
    void checkWin();
    void loseAt(int x, int y);
    void recordBestTimeIfNeeded();
    int index(int x, int y) const { return y * m_width + x; }
    bool inBounds(int x, int y) const;
    int flagCount() const;
    int neighborFlagCount(int x, int y) const;
    void layoutBoard(const SDL_Rect& contentRect);
    void clientBoardMetrics(int& boardX, int& boardY, int& cellPx, int& boardPxW, int& boardPxH) const;
    bool cellAtClient(int mx, int my, int& outX, int& outY) const;
    void drawText(SDL_Renderer* renderer, const char* text, int x, int y,
                  SDL_Color color) const;
    void drawCenteredText(SDL_Renderer* renderer, const char* text,
                          const SDL_Rect& area, SDL_Color color) const;
    SDL_Color numberColor(int n) const;
    void loadBestTimes();
    void saveBestTimes() const;
    static std::string bestTimesHostPath();
    int bestTimeFor(Difficulty d) const;
    void setBestTime(Difficulty d, int seconds);

    TTF_Font* m_font = nullptr;

    Difficulty m_difficulty = Difficulty::Beginner;
    int m_width = 9;
    int m_height = 9;
    int m_mineCount = 10;
    std::vector<Cell> m_cells;
    bool m_minesPlaced = false;
    State m_state = State::Ready;
    int m_revealedSafe = 0;

    Uint32 m_timerStartMs = 0;
    int m_elapsedSec = 0;
    bool m_newBest = false;

    // Hit mine position on loss (-1 = unknown / chord multi-loss)
    int m_hitX = -1;
    int m_hitY = -1;

    // Best times in seconds; 0 means none recorded yet
    int m_bestBeginner = 0;
    int m_bestIntermediate = 0;
    int m_bestExpert = 0;

    // Pressed cell preview (client grid coords)
    bool m_pressing = false;
    int m_pressX = -1;
    int m_pressY = -1;

    int m_clientWidth = 0;
    int m_clientHeight = 0;
    int m_cellPx = 24;
    int m_boardX = 0;
    int m_boardY = 0;
    int m_boardPxW = 0;
    int m_boardPxH = 0;

    SDL_Rect m_diffBtnRects[3]{};
    SDL_Rect m_faceBtnRect{};
};

} // namespace monolith::app
