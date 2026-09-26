#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include <iostream>
#include <string>

#include "../src/detail/TextTextureCache.hpp"

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

    check(TTF_Init() == 0, "text texture cache SDL_ttf initialize");
    TTF_Font* font = TTF_OpenFont("assets/fonts/DejaVuSans.ttf", 14);
    check(font != nullptr, "text texture cache loads test font");
    if (!font) {
        TTF_Quit();
        return 1;
    }

    SDL_Surface* firstSurface = SDL_CreateRGBSurfaceWithFormat(
        0, 320, 120, 32, SDL_PIXELFORMAT_RGBA32);
    SDL_Renderer* firstRenderer = firstSurface
        ? SDL_CreateSoftwareRenderer(firstSurface)
        : nullptr;
    SDL_Surface* secondSurface = SDL_CreateRGBSurfaceWithFormat(
        0, 320, 120, 32, SDL_PIXELFORMAT_RGBA32);
    SDL_Renderer* secondRenderer = secondSurface
        ? SDL_CreateSoftwareRenderer(secondSurface)
        : nullptr;
    check(firstRenderer && secondRenderer,
          "text texture cache creates two software renderers");
    if (!firstRenderer || !secondRenderer) {
        if (firstRenderer) SDL_DestroyRenderer(firstRenderer);
        if (secondRenderer) SDL_DestroyRenderer(secondRenderer);
        if (firstSurface) SDL_FreeSurface(firstSurface);
        if (secondSurface) SDL_FreeSurface(secondSurface);
        TTF_CloseFont(font);
        TTF_Quit();
        return 1;
    }

    monolith::detail::TextTextureCache cache;
    const SDL_Color white{240, 240, 245, 255};
    const auto first = cache.get(firstRenderer, font, "Shared HUD", white);
    const auto repeated = cache.get(firstRenderer, font, "Shared HUD", white);
    check(first && repeated.handle == first.handle
              && repeated.width == first.width && repeated.height == first.height
              && cache.size() == 1,
          "repeated text reuses the same renderer texture");

    const auto colored = cache.get(firstRenderer, font, "Shared HUD", {200, 90, 70, 255});
    check(colored && colored.handle != first.handle && cache.size() == 2,
          "text color participates in the cache key");

    SDL_Texture* newest = nullptr;
    for (int i = 0; i < 300; ++i) {
        const std::string label = "status-" + std::to_string(i);
        newest = cache.get(firstRenderer, font, label.c_str(), white).handle;
    }
    check(newest && cache.size() == monolith::detail::TextTextureCache::kMaxEntries,
          "text texture cache evicts old entries at its count limit");
    check(cache.estimatedBytes() <= monolith::detail::TextTextureCache::kMaxEstimatedBytes,
          "small text entries stay within the estimated byte budget");
    const auto newestAgain = cache.get(firstRenderer, font, "status-299", white);
    check(newestAgain.handle == newest,
          "recently used text survives least-recently-used eviction");

    cache.clear();
    const std::string widePrefix(500, 'W');
    int wideWidth = 0;
    int wideHeight = 0;
    const std::string measuredWideLabel = widePrefix + "0";
    const bool measuredWideText = TTF_SizeUTF8(
        font, measuredWideLabel.c_str(), &wideWidth, &wideHeight) == 0;
    check(measuredWideText, "measure byte-budget test label");
    if (measuredWideText && wideWidth > 0 && wideHeight > 0) {
        const std::uint64_t entryBytes = static_cast<std::uint64_t>(wideWidth)
            * static_cast<std::uint64_t>(wideHeight) * 4;
        const std::size_t wideLabelCount = static_cast<std::size_t>(
            monolith::detail::TextTextureCache::kMaxEstimatedBytes / entryBytes + 4);
        for (std::size_t i = 0; i < wideLabelCount; ++i) {
            const std::string label = widePrefix + std::to_string(i);
            cache.get(firstRenderer, font, label.c_str(), white);
        }
        check(cache.size() < wideLabelCount
                  && cache.estimatedBytes()
                      <= monolith::detail::TextTextureCache::kMaxEstimatedBytes,
              "text texture cache also evicts to stay within its estimated byte budget");
    }

    const auto switched = cache.get(secondRenderer, font, "Renderer switch", white);
    check(switched && cache.size() == 1,
          "switching renderers discards textures owned by the previous renderer");
    cache.clear();
    check(cache.size() == 0 && cache.estimatedBytes() == 0,
          "clear releases cached textures and resets accounting");

    SDL_DestroyRenderer(secondRenderer);
    SDL_DestroyRenderer(firstRenderer);
    SDL_FreeSurface(secondSurface);
    SDL_FreeSurface(firstSurface);
    TTF_CloseFont(font);
    TTF_Quit();

    if (failures == 0) {
        std::cout << "ALL TEXT TEXTURE CACHE TESTS PASSED\n";
        return 0;
    }
    std::cerr << failures << " test(s) failed\n";
    return 1;
}
