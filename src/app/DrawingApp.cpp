#include "DrawingApp.hpp"
#include "FilePath.hpp"
#include "DrawingRaster.hpp"
#include "Utf8.hpp"
#include "../detail/RendererClip.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <sstream>
#include <string_view>
#include <utility>
#include <vector>

namespace monolith::app {

namespace {
constexpr int kToolbarPadding = 8;
constexpr int kToolbarButtonHeight = 22;
constexpr int kToolbarGap = 6;
constexpr int kStatusBarHeight = 22;
constexpr int kSwatchSize = 18;
constexpr int kSwatchGap = 4;
constexpr size_t kMaxHistoryStates = 32;
constexpr size_t kMaxHistoryBytes = 64 * 1024 * 1024;
constexpr uint8_t kCanvasBackgroundR = 245;
constexpr uint8_t kCanvasBackgroundG = 245;
constexpr uint8_t kCanvasBackgroundB = 248;

bool pointInRect(int x, int y, const SDL_Rect& rect) {
    return x >= rect.x && x < rect.x + rect.w && y >= rect.y && y < rect.y + rect.h;
}

} // namespace

using monolith::detail::RendererClipState;
using monolith::detail::captureRendererClip;
using monolith::detail::restoreRendererClip;
using monolith::detail::intersectRendererClip;

#include "DrawingApp_body_00.inc"
#include "DrawingApp_body_01.inc"
#include "DrawingApp_body_02.inc"
#include "DrawingApp_body_03.inc"
#include "DrawingApp_body_04.inc"
