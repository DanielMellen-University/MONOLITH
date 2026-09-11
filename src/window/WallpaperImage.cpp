#include "WallpaperImage.hpp"

#include "../app/FilePath.hpp"

#include <cstdio>
#include <cstring>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#define STBI_NO_STDIO
#include "stb_image.h"

namespace monolith::window {
namespace {

SDL_Surface* surfaceFromRgba(unsigned char* rgba, int width, int height) {
    if (!rgba || width <= 0 || height <= 0) {
        return nullptr;
    }

    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(
        0, width, height, 32, SDL_PIXELFORMAT_RGBA32);
    if (!surface) {
        return nullptr;
    }

    if (SDL_LockSurface(surface) != 0) {
        SDL_FreeSurface(surface);
        return nullptr;
    }

    const int srcPitch = width * 4;
    auto* dst = static_cast<unsigned char*>(surface->pixels);
    if (surface->pitch == srcPitch) {
        std::memcpy(dst, rgba, static_cast<size_t>(srcPitch) * static_cast<size_t>(height));
    } else {
        for (int y = 0; y < height; ++y) {
            std::memcpy(
                dst + static_cast<size_t>(y) * static_cast<size_t>(surface->pitch),
                rgba + static_cast<size_t>(y) * static_cast<size_t>(srcPitch),
                static_cast<size_t>(srcPitch));
        }
    }
    SDL_UnlockSurface(surface);
    return surface;
}

SDL_Surface* loadWithStb(const std::string& hostPath) {
    FILE* file = std::fopen(hostPath.c_str(), "rb");
    if (!file) {
        return nullptr;
    }
    if (std::fseek(file, 0, SEEK_END) != 0) {
        std::fclose(file);
        return nullptr;
    }
    const long fileSize = std::ftell(file);
    if (fileSize <= 0) {
        std::fclose(file);
        return nullptr;
    }
    if (std::fseek(file, 0, SEEK_SET) != 0) {
        std::fclose(file);
        return nullptr;
    }

    std::string bytes(static_cast<size_t>(fileSize), '\0');
    const size_t readCount = std::fread(bytes.data(), 1, bytes.size(), file);
    std::fclose(file);
    if (readCount != bytes.size()) {
        return nullptr;
    }

    int width = 0;
    int height = 0;
    int components = 0;
    unsigned char* rgba = stbi_load_from_memory(
        reinterpret_cast<const stbi_uc*>(bytes.data()),
        static_cast<int>(bytes.size()),
        &width,
        &height,
        &components,
        4);
    if (!rgba) {
        return nullptr;
    }

    SDL_Surface* surface = surfaceFromRgba(rgba, width, height);
    stbi_image_free(rgba);
    return surface;
}

} // namespace

SDL_Surface* loadWallpaperSurface(const std::string& hostPath) {
    using monolith::app::hasCaseInsensitiveSuffix;
    using monolith::app::isWallpaperImagePath;

    if (hostPath.empty() || !isWallpaperImagePath(hostPath)) {
        return nullptr;
    }

    // Keep the original BMP path exact for existing samples and regressions.
    if (hasCaseInsensitiveSuffix(hostPath, ".bmp")) {
        return SDL_LoadBMP(hostPath.c_str());
    }

    return loadWithStb(hostPath);
}

} // namespace monolith::window
