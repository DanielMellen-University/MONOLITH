#include "DrawingRaster.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <cstdlib>
#include <limits>
#include <sstream>

namespace monolith::drawing {

namespace {
constexpr char kModrMagic[4] = {'M', 'O', 'D', 'R'};

bool hasValidCanvasBuffer(const std::vector<uint8_t>& rgba, int width, int height) {
    if (width <= 0 || height <= 0) return false;
    const std::size_t canvasWidth = static_cast<std::size_t>(width);
    const std::size_t canvasHeight = static_cast<std::size_t>(height);
    if (canvasWidth > std::numeric_limits<std::size_t>::max() / canvasHeight) return false;
    const std::size_t pixelCount = canvasWidth * canvasHeight;
    if (pixelCount > std::numeric_limits<std::size_t>::max() / 4) return false;
    return rgba.size() >= pixelCount * 4;
}

bool writePixelAt(std::vector<uint8_t>& rgba, int width,
                  int x, int y, uint8_t r, uint8_t g, uint8_t b) {
    const std::size_t index =
        (static_cast<std::size_t>(y) * static_cast<std::size_t>(width)
         + static_cast<std::size_t>(x)) * 4;
    if (rgba[index] == r && rgba[index + 1] == g && rgba[index + 2] == b
        && rgba[index + 3] == 255) {
        return false;
    }
    rgba[index] = r;
    rgba[index + 1] = g;
    rgba[index + 2] = b;
    rgba[index + 3] = 255;
    return true;
}

bool writePixelInBounds(std::vector<uint8_t>& rgba, int width, int height,
                        int x, int y, uint8_t r, uint8_t g, uint8_t b) {
    if (x < 0 || y < 0 || x >= width || y >= height) return false;
    return writePixelAt(rgba, width, x, y, r, g, b);
}

bool stampBrushInValidBuffer(std::vector<uint8_t>& rgba, int width, int height,
                             int centerX, int centerY, int radius,
                             uint8_t r, uint8_t g, uint8_t b) {
    if (radius < 0) return false;
    const long long brushRadius = radius;
    const long long left = std::max(0LL, static_cast<long long>(centerX) - brushRadius);
    const long long right = std::min(static_cast<long long>(width) - 1,
                                     static_cast<long long>(centerX) + brushRadius);
    const long long top = std::max(0LL, static_cast<long long>(centerY) - brushRadius);
    const long long bottom = std::min(static_cast<long long>(height) - 1,
                                      static_cast<long long>(centerY) + brushRadius);
    if (left > right || top > bottom) return false;

    const long long radiusSquared = brushRadius * brushRadius;
    bool changed = false;
    for (long long y = top; y <= bottom; ++y) {
        const long long dy = y - centerY;
        for (long long x = left; x <= right; ++x) {
            const long long dx = x - centerX;
            if (dx * dx + dy * dy > radiusSquared) continue;
            changed = writePixelAt(rgba, width, static_cast<int>(x),
                                   static_cast<int>(y), r, g, b) || changed;
        }
    }
    return changed;
}

template <typename PaintPoint>
bool traceBresenhamLine(int x0, int y0, int x1, int y1, PaintPoint&& paintPoint) {
    const long long dx = std::abs(static_cast<long long>(x1) - x0);
    const long long dy = std::abs(static_cast<long long>(y1) - y0);
    const int sx = x0 < x1 ? 1 : -1;
    const int sy = y0 < y1 ? 1 : -1;
    long long error = dx - dy;
    int x = x0;
    int y = y0;
    bool changed = false;

    while (true) {
        changed = paintPoint(x, y) || changed;
        if (x == x1 && y == y1) break;
        const long long twiceError = 2 * error;
        if (twiceError > -dy) {
            error -= dy;
            x += sx;
        }
        if (twiceError < dx) {
            error += dx;
            y += sy;
        }
    }
    return changed;
}

void writeU32LE(std::string& out, uint32_t value) {
    out.push_back(static_cast<char>(value & 0xFF));
    out.push_back(static_cast<char>((value >> 8) & 0xFF));
    out.push_back(static_cast<char>((value >> 16) & 0xFF));
    out.push_back(static_cast<char>((value >> 24) & 0xFF));
}

uint32_t readU32LE(const std::string& data, size_t offset) {
    if (offset + 4 > data.size()) return 0;
    const auto* bytes = reinterpret_cast<const unsigned char*>(data.data() + offset);
    return static_cast<uint32_t>(bytes[0])
         | (static_cast<uint32_t>(bytes[1]) << 8)
         | (static_cast<uint32_t>(bytes[2]) << 16)
         | (static_cast<uint32_t>(bytes[3]) << 24);
}
} // namespace

bool setPixel(std::vector<uint8_t>& rgba, int width, int height,
              int x, int y, uint8_t r, uint8_t g, uint8_t b) {
    if (!hasValidCanvasBuffer(rgba, width, height)) return false;
    return writePixelInBounds(rgba, width, height, x, y, r, g, b);
}

bool getPixel(const std::vector<uint8_t>& rgba, int width, int height,
              int x, int y, uint8_t& r, uint8_t& g, uint8_t& b) {
    if (!hasValidCanvasBuffer(rgba, width, height)) return false;
    if (x < 0 || y < 0 || x >= width || y >= height) return false;
    const size_t idx = (static_cast<size_t>(y) * static_cast<size_t>(width)
                        + static_cast<size_t>(x)) * 4;
    r = rgba[idx + 0];
    g = rgba[idx + 1];
    b = rgba[idx + 2];
    return true;
}

bool drawLine(std::vector<uint8_t>& rgba, int width, int height,
              int x0, int y0, int x1, int y1, uint8_t r, uint8_t g, uint8_t b) {
    if (!hasValidCanvasBuffer(rgba, width, height)) return false;
    return traceBresenhamLine(x0, y0, x1, y1, [&](int x, int y) {
        return writePixelInBounds(rgba, width, height, x, y, r, g, b);
    });
}

bool drawRect(std::vector<uint8_t>& rgba, int width, int height,
              int x0, int y0, int x1, int y1, uint8_t r, uint8_t g, uint8_t b) {
    if (!hasValidCanvasBuffer(rgba, width, height)) return false;
    bool changed = false;
    if (x0 > x1) std::swap(x0, x1);
    if (y0 > y1) std::swap(y0, y1);
    const long long left = std::max(0LL, static_cast<long long>(x0));
    const long long right = std::min(static_cast<long long>(width) - 1,
                                     static_cast<long long>(x1));
    if (left <= right) {
        if (y0 >= 0 && y0 < height) {
            for (long long x = left; x <= right; ++x) {
                changed = writePixelAt(rgba, width, static_cast<int>(x),
                                       y0, r, g, b) || changed;
            }
        }
        if (y1 >= 0 && y1 < height) {
            for (long long x = left; x <= right; ++x) {
                changed = writePixelAt(rgba, width, static_cast<int>(x),
                                       y1, r, g, b) || changed;
            }
        }
    }
    const long long top = std::max(0LL, static_cast<long long>(y0));
    const long long bottom = std::min(static_cast<long long>(height) - 1,
                                      static_cast<long long>(y1));
    if (top <= bottom) {
        if (x0 >= 0 && x0 < width) {
            for (long long y = top; y <= bottom; ++y) {
                changed = writePixelAt(rgba, width, x0, static_cast<int>(y),
                                       r, g, b) || changed;
            }
        }
        if (x1 >= 0 && x1 < width) {
            for (long long y = top; y <= bottom; ++y) {
                changed = writePixelAt(rgba, width, x1, static_cast<int>(y),
                                       r, g, b) || changed;
            }
        }
    }
    return changed;
}

bool stampBrush(std::vector<uint8_t>& rgba, int width, int height,
                int centerX, int centerY, int radius,
                uint8_t r, uint8_t g, uint8_t b) {
    if (!hasValidCanvasBuffer(rgba, width, height)) return false;
    return stampBrushInValidBuffer(
        rgba, width, height, centerX, centerY, radius, r, g, b);
}

bool drawBrushStroke(std::vector<uint8_t>& rgba, int width, int height,
                     int x0, int y0, int x1, int y1, int radius,
                     uint8_t r, uint8_t g, uint8_t b) {
    if (!hasValidCanvasBuffer(rgba, width, height) || radius < 0) return false;
    return traceBresenhamLine(x0, y0, x1, y1, [&](int x, int y) {
        return stampBrushInValidBuffer(rgba, width, height, x, y, radius, r, g, b);
    });
}

std::size_t fillRegion(std::vector<uint8_t>& rgba, int width, int height,
                       int x, int y, uint8_t r, uint8_t g, uint8_t b) {
    if (!hasValidCanvasBuffer(rgba, width, height)
        || x < 0 || y < 0 || x >= width || y >= height) return 0;
    const std::size_t canvasWidth = static_cast<std::size_t>(width);

    const std::size_t startIndex =
        (static_cast<std::size_t>(y) * canvasWidth + static_cast<std::size_t>(x)) * 4;
    const uint8_t targetR = rgba[startIndex];
    const uint8_t targetG = rgba[startIndex + 1];
    const uint8_t targetB = rgba[startIndex + 2];
    if (targetR == r && targetG == g && targetB == b) return 0;

    struct Span {
        int y;
        int left;
        int right;
    };
    std::vector<Span> pending;
    std::size_t filledPixels = 0;

    auto isTarget = [&](int px, int py) {
        const std::size_t index =
            (static_cast<std::size_t>(py) * canvasWidth
             + static_cast<std::size_t>(px)) * 4;
        return rgba[index] == targetR
            && rgba[index + 1] == targetG
            && rgba[index + 2] == targetB;
    };
    auto paintSpan = [&](int seedX, int row) {
        int left = seedX;
        while (left > 0 && isTarget(left - 1, row)) --left;

        int right = seedX;
        while (right + 1 < width && isTarget(right + 1, row)) ++right;

        for (int px = left; px <= right; ++px) {
            const std::size_t index =
                (static_cast<std::size_t>(row) * canvasWidth
                 + static_cast<std::size_t>(px)) * 4;
            rgba[index] = r;
            rgba[index + 1] = g;
            rgba[index + 2] = b;
            rgba[index + 3] = 255;
        }
        filledPixels += static_cast<std::size_t>(right - left + 1);
        return Span{row, left, right};
    };

    pending.push_back(paintSpan(x, y));
    while (!pending.empty()) {
        const Span span = pending.back();
        pending.pop_back();

        auto addAdjacentSpans = [&](int row) {
            if (row < 0 || row >= height) return;
            int px = span.left;
            while (px <= span.right) {
                if (!isTarget(px, row)) {
                    ++px;
                    continue;
                }
                const Span adjacent = paintSpan(px, row);
                pending.push_back(adjacent);
                px = adjacent.right + 1;
            }
        };
        addAdjacentSpans(span.y - 1);
        addAdjacentSpans(span.y + 1);
    }

    return filledPixels;
}

std::string encodeModr(int width, int height, const std::vector<uint8_t>& rgba) {
    if (width <= 0 || height <= 0) return {};
    const size_t expected = static_cast<size_t>(width) * static_cast<size_t>(height) * 4;
    if (rgba.size() < expected) return {};

    std::string blob;
    blob.reserve(12 + static_cast<size_t>(width) * static_cast<size_t>(height) * 3);
    blob.append(kModrMagic, 4);
    writeU32LE(blob, static_cast<uint32_t>(width));
    writeU32LE(blob, static_cast<uint32_t>(height));
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            const size_t idx = (static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 4;
            blob.push_back(static_cast<char>(rgba[idx + 0]));
            blob.push_back(static_cast<char>(rgba[idx + 1]));
            blob.push_back(static_cast<char>(rgba[idx + 2]));
        }
    }
    return blob;
}

bool decodeModr(const std::string& blob, int& width, int& height, std::vector<uint8_t>& rgba) {
    if (blob.size() < 12 || std::memcmp(blob.data(), kModrMagic, 4) != 0) return false;
    const int w = static_cast<int>(readU32LE(blob, 4));
    const int h = static_cast<int>(readU32LE(blob, 8));
    if (w <= 0 || h <= 0 || w > 4096 || h > 4096) return false;
    const size_t expected = static_cast<size_t>(w) * static_cast<size_t>(h) * 3;
    if (blob.size() != 12 + expected) return false;

    rgba.assign(static_cast<size_t>(w) * static_cast<size_t>(h) * 4, 255);
    size_t offset = 12;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const size_t idx = (static_cast<size_t>(y) * static_cast<size_t>(w) + static_cast<size_t>(x)) * 4;
            rgba[idx + 0] = static_cast<uint8_t>(blob[offset++]);
            rgba[idx + 1] = static_cast<uint8_t>(blob[offset++]);
            rgba[idx + 2] = static_cast<uint8_t>(blob[offset++]);
            rgba[idx + 3] = 255;
        }
    }
    width = w;
    height = h;
    return true;
}

bool parseRgb(const std::string& text, uint8_t& r, uint8_t& g, uint8_t& b) {
    std::string normalized;
    normalized.reserve(text.size());
    for (char c : text) {
        if (c == ',') {
            normalized.push_back(' ');
        } else if (!std::isspace(static_cast<unsigned char>(c))) {
            normalized.push_back(c);
        } else if (!normalized.empty() && normalized.back() != ' ') {
            normalized.push_back(' ');
        }
    }
    while (!normalized.empty() && normalized.back() == ' ') normalized.pop_back();

    std::istringstream iss(normalized);
    int ir = -1, ig = -1, ib = -1;
    if (!(iss >> ir >> ig >> ib)) return false;
    std::string extra;
    if (iss >> extra) return false;
    if (ir < 0 || ir > 255 || ig < 0 || ig > 255 || ib < 0 || ib > 255) return false;
    r = static_cast<uint8_t>(ir);
    g = static_cast<uint8_t>(ig);
    b = static_cast<uint8_t>(ib);
    return true;
}

} // namespace monolith::drawing
