#pragma once

#include <cstdint>
#include <string>

namespace monolith::settings {

struct RGB {
    uint8_t r = 25;
    uint8_t g = 25;
    uint8_t b = 30;
};

class DesktopSettings {
public:
    static constexpr RGB kDefaultDesktopBackground{25, 25, 30};

    RGB desktopBackground() const { return m_desktopBackground; }
    void setDesktopBackground(RGB color) { m_desktopBackground = color; }

    // false = 12-hour (default), true = 24-hour
    bool clock24Hour() const { return m_clock24Hour; }
    void setClock24Hour(bool enabled) { m_clock24Hour = enabled; }

    // Stored as a percentage. The Settings app exposes 90, 100, and 115.
    int uiScalePercent() const { return m_uiScalePercent; }
    void setUiScalePercent(int percent) { m_uiScalePercent = percent; }

    // Empty clears wallpaper (solid color only).
    const std::string& wallpaperPath() const { return m_wallpaperPath; }
    void setWallpaperPath(std::string path) { m_wallpaperPath = std::move(path); }

    bool loadFromHostPath(const std::string& hostPath);
    bool saveToHostPath(const std::string& hostPath) const;

private:
    RGB m_desktopBackground = kDefaultDesktopBackground;
    bool m_clock24Hour = false;
    int m_uiScalePercent = 100;
    std::string m_wallpaperPath;
};

} // namespace monolith::settings
