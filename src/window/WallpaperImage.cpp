#include "WallpaperImage.hpp"

#include "../app/FilePath.hpp"

#include <array>
#include <cstdio>
#include <cstdint>
#include <cstring>

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#include "stb_image.h"

namespace monolith::window {
namespace {

constexpr std::uint64_t kMaxWallpaperPixels = 16'777'216;

bool wallpaperDimensionsAllowed(std::uint64_t width, std::uint64_t height) {
    return width > 0 && height > 0
        && width <= kMaxWallpaperPixels
        && height <= kMaxWallpaperPixels
        && width * height <= kMaxWallpaperPixels;
}

bool wallpaperDimensionsAllowed(int width, int height) {
    return width > 0 && height > 0
        && wallpaperDimensionsAllowed(
            static_cast<std::uint64_t>(width), static_cast<std::uint64_t>(height));
}

std::uint16_t littleEndian16(const unsigned char* bytes) {
    return static_cast<std::uint16_t>(bytes[0])
        | static_cast<std::uint16_t>(static_cast<std::uint16_t>(bytes[1]) << 8);
}

std::uint32_t littleEndian32(const unsigned char* bytes) {
    return static_cast<std::uint32_t>(bytes[0])
        | (static_cast<std::uint32_t>(bytes[1]) << 8)
        | (static_cast<std::uint32_t>(bytes[2]) << 16)
        | (static_cast<std::uint32_t>(bytes[3]) << 24);
}

std::int64_t signedLittleEndian32(const unsigned char* bytes) {
    const std::uint32_t bits = littleEndian32(bytes);
    return bits <= 0x7fffffffU
        ? static_cast<std::int64_t>(bits)
        : static_cast<std::int64_t>(bits) - 0x1'0000'0000LL;
}

bool bmpDimensionsAllowed(FILE* file) {
    std::array<unsigned char, 26> header{};
    if (std::fread(header.data(), 1, header.size(), file) != header.size()
        || header[0] != 'B' || header[1] != 'M') {
        return false;
    }

    const std::uint32_t dibHeaderSize = littleEndian32(header.data() + 14);
    if (dibHeaderSize == 12) {
        const std::uint64_t width = littleEndian16(header.data() + 18);
        const std::uint64_t height = littleEndian16(header.data() + 20);
        return wallpaperDimensionsAllowed(width, height);
    }
    if (dibHeaderSize < 40) return false;

    const std::int64_t width = signedLittleEndian32(header.data() + 18);
    const std::int64_t signedHeight = signedLittleEndian32(header.data() + 22);
    if (width <= 0 || signedHeight == 0) return false;
    const std::uint64_t height = static_cast<std::uint64_t>(
        signedHeight < 0 ? -signedHeight : signedHeight);
    return wallpaperDimensionsAllowed(static_cast<std::uint64_t>(width), height);
}

SDL_Surface* loadBmpWithLimit(const std::string& hostPath) {
    FILE* file = std::fopen(hostPath.c_str(), "rb");
    if (!file) return nullptr;

    if (!bmpDimensionsAllowed(file) || std::fseek(file, 0, SEEK_SET) != 0) {
        std::fclose(file);
        return nullptr;
    }

    SDL_RWops* rw = SDL_RWFromFP(file, SDL_TRUE);
    if (!rw) {
        std::fclose(file);
        return nullptr;
    }
    return SDL_LoadBMP_RW(rw, 1);
}

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

    int width = 0;
    int height = 0;
    int components = 0;
    if (!stbi_info_from_file(file, &width, &height, &components)
        || !wallpaperDimensionsAllowed(width, height)) {
        std::fclose(file);
        return nullptr;
    }

    unsigned char* rgba = stbi_load_from_file(
        file, &width, &height, &components, 4);
    std::fclose(file);
    if (!rgba) {
        return nullptr;
    }
    if (!wallpaperDimensionsAllowed(width, height)) {
        stbi_image_free(rgba);
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

    if (hasCaseInsensitiveSuffix(hostPath, ".bmp")) {
        return loadBmpWithLimit(hostPath);
    }

    return loadWithStb(hostPath);
}

} // namespace monolith::window
