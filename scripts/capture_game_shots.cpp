// Regenerates docs/images/pong.png and docs/images/breakout.png.
// Not part of the test suite. From the repository root:
// g++ -std=c++23 -Isrc scripts/capture_game_shots.cpp src/app/PongApp.cpp src/app/PongLogic.cpp src/app/BreakoutApp.cpp src/app/BreakoutLogic.cpp $(pkg-config --cflags --libs sdl2 SDL2_ttf) -lz -o build/capture_game_shots && SDL_VIDEODRIVER=dummy ./build/capture_game_shots

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <zlib.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#define private public
#include "app/BreakoutApp.hpp"
#include "app/PongApp.hpp"
#undef private

namespace {

constexpr int kPongClientW = 420;
constexpr int kPongClientH = 320 - 32;
constexpr int kBreakoutClientW = 420;
constexpr int kBreakoutClientH = 360 - 32;

void stepPong(monolith::app::PongApp& game) {
    for (int i = 0; i < 36; ++i) {
        game.m_game.tick(1.f / 60.f);
    }
}

void stepBreakout(monolith::app::BreakoutApp& game) {
    for (int i = 0; i < 20; ++i) {
        const float paddleCenter =
            game.m_game.paddleX + static_cast<float>(monolith::breakout::Game::kPaddleW) * 0.5f;
        const float ballCenter =
            game.m_game.ballX + static_cast<float>(monolith::breakout::Game::kBallSize) * 0.5f;
        auto input = monolith::breakout::Input::None;
        if (ballCenter < paddleCenter - 2.f) input = monolith::breakout::Input::Left;
        else if (ballCenter > paddleCenter + 2.f) input = monolith::breakout::Input::Right;
        game.m_game.applyInput(input, 1.f / 60.f);
        game.m_game.tick(1.f / 60.f);
    }
}

bool writePng(const std::string& path, SDL_Surface* surface) {
    if (SDL_LockSurface(surface) != 0) {
        std::fprintf(stderr, "lock surface failed: %s\n", SDL_GetError());
        return false;
    }
    const int width = surface->w;
    const int height = surface->h;
    std::vector<unsigned char> raw(static_cast<std::size_t>(height) * (static_cast<std::size_t>(width) * 3u + 1u));
    std::size_t dst = 0;
    const auto* pixels = static_cast<const unsigned char*>(surface->pixels);
    for (int y = 0; y < height; ++y) {
        raw[dst++] = 0;
        const unsigned char* row = pixels + static_cast<std::size_t>(y) * static_cast<std::size_t>(surface->pitch);
        for (int x = 0; x < width; ++x) {
            Uint32 pixel = 0;
            std::memcpy(&pixel, row + static_cast<std::size_t>(x) * 4u, sizeof(pixel));
            Uint8 r = 0;
            Uint8 g = 0;
            Uint8 b = 0;
            Uint8 a = 0;
            SDL_GetRGBA(pixel, surface->format, &r, &g, &b, &a);
            (void)a;
            raw[dst++] = r;
            raw[dst++] = g;
            raw[dst++] = b;
        }
    }
    SDL_UnlockSurface(surface);

    uLongf compressedBound = compressBound(static_cast<uLong>(raw.size()));
    std::vector<unsigned char> compressed(compressedBound);
    if (compress2(compressed.data(), &compressedBound, raw.data(), static_cast<uLong>(raw.size()), 9) != Z_OK) {
        std::fprintf(stderr, "png compress failed\n");
        return false;
    }
    compressed.resize(compressedBound);

    auto chunk = [](const char* tag, const unsigned char* data, std::size_t size) {
        std::vector<unsigned char> out(12 + size);
        const auto length = static_cast<std::uint32_t>(size);
        out[0] = static_cast<unsigned char>((length >> 24) & 0xff);
        out[1] = static_cast<unsigned char>((length >> 16) & 0xff);
        out[2] = static_cast<unsigned char>((length >> 8) & 0xff);
        out[3] = static_cast<unsigned char>(length & 0xff);
        std::memcpy(out.data() + 4, tag, 4);
        if (size > 0) std::memcpy(out.data() + 8, data, size);
        const auto crc = static_cast<std::uint32_t>(crc32(0L, out.data() + 4, static_cast<uInt>(4 + size)));
        out[8 + size] = static_cast<unsigned char>((crc >> 24) & 0xff);
        out[9 + size] = static_cast<unsigned char>((crc >> 16) & 0xff);
        out[10 + size] = static_cast<unsigned char>((crc >> 8) & 0xff);
        out[11 + size] = static_cast<unsigned char>(crc & 0xff);
        return out;
    };

    unsigned char ihdr[13];
    const auto pngW = static_cast<std::uint32_t>(width);
    const auto pngH = static_cast<std::uint32_t>(height);
    ihdr[0] = static_cast<unsigned char>((pngW >> 24) & 0xff);
    ihdr[1] = static_cast<unsigned char>((pngW >> 16) & 0xff);
    ihdr[2] = static_cast<unsigned char>((pngW >> 8) & 0xff);
    ihdr[3] = static_cast<unsigned char>(pngW & 0xff);
    ihdr[4] = static_cast<unsigned char>((pngH >> 24) & 0xff);
    ihdr[5] = static_cast<unsigned char>((pngH >> 16) & 0xff);
    ihdr[6] = static_cast<unsigned char>((pngH >> 8) & 0xff);
    ihdr[7] = static_cast<unsigned char>(pngH & 0xff);
    ihdr[8] = 8;
    ihdr[9] = 2;
    ihdr[10] = 0;
    ihdr[11] = 0;
    ihdr[12] = 0;

    const std::vector<unsigned char> signature = {137, 80, 78, 71, 13, 10, 26, 10};
    const std::vector<unsigned char> ihdrChunk = chunk("IHDR", ihdr, sizeof(ihdr));
    const std::vector<unsigned char> idatChunk = chunk("IDAT", compressed.data(), compressed.size());
    const std::vector<unsigned char> iendChunk = chunk("IEND", nullptr, 0);

    FILE* file = std::fopen(path.c_str(), "wb");
    if (!file) {
        std::fprintf(stderr, "open %s failed\n", path.c_str());
        return false;
    }
    const auto writeAll = [&](const std::vector<unsigned char>& bytes) {
        return std::fwrite(bytes.data(), 1, bytes.size(), file) == bytes.size();
    };
    const bool wrote = writeAll(signature) && writeAll(ihdrChunk) && writeAll(idatChunk) && writeAll(iendChunk);
    std::fclose(file);
    if (!wrote) {
        std::fprintf(stderr, "write %s failed\n", path.c_str());
        return false;
    }
    return true;
}

bool renderShot(SDL_Renderer* renderer, SDL_Surface* surface, const SDL_Rect& rect,
                const std::string& path) {
    SDL_RenderSetClipRect(renderer, nullptr);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_RenderPresent(renderer);
    if (!writePng(path, surface)) return false;
    const long bytes = [&]() {
        FILE* file = std::fopen(path.c_str(), "rb");
        if (!file) return 0L;
        std::fseek(file, 0, SEEK_END);
        const long size = std::ftell(file);
        std::fclose(file);
        return size;
    }();
    std::printf("wrote %s (%ld bytes, %dx%d)\n", path.c_str(), bytes, rect.w, rect.h);
    return bytes > 1000;
}

} // namespace

int main() {
    if (!std::getenv("SDL_VIDEODRIVER")) {
        setenv("SDL_VIDEODRIVER", "dummy", 0);
    }
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    if (TTF_Init() != 0) {
        std::fprintf(stderr, "TTF_Init: %s\n", TTF_GetError());
        SDL_Quit();
        return 1;
    }
    TTF_Font* font = TTF_OpenFont("assets/fonts/DejaVuSans.ttf", 14);
    if (!font) {
        std::fprintf(stderr, "font: %s\n", TTF_GetError());
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    int failures = 0;
    {
        SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(0, kPongClientW, kPongClientH, 32, SDL_PIXELFORMAT_RGBA32);
        SDL_Renderer* renderer = surface ? SDL_CreateSoftwareRenderer(surface) : nullptr;
        if (!renderer) {
            std::fprintf(stderr, "pong renderer failed\n");
            ++failures;
        } else {
            monolith::app::PongApp game(font);
            stepPong(game);
            const SDL_Rect rect{0, 0, kPongClientW, kPongClientH};
            game.render(renderer, rect);
            if (!renderShot(renderer, surface, rect, "docs/images/pong.png")) ++failures;
            SDL_DestroyRenderer(renderer);
        }
        if (surface) SDL_FreeSurface(surface);
    }
    {
        SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(
            0, kBreakoutClientW, kBreakoutClientH, 32, SDL_PIXELFORMAT_RGBA32);
        SDL_Renderer* renderer = surface ? SDL_CreateSoftwareRenderer(surface) : nullptr;
        if (!renderer) {
            std::fprintf(stderr, "breakout renderer failed\n");
            ++failures;
        } else {
            monolith::app::BreakoutApp game(font);
            stepBreakout(game);
            const SDL_Rect rect{0, 0, kBreakoutClientW, kBreakoutClientH};
            game.render(renderer, rect);
            if (!renderShot(renderer, surface, rect, "docs/images/breakout.png")) ++failures;
            SDL_DestroyRenderer(renderer);
        }
        if (surface) SDL_FreeSurface(surface);
    }

    TTF_CloseFont(font);
    TTF_Quit();
    SDL_Quit();
    return failures == 0 ? 0 : 1;
}
