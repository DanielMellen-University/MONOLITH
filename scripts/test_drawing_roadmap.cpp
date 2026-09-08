// Headless test of shipped Drawing raster (line/rect/custom RGB) and .modr round-trip.
// Compiles against src/app/DrawingRaster.cpp (no SDL).

#include "../src/app/DrawingRaster.hpp"

#include <cstdint>
#include <iostream>
#include <vector>

using monolith::drawing::decodeModr;
using monolith::drawing::drawLine;
using monolith::drawing::drawRect;
using monolith::drawing::encodeModr;
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
