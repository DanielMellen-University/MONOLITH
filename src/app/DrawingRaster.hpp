#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace monolith::drawing {

/** Inclusive max width/height for live canvases and .modr files. */
constexpr int kMaxCanvasDimension = 4096;

/** Largest on-disk .modr size: 12-byte header plus 4096×4096 RGB payload. */
constexpr std::size_t maxModrFileBytes() {
    return 12u
        + static_cast<std::size_t>(kMaxCanvasDimension)
            * static_cast<std::size_t>(kMaxCanvasDimension) * 3u;
}

/** Called before each actual pixel change, allowing sparse history capture. */
using PixelWriteObserver = void (*)(void* context, int x, int y);
/** Called once before the pixels in a changed fill span are written. */
using PixelSpanWriteObserver = void (*)(void* context, int y, int left, int right);

/** Set one RGBA pixel. Returns false if the pixel was already that color. */
bool setPixel(std::vector<uint8_t>& rgba, int width, int height,
              int x, int y, uint8_t r, uint8_t g, uint8_t b,
              PixelWriteObserver observer = nullptr, void* observerContext = nullptr);

/** Read one RGB pixel. Returns false if (x,y) or the RGBA buffer is invalid. */
bool getPixel(const std::vector<uint8_t>& rgba, int width, int height,
              int x, int y, uint8_t& r, uint8_t& g, uint8_t& b);

/** Bresenham line, 1px wide, including both endpoints. */
bool drawLine(std::vector<uint8_t>& rgba, int width, int height,
              int x0, int y0, int x1, int y1, uint8_t r, uint8_t g, uint8_t b,
              PixelWriteObserver observer = nullptr, void* observerContext = nullptr);

/** Axis-aligned rectangle boundary (inclusive), 1px wide. */
bool drawRect(std::vector<uint8_t>& rgba, int width, int height,
              int x0, int y0, int x1, int y1, uint8_t r, uint8_t g, uint8_t b,
              PixelWriteObserver observer = nullptr, void* observerContext = nullptr);

/** Stamp a clipped, filled circular brush footprint. */
bool stampBrush(std::vector<uint8_t>& rgba, int width, int height,
                int centerX, int centerY, int radius,
                uint8_t r, uint8_t g, uint8_t b,
                PixelWriteObserver observer = nullptr, void* observerContext = nullptr);

/** Draw a Bresenham path using filled circular brush stamps. */
bool drawBrushStroke(std::vector<uint8_t>& rgba, int width, int height,
                     int x0, int y0, int x1, int y1, int radius,
                     uint8_t r, uint8_t g, uint8_t b,
                     PixelWriteObserver observer = nullptr, void* observerContext = nullptr);

/** Fill a 4-connected RGB region using a scanline worklist; return pixels changed. */
std::size_t fillRegion(std::vector<uint8_t>& rgba, int width, int height,
                       int x, int y, uint8_t r, uint8_t g, uint8_t b,
                       PixelSpanWriteObserver observer = nullptr,
                       void* observerContext = nullptr);

/** Encode live RGBA canvas as .modr (magic + w/h + RGB payload). */
std::string encodeModr(int width, int height, const std::vector<uint8_t>& rgba);

/** Decode .modr into an RGBA buffer (A=255). Returns false on corrupt data. */
bool decodeModr(const std::string& blob, int& width, int& height, std::vector<uint8_t>& rgba);

/**
 * Parse "R,G,B" or "R G B" with each channel 0–255.
 * Returns false if the text is not exactly three integers in range.
 */
bool parseRgb(const std::string& text, uint8_t& r, uint8_t& g, uint8_t& b);

} // namespace monolith::drawing
