// Headless test of shipped Snake movement rules.
// Compiles against src/app/SnakeApp.cpp and does not render a window.

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include <cstdint>
#include <cstdlib>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <new>
#include <string>
#include <utility>

#include "TestTempDir.hpp"
#include "../src/app/App.hpp"
#include "../src/detail/TickMath.hpp"

#define private public
#include "../src/app/SnakeApp.hpp"
#undef private

using monolith::app::SnakeApp;

namespace {
bool g_trackAllocations = false;
std::size_t g_trackedAllocations = 0;
}

void* operator new(std::size_t size) {
    if (g_trackAllocations) ++g_trackedAllocations;
    if (void* memory = std::malloc(size == 0 ? 1 : size)) return memory;
    throw std::bad_alloc();
}

void* operator new[](std::size_t size) {
    if (g_trackAllocations) ++g_trackedAllocations;
    if (void* memory = std::malloc(size == 0 ? 1 : size)) return memory;
    throw std::bad_alloc();
}

void operator delete(void* memory) noexcept { std::free(memory); }
void operator delete(void* memory, std::size_t) noexcept { std::free(memory); }
void operator delete[](void* memory) noexcept { std::free(memory); }
void operator delete[](void* memory, std::size_t) noexcept { std::free(memory); }

int main() {
    int failures = 0;
    auto check = [&](bool ok, const char* message) {
        if (!ok) {
            std::cerr << "FAIL: " << message << '\n';
            ++failures;
        } else {
            std::cout << "ok: " << message << '\n';
        }
    };

    check(TTF_Init() == 0, "Snake state SDL_ttf initialize");
    TTF_Font* font = TTF_OpenFont("assets/fonts/DejaVuSans.ttf", 14);
    check(font != nullptr, "Snake state loads test font");
    if (!font) {
        TTF_Quit();
        return 1;
    }

    const char* originalHomeValue = std::getenv("HOME");
    const bool hadOriginalHome = originalHomeValue != nullptr;
    const std::string originalHome = originalHomeValue ? originalHomeValue : "";
    monolith::test::ScopedTempDirectory testHomeTemp("monolith-snake-state");
    const std::filesystem::path testHome = testHomeTemp.path();
    std::error_code cleanupError;
    const bool homeConfigured = testHomeTemp
        && setenv("HOME", testHome.c_str(), 1) == 0;
    check(homeConfigured,
          "Snake state isolates host score persistence");
    if (!homeConfigured) {
        TTF_CloseFont(font);
        TTF_Quit();
        return 1;
    }

    SnakeApp game(font);
    const std::filesystem::path scorePath = testHome / ".monolith/snake_highscore.txt";
    game.m_highScore = 17;
    game.saveHighScore();
    std::ifstream scoreFile(scorePath);
    const std::string scoreText(std::istreambuf_iterator<char>(scoreFile), {});
    check(scoreText == "17\n", "Snake saves a complete high-score record");
    check(!std::filesystem::exists(scorePath.string() + ".tmp"),
          "Snake removes the temporary score record after replacement");

    SnakeApp reloaded(font);
    check(reloaded.m_highScore == 17, "Snake reloads the replaced high score");

    {
        std::ofstream scoreFixture(scorePath, std::ios::binary | std::ios::trunc);
        scoreFixture << "397" << std::string(28, ' ') << "\r\n";
    }
    SnakeApp crlfScore(font);
    check(crlfScore.m_highScore == 397,
          "Snake accepts a bounded CRLF high-score record at the board limit");

    {
        std::ofstream scoreFixture(scorePath, std::ios::binary | std::ios::trunc);
        scoreFixture << "17 trailing\n";
    }
    SnakeApp trailingScore(font);
    check(trailingScore.m_highScore == 0,
          "Snake rejects trailing data in a high-score record");

    {
        std::ofstream scoreFixture(scorePath, std::ios::binary | std::ios::trunc);
        scoreFixture << "398\n";
    }
    SnakeApp impossibleScore(font);
    check(impossibleScore.m_highScore == 0,
          "Snake rejects scores that exceed the playable board");

    {
        std::ofstream scoreFixture(scorePath, std::ios::binary | std::ios::trunc);
        scoreFixture << std::string(33, '9') << '\n';
    }
    SnakeApp oversizedScore(font);
    check(oversizedScore.m_highScore == 0,
          "Snake ignores an oversized persisted high-score record");

    {
        std::ofstream scoreFixture(scorePath, std::ios::binary | std::ios::trunc);
        scoreFixture << std::string(16, '\n') << "17\n";
    }
    SnakeApp excessiveScoreRows(font);
    check(excessiveScoreRows.m_highScore == 0,
          "Snake bounds the number of rows scanned in its score file");

    std::filesystem::remove(scorePath, cleanupError);
    check(std::filesystem::create_directory(scorePath),
          "create blocked Snake score target");
    game.m_highScore = 17;
    game.m_score = 23;
    game.maybeUpdateHighScore();
    check(game.m_highScore == 23 && game.m_newHighScore
              && game.m_highScoreSaveFailed,
          "Snake keeps a new in-session record and reports a failed save");
    check(std::filesystem::is_directory(scorePath)
              && !std::filesystem::exists(scorePath.string() + ".tmp"),
          "failed Snake score replacement preserves the target and cleans up");
    std::filesystem::remove_all(scorePath, cleanupError);
    game.onFocusGained();
    std::ifstream recoveredScoreFile(scorePath);
    const std::string recoveredScore(
        std::istreambuf_iterator<char>(recoveredScoreFile), {});
    check(!game.m_highScoreSaveFailed && recoveredScore == "23\n",
          "Snake retries and persists the record after focus returns");

    const int baseHudHeight = game.hudHeight();
    check(TTF_SetFontSize(font, 22) == 0, "Snake state scales test font");
    const int scaledHudHeight = game.hudHeight();
    check(scaledHudHeight > baseHudHeight,
          "Snake HUD grows with the shared interface font");
    const SDL_Rect tinyContent{0, 0, 120, 80};
    game.layoutBoard(tinyContent);
    check(game.m_boardX >= tinyContent.x
              && game.m_boardY >= tinyContent.y + scaledHudHeight
              && game.m_boardX + game.m_boardPxW <= tinyContent.x + tinyContent.w
              && game.m_boardY + game.m_boardPxH <= tinyContent.y + tinyContent.h,
          "Snake keeps the complete board inside a tiny client area");
    const auto tailTurnBody = [] {
        return std::deque<std::pair<int, int>>{
            {2, 1}, // head
            {2, 2},
            {1, 2},
            {1, 1}, // tail and next destination
        };
    };

    game.m_body = tailTurnBody();
    game.m_dir = SnakeApp::Dir::Left;
    game.m_dirQueue.clear();
    game.m_foodX = 10;
    game.m_foodY = 10;
    game.m_state = SnakeApp::State::Playing;
    game.m_paused = false;
    game.step();

    check(game.m_state == SnakeApp::State::Playing,
          "moving into a vacating tail square stays alive");
    check(game.m_body.size() == 4 && game.m_body.front() == std::make_pair(1, 1),
          "tail entry advances the body without changing length");

    game.m_body = tailTurnBody();
    game.m_dir = SnakeApp::Dir::Left;
    game.m_dirQueue.clear();
    game.m_foodX = 1;
    game.m_foodY = 1;
    game.m_state = SnakeApp::State::Playing;
    game.m_paused = false;
    game.step();

    check(game.m_state == SnakeApp::State::GameOver,
          "moving into the tail while eating is a collision");
    check(game.m_body.size() == 4 && game.m_body.front() == std::make_pair(2, 1),
          "food collision leaves the body unchanged");

    game.m_state = SnakeApp::State::Playing;
    game.m_paused = true;
    SDL_Event keypadEnter{};
    keypadEnter.type = SDL_KEYDOWN;
    keypadEnter.key.keysym.sym = SDLK_KP_ENTER;
    game.handleEvent(keypadEnter);
    check(!game.m_paused, "keypad Enter resumes a paused Snake game");

    check(monolith::detail::tickDeadlinePending(0xfffffff0u, 0x00000050u),
          "Snake flash deadline remains active across tick wraparound");
    check(!monolith::detail::tickDeadlinePending(0x00000060u, 0x00000050u),
          "Snake flash deadline expires after tick wraparound");
    check(!monolith::detail::tickDeadlinePending(0x00000050u, 0x00000050u),
          "Snake flash deadline is inactive at its exact expiry");

    SDL_Surface* snakeSurface = SDL_CreateRGBSurfaceWithFormat(
        0, 240, 240, 32, SDL_PIXELFORMAT_RGBA32);
    SDL_Renderer* snakeRenderer = snakeSurface
        ? SDL_CreateSoftwareRenderer(snakeSurface)
        : nullptr;
    check(snakeRenderer != nullptr, "Snake state creates a software renderer");
    if (snakeRenderer) {
        SDL_SetRenderDrawBlendMode(snakeRenderer, SDL_BLENDMODE_ADD);
        game.m_score = 23;
        game.m_highScore = 41;
        game.m_state = SnakeApp::State::GameOver;
        game.m_highScoreSaveFailed = true;
        game.m_clientWidth = 1;
        game.m_clientHeight = 1;
        game.render(snakeRenderer, {0, 0, 240, 240});
        check(game.m_clientWidth == 240 && game.m_clientHeight == 240,
              "Snake direct renders synchronize cached client geometry");
        const size_t cachedTextureCount = game.m_textTextureCache.size();
        const auto gameOverTexture = game.m_textTextureCache.get(
            snakeRenderer, font, "GAME OVER", {245, 245, 250, 255});
        g_trackedAllocations = 0;
        g_trackAllocations = true;
        game.render(snakeRenderer, {0, 0, 240, 240});
        g_trackAllocations = false;
        check(g_trackedAllocations == 0,
              "Snake warmed game-over overlay rendering performs no heap allocations");
        const size_t scoreLineTextureCount = game.m_textTextureCache.size();
        const auto scoreLineTexture = game.m_textTextureCache.get(
            snakeRenderer, font, "Score 23  ·  Best 41", {140, 140, 150, 255});
        check(scoreLineTexture.handle
                  && game.m_textTextureCache.size() == scoreLineTextureCount,
              "Snake stack-formatted game-over score preserves text and spacing");
        const auto repeatedGameOverTexture = game.m_textTextureCache.get(
            snakeRenderer, font, "GAME OVER", {245, 245, 250, 255});
        check(cachedTextureCount > 0 && repeatedGameOverTexture.handle
                  == gameOverTexture.handle
                  && game.m_textTextureCache.size() == cachedTextureCount,
              "Snake reuses cached text textures between frames");
        const size_t warningTextureCount = game.m_textTextureCache.size();
        const auto warningTexture = game.m_textTextureCache.get(
            snakeRenderer, font, "BEST NOT SAVED", {240, 160, 90, 255});
        check(warningTexture.handle
                  && game.m_textTextureCache.size() == warningTextureCount,
              "Snake renders a warning instead of a false new-best claim");
        game.onUiScaleChanged();
        check(game.m_textTextureCache.size() == 0,
              "Snake releases cached text textures when UI scale changes");
        SDL_BlendMode restoredBlend = SDL_BLENDMODE_NONE;
        SDL_GetRenderDrawBlendMode(snakeRenderer, &restoredBlend);
        check(restoredBlend == SDL_BLENDMODE_ADD,
              "Snake overlay restores the caller renderer blend mode");
        SDL_DestroyRenderer(snakeRenderer);
    }
    if (snakeSurface) SDL_FreeSurface(snakeSurface);

    if (hadOriginalHome) {
        setenv("HOME", originalHome.c_str(), 1);
    } else {
        unsetenv("HOME");
    }

    if (failures == 0) {
        std::cout << "ALL SNAKE STATE TESTS PASSED\n";
        TTF_CloseFont(font);
        TTF_Quit();
        return 0;
    }
    std::cerr << failures << " test(s) failed\n";
    TTF_CloseFont(font);
    TTF_Quit();
    return 1;
}
