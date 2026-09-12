#include "SettingsApp.hpp"
#include "FilePath.hpp"
#include "Utf8.hpp"

#include <algorithm>
#include <cctype>
#include <vector>

namespace monolith::app {

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

void SettingsApp::onUiScaleChanged() {
    // The wallpaper prompt stores its horizontal position in pixels; discard
    // that stale offset so the next render measures it with the new font.
    m_wallpaperScrollPx = 0;
    m_contentHeight = computeContentHeight();
    clampScrollOffset();
}

} // namespace monolith::app
