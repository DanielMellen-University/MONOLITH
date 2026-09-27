// Headless tests of Drawing raster tools, fill, custom RGB, and .modr round-trip.
// Compiles against src/app/DrawingRaster.cpp (no SDL).

#include "../src/app/DrawingRaster.hpp"

#include <cstdint>
#include <iostream>
#include <vector>

using monolith::drawing::decodeModr;
using monolith::drawing::drawLine;
using monolith::drawing::drawRect;
using monolith::drawing::encodeModr;
using monolith::drawing::fillRegion;
using monolith::drawing::getPixel;
using monolith::drawing::parseRgb;
using monolith::drawing::setPixel;

namespace {
void pixelRgb(const std::vector<uint8_t>& rgba, int width, int x, int y,
              uint8_t& r, uint8_t& g, uint8_t& b) {
    const size_t idx = (static_cast<size_t>(y) * static_cast<size_t>(width) + static_cast<size_t>(x)) * 4;
    r = rgba[idx + 0];
    g = rgba[idx + 1];
    b = rgba[idx + 2];
}

bool pixelIs(const std::vector<uint8_t>& rgba, int width, int x, int y,
             uint8_t r, uint8_t g, uint8_t b) {
    uint8_t pr = 0, pg = 0, pb = 0;
    pixelRgb(rgba, width, x, y, pr, pg, pb);
    return pr == r && pg == g && pb == b;
}
} // namespace

int main() {
    int failures = 0;
    auto check = [&](bool ok, const char* msg) {
        if (!ok) {
            std::cerr << "FAIL: " << msg << '\n';
            ++failures;
        } else {
            std::cout << "ok: " << msg << '\n';
        }
    };

    constexpr int kW = 16;
    constexpr int kH = 12;
    std::vector<uint8_t> canvas(static_cast<size_t>(kW) * static_cast<size_t>(kH) * 4, 0);

    uint8_t cr = 0, cg = 0, cb = 0;
    check(parseRgb("12,34,56", cr, cg, cb) && cr == 12 && cg == 34 && cb == 56,
          "parseRgb comma form");
    check(parseRgb(" 200 10 1 ", cr, cg, cb) && cr == 200 && cg == 10 && cb == 1,
          "parseRgb space form");
    check(!parseRgb("12,34", cr, cg, cb), "parseRgb rejects two channels");
    check(!parseRgb("12,34,999", cr, cg, cb), "parseRgb rejects out of range");

    const uint8_t lr = 220, lg = 70, lb = 70;
    check(parseRgb("220,70,70", cr, cg, cb) && cr == lr && cg == lg && cb == lb,
          "custom RGB selection 220,70,70");

    drawLine(canvas, kW, kH, 0, 0, 5, 0, lr, lg, lb);
    check(pixelIs(canvas, kW, 0, 0, lr, lg, lb), "line paints start endpoint");
    check(pixelIs(canvas, kW, 5, 0, lr, lg, lb), "line paints end endpoint");
    check(pixelIs(canvas, kW, 1, 0, lr, lg, lb)
              && pixelIs(canvas, kW, 2, 0, lr, lg, lb)
              && pixelIs(canvas, kW, 3, 0, lr, lg, lb)
              && pixelIs(canvas, kW, 4, 0, lr, lg, lb),
          "line paints intervening pixels");
    check(pixelIs(canvas, kW, 6, 0, 0, 0, 0), "line does not paint past endpoint");

    const uint8_t rr = 10, rg = 20, rb = 30;
    drawRect(canvas, kW, kH, 2, 2, 8, 6, rr, rg, rb);
    check(pixelIs(canvas, kW, 2, 2, rr, rg, rb) && pixelIs(canvas, kW, 8, 2, rr, rg, rb)
              && pixelIs(canvas, kW, 2, 6, rr, rg, rb) && pixelIs(canvas, kW, 8, 6, rr, rg, rb),
          "rect paints corners");
    check(pixelIs(canvas, kW, 5, 2, rr, rg, rb) && pixelIs(canvas, kW, 5, 6, rr, rg, rb)
              && pixelIs(canvas, kW, 2, 4, rr, rg, rb) && pixelIs(canvas, kW, 8, 4, rr, rg, rb),
          "rect paints boundary");
    check(pixelIs(canvas, kW, 4, 4, 0, 0, 0), "rect does not fill interior");

    uint8_t pickedR = 0, pickedG = 0, pickedB = 0;
    check(getPixel(canvas, kW, kH, 2, 2, pickedR, pickedG, pickedB)
              && pickedR == rr && pickedG == rg && pickedB == rb,
          "eyedropper reads a canvas pixel");
    check(!getPixel(canvas, kW, kH, -1, 0, pickedR, pickedG, pickedB),
          "eyedropper rejects an out-of-bounds pixel");

    constexpr int fillWidth = 7;
    constexpr int fillHeight = 5;
    std::vector<uint8_t> fillCanvas(
        static_cast<size_t>(fillWidth) * static_cast<size_t>(fillHeight) * 4);
    for (int y = 0; y < fillHeight; ++y) {
        for (int x = 0; x < fillWidth; ++x) {
            setPixel(fillCanvas, fillWidth, fillHeight, x, y, 10, 20, 30);
        }
    }
    for (int y = 0; y < fillHeight; ++y) {
        if (y != 2) setPixel(fillCanvas, fillWidth, fillHeight, 3, y, 200, 210, 220);
    }
    const size_t connectedFillCount =
        fillRegion(fillCanvas, fillWidth, fillHeight, 0, 0, 40, 50, 60);
    bool fillCrossedOpening = true;
    for (int y = 0; y < fillHeight; ++y) {
        for (int x = 0; x < fillWidth; ++x) {
            const bool barrier = x == 3 && y != 2;
            fillCrossedOpening &= barrier
                ? pixelIs(fillCanvas, fillWidth, x, y, 200, 210, 220)
                : pixelIs(fillCanvas, fillWidth, x, y, 40, 50, 60);
        }
    }
    check(connectedFillCount == static_cast<size_t>(fillWidth * fillHeight - (fillHeight - 1))
              && fillCrossedOpening,
          "scanline fill crosses a one-pixel opening and preserves barriers");
    check(fillRegion(fillCanvas, fillWidth, fillHeight, 0, 0, 40, 50, 60) == 0,
          "scanline fill leaves already-matching regions unchanged");

    constexpr int diagonalSize = 3;
    std::vector<uint8_t> diagonalCanvas(
        static_cast<size_t>(diagonalSize) * static_cast<size_t>(diagonalSize) * 4);
    for (int y = 0; y < diagonalSize; ++y) {
        for (int x = 0; x < diagonalSize; ++x) {
            setPixel(diagonalCanvas, diagonalSize, diagonalSize, x, y, 200, 210, 220);
        }
    }
    setPixel(diagonalCanvas, diagonalSize, diagonalSize, 0, 0, 10, 20, 30);
    setPixel(diagonalCanvas, diagonalSize, diagonalSize, 1, 1, 10, 20, 30);
    check(fillRegion(diagonalCanvas, diagonalSize, diagonalSize, 0, 0, 40, 50, 60) == 1
              && pixelIs(diagonalCanvas, diagonalSize, 0, 0, 40, 50, 60)
              && pixelIs(diagonalCanvas, diagonalSize, 1, 1, 10, 20, 30),
          "scanline fill retains four-connected rather than diagonal selection");

    constexpr int largeFillWidth = 1024;
    constexpr int largeFillHeight = 512;
    std::vector<uint8_t> largeFillCanvas(
        static_cast<size_t>(largeFillWidth) * static_cast<size_t>(largeFillHeight) * 4);
    for (int y = 0; y < largeFillHeight; ++y) {
        for (int x = 0; x < largeFillWidth; ++x) {
            setPixel(largeFillCanvas, largeFillWidth, largeFillHeight, x, y, 1, 2, 3);
        }
    }
    const size_t largeFillCount = fillRegion(
        largeFillCanvas, largeFillWidth, largeFillHeight, 0, 0, 7, 8, 9);
    check(largeFillCount == static_cast<size_t>(largeFillWidth) * largeFillHeight
              && pixelIs(largeFillCanvas, largeFillWidth,
                         largeFillWidth - 1, largeFillHeight - 1, 7, 8, 9),
          "scanline fill covers a large flat canvas without gaps");
    check(fillRegion(largeFillCanvas, largeFillWidth, largeFillHeight,
                     -1, 0, 10, 11, 12) == 0,
          "scanline fill rejects out-of-bounds seeds");

    const std::string blob = encodeModr(kW, kH, canvas);
    check(!blob.empty(), "encodeModr produced a blob");
    int dw = 0, dh = 0;
    std::vector<uint8_t> decoded;
    check(decodeModr(blob, dw, dh, decoded), "decodeModr of line/rect canvas");
    check(dw == kW && dh == kH, "decoded dimensions");
    check(pixelIs(decoded, dw, 0, 0, lr, lg, lb)
              && pixelIs(decoded, dw, 5, 0, lr, lg, lb)
              && pixelIs(decoded, dw, 2, 2, rr, rg, rb)
              && pixelIs(decoded, dw, 4, 4, 0, 0, 0),
          "round-trip keeps line, rect, and interior");

    if (failures == 0) {
        std::cout << "ALL DRAWING ROADMAP TESTS PASSED\n";
        return 0;
    }
    std::cerr << failures << " test(s) failed\n";
    return 1;
}
