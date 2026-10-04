#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <string>
#include <string_view>
#include <vector>

namespace monolith::drawing {

inline constexpr int kMaxModrDimension = 4096;
inline constexpr std::size_t kModrHeaderBytes = 12;
inline constexpr std::size_t kModrRgbBytesPerPixel = 3;
inline constexpr std::size_t kMaxModrEncodedBytes =
    kModrHeaderBytes
    + static_cast<std::size_t>(kMaxModrDimension)
        * static_cast<std::size_t>(kMaxModrDimension)
        * kModrRgbBytesPerPixel;

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

/** Encode live RGBA canvas as .modr; reject unsupported dimensions or buffer sizes. */
std::string encodeModr(int width, int height, const std::vector<uint8_t>& rgba);

/** Stream live RGBA canvas as .modr using a bounded temporary buffer. */
bool writeModr(std::ostream& output, int width, int height,
               const std::vector<uint8_t>& rgba);

/** Compare an encoded byte range with the exact .modr stream for a canvas. */
bool matchesModrChunk(int width, int height, const std::vector<uint8_t>& rgba,
                      std::size_t encodedOffset, std::string_view encodedChunk);

/** Incrementally validates and decodes a .modr file without buffering its RGB payload. */
class ModrStreamDecoder {
public:
    explicit ModrStreamDecoder(std::uint64_t fileBytes);

    /** Consume one input chunk. Returns false as soon as the stream is invalid. */
    bool consume(std::string_view chunk);

    /** Commit decoded outputs only after the complete file has been validated. */
    bool finish(int& width, int& height, std::vector<uint8_t>& rgba);

private:
    bool parseHeader();
    bool appendPayload(std::string_view bytes);

    std::uint64_t m_fileBytes = 0;
    std::array<char, kModrHeaderBytes> m_header{};
    std::array<uint8_t, kModrRgbBytesPerPixel> m_partialPixel{};
    std::size_t m_headerBytes = 0;
    std::size_t m_expectedPayloadBytes = 0;
    std::size_t m_writtenPixels = 0;
    std::size_t m_partialPixelBytes = 0;
    int m_width = 0;
    int m_height = 0;
    std::vector<uint8_t> m_rgba;
    bool m_headerParsed = false;
    bool m_failed = false;
    bool m_finished = false;
};

/** Decode .modr into an RGBA buffer (A=255). Returns false on corrupt data. */
bool decodeModr(const std::string& blob, int& width, int& height, std::vector<uint8_t>& rgba);

/**
 * Parse "R,G,B" or "R G B" with each channel 0–255.
 * Returns false if the text is not exactly three integers in range.
 */
bool parseRgb(const std::string& text, uint8_t& r, uint8_t& g, uint8_t& b);

} // namespace monolith::drawing
