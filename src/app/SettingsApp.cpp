#include "SettingsApp.hpp"
#include "FilePath.hpp"
#include "Utf8.hpp"
#include "../detail/RendererClip.hpp"

#include <algorithm>
#include <cctype>
#include <vector>

namespace monolith::app {

using monolith::detail::RendererClipState;
using monolith::detail::captureRendererClip;
using monolith::detail::restoreRendererClip;
using monolith::detail::intersectRendererClip;

#include "SettingsApp_body_00.inc"
#include "SettingsApp_body_01.inc"
#include "SettingsApp_body_02.inc"
#include "SettingsApp_body_03.inc"
#include "SettingsApp_body_04.inc"

int SettingsApp::getLineHeight() const {
    const int fontHeight = m_font ? TTF_FontHeight(m_font) : 16;
    return std::max(20, fontHeight);
}

int SettingsApp::getControlSize() const {
    return std::max(22, getLineHeight() + 8);
}

int SettingsApp::getFieldHeight() const {
    return std::max(24, getLineHeight() + 8);
}

int SettingsApp::getFooterHeight() const {
    return std::max(28, getLineHeight() + 8);
}

SDL_Rect SettingsApp::getFooterRect(const SDL_Rect& contentRect) const {
    const int clientHeight = std::max(0, contentRect.h);
    const int footerHeight = std::min(getFooterHeight(), clientHeight);
    return {
        contentRect.x,
        contentRect.y + clientHeight - footerHeight,
        std::max(0, contentRect.w),
        footerHeight
    };
}

void SettingsApp::ensureHitTargets() {
    if (m_hitTargetsValid) return;

    if (!m_font) {
        m_hitTargetsValid = true;
        return;
    }

    int y = kPadY - m_scrollOffset;
    y += getLineHeight() + 2 + 8;
    y += getLineHeight() + 6;

    int swatchX = kPadX;
    const int controlSize = getControlSize();
    for (SDL_Rect& swatch : m_backgroundSwatches) {
        swatch = {swatchX, y, controlSize, controlSize};
        swatchX += controlSize + kSwatchGap;
    }

    y += controlSize + 8;
    y += getLineHeight() + 8;
    y += getLineHeight() + 6;

    const int setW = measureTextWidth(m_font, "Set") + kBtnPadX * 2;
    const int clearW = measureTextWidth(m_font, "Clear") + kBtnPadX * 2;
    const int btnGap = 6;
    const int rightReserve = setW + clearW + btnGap * 2;
    const int fieldW = std::max(0, m_clientWidth - kPadX * 2 - rightReserve);
    const int fieldHeight = getFieldHeight();
    m_wallpaperFieldRect = {kPadX, y, fieldW, fieldHeight};
    m_wallpaperSetRect = {kPadX + fieldW + btnGap, y, setW, fieldHeight};
    m_wallpaperClearRect = {
        kPadX + fieldW + btnGap + setW + btnGap, y, clearW, fieldHeight
    };

    y += fieldHeight + 8;
    y += getLineHeight() + 8;
    y += getLineHeight() + 6;

    auto textWidthOr = [&](const char* text, int fallback) {
        const int width = measureTextWidth(m_font, text);
        return width > 0 ? width : fallback;
    };

    int fitX = kPadX;
    const int fitHeight = getControlSize();
    for (int i = 0; i < kWallpaperFitCount; ++i) {
        const int optionWidth = textWidthOr(kWallpaperFitOptions[static_cast<size_t>(i)].label, 50)
            + 10 * 2;
        m_wallpaperFitHitRects[static_cast<size_t>(i)] = {
            fitX, y, optionWidth, fitHeight
        };
        fitX += optionWidth + kSwatchGap;
    }

    y += fitHeight + 8;
    y += getLineHeight() + 8;
    y += getLineHeight() + 6;

    int optionX = kPadX;
    const int optionHeight = getControlSize();
    for (int i = 0; i < 2; ++i) {
        const int optionWidth = textWidthOr(i == 0 ? "12-hour" : "24-hour", 60)
            + 10 * 2;
        m_clockFormatHitRects[static_cast<size_t>(i)] = {
            optionX, y, optionWidth, optionHeight
        };
        optionX += optionWidth + kSwatchGap;
    }

    y += optionHeight + 8;
    y += getLineHeight() + 8;
    y += getLineHeight() + 6;

    int scaleX = kPadX;
    const int scaleHeight = getControlSize();
    for (int i = 0; i < kUiScaleCount; ++i) {
        const int optionWidth = textWidthOr(kUiScaleOptions[static_cast<size_t>(i)].label, 80)
            + 10 * 2;
        m_uiScaleHitRects[static_cast<size_t>(i)] = {
            scaleX, y, optionWidth, scaleHeight
        };
        scaleX += optionWidth + kSwatchGap;
    }

    m_hitTargetsValid = true;
}

void SettingsApp::onUiScaleChanged() {
    m_textSurfaceCache.clear();
    invalidateHitTargets();
    // The wallpaper prompt stores its horizontal position in pixels; discard
    // that stale offset so the next render measures it with the new font.
    m_wallpaperScrollPx = 0;
    m_contentHeight = computeContentHeight();
    clampScrollOffset();
}

void SettingsApp::invalidateHitTargets() {
    m_hitTargetsValid = false;
    for (SDL_Rect& rect : m_backgroundSwatches) {
        rect = {0, 0, 0, 0};
    }
    for (SDL_Rect& rect : m_clockFormatHitRects) {
        rect = {0, 0, 0, 0};
    }
    for (SDL_Rect& rect : m_uiScaleHitRects) {
        rect = {0, 0, 0, 0};
    }
    for (SDL_Rect& rect : m_wallpaperFitHitRects) {
        rect = {0, 0, 0, 0};
    }
    m_wallpaperFieldRect = {0, 0, 0, 0};
    m_wallpaperSetRect = {0, 0, 0, 0};
    m_wallpaperClearRect = {0, 0, 0, 0};
}

} // namespace monolith::app
