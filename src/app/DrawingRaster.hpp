#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace monolith::drawing {

/** Set one RGBA pixel. No-op if (x,y) is outside the canvas. */
void setPixel(std::vector<uint8_t>& rgba, int width, int height,
              int x, int y, uint8_t r, uint8_t g, uint8_t b);

/** Bresenham line, 1px wide, including both endpoints. */
void drawLine(std::vector<uint8_t>& rgba, int width, int height,
              int x0, int y0, int x1, int y1, uint8_t r, uint8_t g, uint8_t b);

/** Axis-aligned rectangle boundary (inclusive), 1px wide. */
void drawRect(std::vector<uint8_t>& rgba, int width, int height,
              int x0, int y0, int x1, int y1, uint8_t r, uint8_t g, uint8_t b);

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
