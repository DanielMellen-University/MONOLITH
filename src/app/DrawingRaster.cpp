#include "DrawingRaster.hpp"

#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <cstdlib>
#include <limits>
#include <ostream>
#include <sstream>

namespace monolith::drawing {

namespace {
constexpr char kModrMagic[4] = {'M', 'O', 'D', 'R'};
constexpr std::size_t kModrWriteChunkBytes = 16 * 1024;

std::array<char, kModrHeaderBytes> makeModrHeader(int width, int height) {
    std::array<char, kModrHeaderBytes> header{};
    std::copy(std::begin(kModrMagic), std::end(kModrMagic), header.begin());
    const auto writeU32LE = [&header](std::size_t offset, uint32_t value) {
        for (std::size_t byte = 0; byte < 4; ++byte) {
            header[offset + byte] = static_cast<char>((value >> (byte * 8)) & 0xFF);
        }
    };
    writeU32LE(4, static_cast<uint32_t>(width));
    writeU32LE(8, static_cast<uint32_t>(height));
    return header;
}

bool hasValidModrCanvas(int width, int height, const std::vector<uint8_t>& rgba) {
    if (width <= 0 || height <= 0
        || width > kMaxModrDimension || height > kMaxModrDimension) {
        return false;
    }
    const std::size_t pixelCount = static_cast<std::size_t>(width)
        * static_cast<std::size_t>(height);
    return rgba.size() == pixelCount * 4;
}

template <typename WriteChunk>
bool emitModr(int width, int height, const std::vector<uint8_t>& rgba,
              WriteChunk&& writeChunk) {
    if (!hasValidModrCanvas(width, height, rgba)) return false;

    const auto header = makeModrHeader(width, height);
    if (!writeChunk(header.data(), header.size())) return false;

    std::array<char, kModrWriteChunkBytes> chunk{};
    std::size_t buffered = 0;
    const std::size_t pixelCount = static_cast<std::size_t>(width)
        * static_cast<std::size_t>(height);
    for (std::size_t pixel = 0; pixel < pixelCount; ++pixel) {
        if (chunk.size() - buffered < kModrRgbBytesPerPixel) {
            if (!writeChunk(chunk.data(), buffered)) return false;
            buffered = 0;
        }
        const std::size_t source = pixel * 4;
        chunk[buffered++] = static_cast<char>(rgba[source]);
        chunk[buffered++] = static_cast<char>(rgba[source + 1]);
        chunk[buffered++] = static_cast<char>(rgba[source + 2]);
    }
    return buffered == 0 || writeChunk(chunk.data(), buffered);
}

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
                  int x, int y, uint8_t r, uint8_t g, uint8_t b,
                  PixelWriteObserver observer, void* observerContext) {
    const std::size_t index =
        (static_cast<std::size_t>(y) * static_cast<std::size_t>(width)
         + static_cast<std::size_t>(x)) * 4;
    if (rgba[index] == r && rgba[index + 1] == g && rgba[index + 2] == b
        && rgba[index + 3] == 255) {
        return false;
    }
    if (observer) observer(observerContext, x, y);
    rgba[index] = r;
    rgba[index + 1] = g;
    rgba[index + 2] = b;
    rgba[index + 3] = 255;
    return true;
}

bool writePixelInBounds(std::vector<uint8_t>& rgba, int width, int height,
                        int x, int y, uint8_t r, uint8_t g, uint8_t b,
                        PixelWriteObserver observer, void* observerContext) {
    if (x < 0 || y < 0 || x >= width || y >= height) return false;
    return writePixelAt(rgba, width, x, y, r, g, b, observer, observerContext);
}

bool stampBrushInValidBuffer(std::vector<uint8_t>& rgba, int width, int height,
                             int centerX, int centerY, int radius,
                             uint8_t r, uint8_t g, uint8_t b,
                             PixelWriteObserver observer, void* observerContext) {
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
                                   static_cast<int>(y), r, g, b,
                                   observer, observerContext) || changed;
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

uint32_t readU32LE(const char* data) {
    const auto* bytes = reinterpret_cast<const unsigned char*>(data);
    return static_cast<uint32_t>(bytes[0])
         | (static_cast<uint32_t>(bytes[1]) << 8)
         | (static_cast<uint32_t>(bytes[2]) << 16)
         | (static_cast<uint32_t>(bytes[3]) << 24);
}
} // namespace

bool setPixel(std::vector<uint8_t>& rgba, int width, int height,
              int x, int y, uint8_t r, uint8_t g, uint8_t b,
              PixelWriteObserver observer, void* observerContext) {
    if (!hasValidCanvasBuffer(rgba, width, height)) return false;
    return writePixelInBounds(rgba, width, height, x, y, r, g, b,
                              observer, observerContext);
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
              int x0, int y0, int x1, int y1, uint8_t r, uint8_t g, uint8_t b,
              PixelWriteObserver observer, void* observerContext) {
    if (!hasValidCanvasBuffer(rgba, width, height)) return false;
    return traceBresenhamLine(x0, y0, x1, y1, [&](int x, int y) {
        return writePixelInBounds(rgba, width, height, x, y, r, g, b,
                                  observer, observerContext);
    });
}

bool drawRect(std::vector<uint8_t>& rgba, int width, int height,
              int x0, int y0, int x1, int y1, uint8_t r, uint8_t g, uint8_t b,
              PixelWriteObserver observer, void* observerContext) {
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
                                       y0, r, g, b, observer, observerContext) || changed;
            }
        }
        if (y1 >= 0 && y1 < height) {
            for (long long x = left; x <= right; ++x) {
                changed = writePixelAt(rgba, width, static_cast<int>(x),
                                       y1, r, g, b, observer, observerContext) || changed;
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
                                       r, g, b, observer, observerContext) || changed;
            }
        }
        if (x1 >= 0 && x1 < width) {
            for (long long y = top; y <= bottom; ++y) {
                changed = writePixelAt(rgba, width, x1, static_cast<int>(y),
                                       r, g, b, observer, observerContext) || changed;
            }
        }
    }
    return changed;
}

bool stampBrush(std::vector<uint8_t>& rgba, int width, int height,
                int centerX, int centerY, int radius,
                uint8_t r, uint8_t g, uint8_t b,
                PixelWriteObserver observer, void* observerContext) {
    if (!hasValidCanvasBuffer(rgba, width, height)) return false;
    return stampBrushInValidBuffer(
        rgba, width, height, centerX, centerY, radius, r, g, b,
        observer, observerContext);
}

bool drawBrushStroke(std::vector<uint8_t>& rgba, int width, int height,
                     int x0, int y0, int x1, int y1, int radius,
                     uint8_t r, uint8_t g, uint8_t b,
                     PixelWriteObserver observer, void* observerContext) {
    if (!hasValidCanvasBuffer(rgba, width, height) || radius < 0) return false;
    return traceBresenhamLine(x0, y0, x1, y1, [&](int x, int y) {
        return stampBrushInValidBuffer(rgba, width, height, x, y, radius, r, g, b,
                                       observer, observerContext);
    });
}

std::size_t fillRegion(std::vector<uint8_t>& rgba, int width, int height,
                       int x, int y, uint8_t r, uint8_t g, uint8_t b,
                       PixelSpanWriteObserver observer, void* observerContext) {
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

        if (observer) observer(observerContext, row, left, right);
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
    if (!hasValidModrCanvas(width, height, rgba)) return {};

    std::string blob;
    const std::size_t pixelCount = static_cast<std::size_t>(width)
        * static_cast<std::size_t>(height);
    blob.reserve(kModrHeaderBytes + pixelCount * kModrRgbBytesPerPixel);
    emitModr(width, height, rgba, [&blob](const char* data, std::size_t size) {
        blob.append(data, size);
        return true;
    });
    return blob;
}

bool writeModr(std::ostream& output, int width, int height,
               const std::vector<uint8_t>& rgba) {
    return emitModr(width, height, rgba,
                    [&output](const char* data, std::size_t size) {
                        output.write(data, static_cast<std::streamsize>(size));
                        return static_cast<bool>(output);
                    });
}

bool matchesModrChunk(int width, int height, const std::vector<uint8_t>& rgba,
                      std::size_t encodedOffset, std::string_view encodedChunk) {
    if (!hasValidModrCanvas(width, height, rgba)) return false;

    const std::size_t pixelCount = static_cast<std::size_t>(width)
        * static_cast<std::size_t>(height);
    const std::size_t encodedBytes = kModrHeaderBytes
        + pixelCount * kModrRgbBytesPerPixel;
    if (encodedOffset > encodedBytes
        || encodedChunk.size() > encodedBytes - encodedOffset) {
        return false;
    }

    const auto header = makeModrHeader(width, height);
    for (std::size_t index = 0; index < encodedChunk.size(); ++index) {
        const std::size_t position = encodedOffset + index;
        uint8_t expected = 0;
        if (position < header.size()) {
            expected = static_cast<uint8_t>(header[position]);
        } else {
            const std::size_t payloadOffset = position - header.size();
            expected = rgba[(payloadOffset / kModrRgbBytesPerPixel) * 4
                            + payloadOffset % kModrRgbBytesPerPixel];
        }
        if (static_cast<uint8_t>(encodedChunk[index]) != expected) return false;
    }
    return true;
}

ModrStreamDecoder::ModrStreamDecoder(std::uint64_t fileBytes)
    : m_fileBytes(fileBytes),
      m_failed(fileBytes < kModrHeaderBytes
               || fileBytes > kMaxModrEncodedBytes) {}

bool ModrStreamDecoder::parseHeader() {
    if (std::memcmp(m_header.data(), kModrMagic, sizeof(kModrMagic)) != 0) return false;

    const uint32_t rawWidth = readU32LE(m_header.data() + 4);
    const uint32_t rawHeight = readU32LE(m_header.data() + 8);
    if (rawWidth == 0 || rawHeight == 0
        || rawWidth > static_cast<uint32_t>(kMaxModrDimension)
        || rawHeight > static_cast<uint32_t>(kMaxModrDimension)) {
        return false;
    }

    const std::size_t pixelCount = static_cast<std::size_t>(rawWidth)
        * static_cast<std::size_t>(rawHeight);
    m_expectedPayloadBytes = pixelCount * kModrRgbBytesPerPixel;
    if (m_fileBytes != kModrHeaderBytes + m_expectedPayloadBytes) return false;

    m_width = static_cast<int>(rawWidth);
    m_height = static_cast<int>(rawHeight);
    m_rgba.assign(pixelCount * 4, 255);
    m_headerParsed = true;
    return true;
}

bool ModrStreamDecoder::appendPayload(std::string_view bytes) {
    const std::size_t consumedBytes =
        m_writtenPixels * kModrRgbBytesPerPixel + m_partialPixelBytes;
    if (consumedBytes > m_expectedPayloadBytes
        || bytes.size() > m_expectedPayloadBytes - consumedBytes) {
        return false;
    }

    std::size_t offset = 0;
    const auto storePixel = [this](const uint8_t* rgb) {
        const std::size_t destination = m_writtenPixels * 4;
        std::copy(rgb, rgb + kModrRgbBytesPerPixel, m_rgba.begin() + destination);
        ++m_writtenPixels;
    };

    if (m_partialPixelBytes > 0) {
        const std::size_t count = std::min(
            kModrRgbBytesPerPixel - m_partialPixelBytes, bytes.size());
        for (std::size_t index = 0; index < count; ++index) {
            m_partialPixel[m_partialPixelBytes + index] = static_cast<uint8_t>(
                static_cast<unsigned char>(bytes[index]));
        }
        m_partialPixelBytes += count;
        offset += count;
        if (m_partialPixelBytes < kModrRgbBytesPerPixel) return true;
        storePixel(m_partialPixel.data());
        m_partialPixelBytes = 0;
    }

    while (bytes.size() - offset >= kModrRgbBytesPerPixel) {
        const std::size_t destination = m_writtenPixels * 4;
        for (std::size_t channel = 0; channel < kModrRgbBytesPerPixel; ++channel) {
            m_rgba[destination + channel] = static_cast<uint8_t>(
                static_cast<unsigned char>(bytes[offset + channel]));
        }
        ++m_writtenPixels;
        offset += kModrRgbBytesPerPixel;
    }

    m_partialPixelBytes = bytes.size() - offset;
    for (std::size_t index = 0; index < m_partialPixelBytes; ++index) {
        m_partialPixel[index] = static_cast<uint8_t>(
            static_cast<unsigned char>(bytes[offset + index]));
    }
    return true;
}

bool ModrStreamDecoder::consume(std::string_view chunk) {
    if (m_failed || m_finished) return false;

    std::size_t offset = 0;
    if (!m_headerParsed) {
        const std::size_t count = std::min(m_header.size() - m_headerBytes, chunk.size());
        if (count > 0) {
            std::memcpy(m_header.data() + m_headerBytes, chunk.data(), count);
            m_headerBytes += count;
            offset = count;
        }
        if (m_headerBytes < m_header.size()) return true;
        if (!parseHeader()) {
            m_failed = true;
            return false;
        }
    }

    if (!appendPayload(chunk.substr(offset))) {
        m_failed = true;
        return false;
    }
    return true;
}

bool ModrStreamDecoder::finish(int& width, int& height,
                               std::vector<uint8_t>& rgba) {
    if (m_failed || m_finished || !m_headerParsed || m_partialPixelBytes != 0
        || m_writtenPixels * kModrRgbBytesPerPixel != m_expectedPayloadBytes) {
        m_failed = true;
        return false;
    }

    width = m_width;
    height = m_height;
    rgba = std::move(m_rgba);
    m_finished = true;
    return true;
}

bool decodeModr(const std::string& blob, int& width, int& height, std::vector<uint8_t>& rgba) {
    ModrStreamDecoder decoder(blob.size());
    return decoder.consume(std::string_view(blob.data(), blob.size()))
        && decoder.finish(width, height, rgba);
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
