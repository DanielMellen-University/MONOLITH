#include "../src/window/WallpaperImage.hpp"
#include "TestTempDir.hpp"

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

void appendBigEndian32(std::vector<std::uint8_t>& output, std::uint32_t value) {
    output.push_back(static_cast<std::uint8_t>(value >> 24));
    output.push_back(static_cast<std::uint8_t>(value >> 16));
    output.push_back(static_cast<std::uint8_t>(value >> 8));
    output.push_back(static_cast<std::uint8_t>(value));
}

void appendLittleEndian16(std::vector<std::uint8_t>& output, std::uint16_t value) {
    output.push_back(static_cast<std::uint8_t>(value));
    output.push_back(static_cast<std::uint8_t>(value >> 8));
}

void appendLittleEndian32(std::vector<std::uint8_t>& output, std::uint32_t value) {
    output.push_back(static_cast<std::uint8_t>(value));
    output.push_back(static_cast<std::uint8_t>(value >> 8));
    output.push_back(static_cast<std::uint8_t>(value >> 16));
    output.push_back(static_cast<std::uint8_t>(value >> 24));
}

std::vector<std::uint8_t> bmpImage(std::uint32_t width, std::uint32_t height) {
    std::vector<std::uint8_t> bmp{'B', 'M'};
    appendLittleEndian32(bmp, 58);
    appendLittleEndian16(bmp, 0);
    appendLittleEndian16(bmp, 0);
    appendLittleEndian32(bmp, 54);
    appendLittleEndian32(bmp, 40);
    appendLittleEndian32(bmp, width);
    appendLittleEndian32(bmp, height);
    appendLittleEndian16(bmp, 1);
    appendLittleEndian16(bmp, 24);
    appendLittleEndian32(bmp, 0);
    appendLittleEndian32(bmp, 4);
    appendLittleEndian32(bmp, 0);
    appendLittleEndian32(bmp, 0);
    appendLittleEndian32(bmp, 0);
    appendLittleEndian32(bmp, 0);
    bmp.insert(bmp.end(), {56, 34, 12, 0});
    return bmp;
}

std::uint32_t crc32(const std::uint8_t* bytes, std::size_t length) {
    std::uint32_t crc = 0xffffffffu;
    for (std::size_t i = 0; i < length; ++i) {
        crc ^= bytes[i];
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
        }
    }
    return ~crc;
}

void appendPngChunk(std::vector<std::uint8_t>& output,
                    const char (&type)[5],
                    const std::vector<std::uint8_t>& data) {
    appendBigEndian32(output, static_cast<std::uint32_t>(data.size()));
    const std::size_t checksumStart = output.size();
    for (int i = 0; i < 4; ++i) {
        output.push_back(static_cast<std::uint8_t>(type[i]));
    }
    output.insert(output.end(), data.begin(), data.end());
    appendBigEndian32(output, crc32(
        output.data() + checksumStart, output.size() - checksumStart));
}

std::vector<std::uint8_t> pngImage(
    std::uint32_t width,
    std::uint32_t height,
    const std::vector<std::uint8_t>& compressedPixels) {
    std::vector<std::uint8_t> png{
        0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a};
    std::vector<std::uint8_t> ihdr;
    appendBigEndian32(ihdr, width);
    appendBigEndian32(ihdr, height);
    ihdr.insert(ihdr.end(), {8, 6, 0, 0, 0});
    appendPngChunk(png, "IHDR", ihdr);
    appendPngChunk(png, "IDAT", compressedPixels);
    appendPngChunk(png, "IEND", {});
    return png;
}

std::vector<std::uint8_t> onePixelPng() {
    const std::vector<std::uint8_t> scanline{0, 12, 34, 56, 255};
    std::vector<std::uint8_t> compressedPixels{
        0x78, 0x01, 0x01, 0x05, 0x00, 0xfa, 0xff};
    compressedPixels.insert(
        compressedPixels.end(), scanline.begin(), scanline.end());
    std::uint32_t a = 1;
    std::uint32_t b = 0;
    for (std::uint8_t byte : scanline) {
        a = (a + byte) % 65521;
        b = (b + a) % 65521;
    }
    appendBigEndian32(compressedPixels, (b << 16) | a);
    return pngImage(1, 1, compressedPixels);
}

bool writeBytes(const std::filesystem::path& path,
                const std::uint8_t* bytes,
                std::size_t length) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write(reinterpret_cast<const char*>(bytes),
                 static_cast<std::streamsize>(length));
    output.close();
    return output.good();
}

bool writeBytes(const std::filesystem::path& path,
                const std::vector<std::uint8_t>& bytes) {
    return writeBytes(path, bytes.data(), bytes.size());
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

    monolith::test::ScopedTempDirectory temp("monolith-wallpaper-image");
    if (!temp) {
        std::cerr << "FAIL: could not create wallpaper test directory\n";
        return 1;
    }
    const std::filesystem::path root = temp.path();

    const auto validPng = onePixelPng();
    const auto validPath = root / "one.png";
    check(writeBytes(validPath, validPng),
          "write valid one-pixel PNG fixture");
    SDL_Surface* surface = monolith::window::loadWallpaperSurface(validPath.string());
    bool pixelMatches = false;
    if (surface && surface->w == 1 && surface->h == 1) {
        Uint32 packedPixel = 0;
        std::memcpy(&packedPixel, surface->pixels, sizeof(packedPixel));
        Uint8 red = 0, green = 0, blue = 0, alpha = 0;
        SDL_GetRGBA(packedPixel, surface->format, &red, &green, &blue, &alpha);
        pixelMatches = red == 12 && green == 34 && blue == 56 && alpha == 255;
    }
    check(pixelMatches, "PNG decodes directly from its file with exact RGBA pixels");
    if (surface) SDL_FreeSurface(surface);

    const auto validBmpPath = root / "one.bmp";
    const auto validBmp = bmpImage(1, 1);
    check(writeBytes(validBmpPath, validBmp), "write valid one-pixel BMP fixture");
    surface = monolith::window::loadWallpaperSurface(validBmpPath.string());
    pixelMatches = false;
    if (surface && surface->w == 1 && surface->h == 1) {
        Uint32 packedPixel = 0;
        std::memcpy(&packedPixel, surface->pixels, sizeof(packedPixel));
        Uint8 red = 0, green = 0, blue = 0, alpha = 0;
        SDL_GetRGBA(packedPixel, surface->format, &red, &green, &blue, &alpha);
        pixelMatches = red == 12 && green == 34 && blue == 56 && alpha == 255;
    }
    check(pixelMatches, "BMP retains SDL decoding for accepted dimensions");
    if (surface) SDL_FreeSurface(surface);

    const auto oversizedBmpPath = root / "oversized.bmp";
    const auto oversizedBmp = bmpImage(5000, 4000);
    check(writeBytes(oversizedBmpPath, oversizedBmp),
          "write over-limit BMP header fixture");
    surface = monolith::window::loadWallpaperSurface(oversizedBmpPath.string());
    check(surface == nullptr, "BMP above the wallpaper pixel cap is rejected");
    if (surface) SDL_FreeSurface(surface);

    const auto oversizedPath = root / "oversized.png";
    const auto oversizedPng = pngImage(5000, 4000, {});
    check(writeBytes(oversizedPath, oversizedPng),
          "write over-limit PNG header fixture");
    surface = monolith::window::loadWallpaperSurface(oversizedPath.string());
    check(surface == nullptr, "PNG above the wallpaper pixel cap is rejected");
    if (surface) SDL_FreeSurface(surface);

    const auto wrongExtensionPath = root / "one.txt";
    check(writeBytes(wrongExtensionPath, validPng),
          "write unsupported-extension image fixture");
    surface = monolith::window::loadWallpaperSurface(wrongExtensionPath.string());
    check(surface == nullptr, "non-wallpaper extensions remain rejected");
    if (surface) SDL_FreeSurface(surface);

    return failures == 0 ? 0 : 1;
}
