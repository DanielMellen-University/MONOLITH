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
#include <string>
#include <utility>
#include <unistd.h>

#include "../src/app/App.hpp"
#include "../src/detail/TickMath.hpp"

#define private public
#include "../src/app/SnakeApp.hpp"
#undef private

using monolith::app::SnakeApp;

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
    const std::filesystem::path testHome =
        std::filesystem::temp_directory_path()
        / ("monolith-snake-state-" + std::to_string(static_cast<long long>(getpid())));
    std::error_code cleanupError;
    std::filesystem::remove_all(testHome, cleanupError);
    const bool homeReady = std::filesystem::create_directories(testHome);
    const bool homeConfigured = homeReady && setenv("HOME", testHome.c_str(), 1) == 0;
    check(homeConfigured,
          "Snake state isolates host score persistence");
    if (!homeConfigured) {
        std::filesystem::remove_all(testHome, cleanupError);
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

    std::filesystem::remove(scorePath, cleanupError);
    check(std::filesystem::create_directory(scorePath),
          "create blocked Snake score target");
    game.m_highScore = 23;
    game.saveHighScore();
    check(std::filesystem::is_directory(scorePath)
              && !std::filesystem::exists(scorePath.string() + ".tmp"),
          "failed Snake score replacement preserves the target and cleans up");

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
        game.m_state = SnakeApp::State::GameOver;
        game.m_clientWidth = 1;
        game.m_clientHeight = 1;
        game.render(snakeRenderer, {0, 0, 240, 240});
        check(game.m_clientWidth == 240 && game.m_clientHeight == 240,
              "Snake direct renders synchronize cached client geometry");
        const size_t cachedTextureCount = game.m_textTextureCache.size();
        const auto gameOverTexture = game.m_textTextureCache.get(
            snakeRenderer, font, "GAME OVER", {245, 245, 250, 255});
        game.render(snakeRenderer, {0, 0, 240, 240});
        const auto repeatedGameOverTexture = game.m_textTextureCache.get(
            snakeRenderer, font, "GAME OVER", {245, 245, 250, 255});
        check(cachedTextureCount > 0 && repeatedGameOverTexture.handle
                  == gameOverTexture.handle
                  && game.m_textTextureCache.size() == cachedTextureCount,
              "Snake reuses cached text textures between frames");
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

    std::filesystem::remove_all(testHome, cleanupError);
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
