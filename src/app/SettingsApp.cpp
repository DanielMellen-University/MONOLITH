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

void SettingsApp::onUiScaleChanged() {
    // The wallpaper prompt stores its horizontal position in pixels; discard
    // that stale offset so the next render measures it with the new font.
    m_wallpaperScrollPx = 0;
    m_contentHeight = computeContentHeight();
    clampScrollOffset();
}

} // namespace monolith::app
