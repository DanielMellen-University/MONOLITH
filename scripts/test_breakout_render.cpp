#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include <iostream>

#define private public
#include "../src/app/BreakoutApp.hpp"
#undef private

using monolith::app::BreakoutApp;

namespace {
struct Pixel {
    Uint8 r;
    Uint8 g;
    Uint8 b;
    Uint8 a;
};

bool readPixel(SDL_Renderer* renderer, const SDL_Rect& rect, Pixel& pixel) {
    const SDL_Rect sample{
        rect.x + rect.w / 2,
        rect.y + rect.h / 2,
        1,
        1
    };
    return SDL_RenderReadPixels(renderer, &sample, SDL_PIXELFORMAT_RGBA32,
                                &pixel, sizeof(pixel)) == 0;
}

bool hasColor(const Pixel& pixel, Uint8 r, Uint8 g, Uint8 b) {
    return pixel.r == r && pixel.g == g && pixel.b == b && pixel.a == 255;
}
} // namespace

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

    const bool videoReady = SDL_Init(SDL_INIT_VIDEO) == 0;
    check(videoReady, "Breakout render initializes SDL video");
    if (!videoReady) return 1;
    const bool ttfReady = TTF_Init() == 0;
    check(ttfReady, "Breakout render initializes SDL_ttf");
    if (!ttfReady) {
        SDL_Quit();
        return 1;
    }

    TTF_Font* font = TTF_OpenFont("assets/fonts/DejaVuSans.ttf", 14);
    check(font != nullptr, "Breakout render loads its test font");
    if (!font) {
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(
        0, 320, 300, 32, SDL_PIXELFORMAT_RGBA32);
    SDL_Renderer* renderer = surface ? SDL_CreateSoftwareRenderer(surface) : nullptr;
    check(renderer != nullptr, "Breakout render creates a software renderer");
    if (renderer) {
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        {
            BreakoutApp game(font);
            game.render(renderer, {0, 0, 320, 300});

            Pixel rowZero{};
            Pixel rowOne{};
            Pixel rowFour{};
            const bool readInitialRows =
                readPixel(renderer, game.m_brickRects[0], rowZero)
                && readPixel(renderer,
                             game.m_brickRects[monolith::breakout::Game::kBrickCols],
                             rowOne)
                && readPixel(renderer,
                             game.m_brickRects[(monolith::breakout::Game::kBrickRows - 1)
                                 * monolith::breakout::Game::kBrickCols],
                             rowFour);
            check(readInitialRows && hasColor(rowZero, 230, 90, 90)
                      && hasColor(rowOne, 230, 150, 70)
                      && hasColor(rowFour, 90, 160, 230),
                  "Breakout row batches preserve each brick color");

            game.m_game.setBrickAlive(1, 0, false);
            --game.m_game.bricksLeft;
            game.render(renderer, {0, 0, 320, 300});
            Pixel removedBrick{};
            Pixel adjacentBrick{};
            const bool readChangedRow =
                readPixel(renderer, game.m_brickRects[1], removedBrick)
                && readPixel(renderer, game.m_brickRects[0], adjacentBrick);
            check(readChangedRow && hasColor(removedBrick, 28, 32, 42)
                      && hasColor(adjacentBrick, 230, 90, 90),
                  "Breakout batch skips removed bricks without leaving stale pixels");

            const int priorBrickWidth = game.m_brickRects[0].w;
            game.render(renderer, {0, 0, 280, 300});
            Pixel resizedBrick{};
            const bool resizedBrickRead = readPixel(renderer, game.m_brickRects[0], resizedBrick);
            check(game.m_brickRectsValid && game.m_brickContentRect.w == 280
                      && game.m_brickRects[0].w != priorBrickWidth
                      && resizedBrickRead && hasColor(resizedBrick, 230, 90, 90),
                  "Breakout rebuilds cached brick geometry after direct resizing");
        }

        SDL_DestroyRenderer(renderer);
    }
    if (surface) SDL_FreeSurface(surface);
    TTF_CloseFont(font);
    TTF_Quit();
    SDL_Quit();
    if (failures != 0) {
        std::cerr << failures << " test(s) failed\n";
        return 1;
    }
    std::cout << "ALL BREAKOUT RENDER TESTS PASSED\n";
    return 0;
}
