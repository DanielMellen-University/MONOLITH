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
    static constexpr int kDefaultUiFontSize = 14;
    static constexpr int kMinUiFontSize = 12;
    static constexpr int kMaxUiFontSize = 18;

    RGB desktopBackground() const { return m_desktopBackground; }
    void setDesktopBackground(RGB color) { m_desktopBackground = color; }

    // false = 12-hour (default), true = 24-hour
    bool clock24Hour() const { return m_clock24Hour; }
    void setClock24Hour(bool enabled) { m_clock24Hour = enabled; }

    // Empty clears wallpaper (solid color only).
    const std::string& wallpaperPath() const { return m_wallpaperPath; }
    void setWallpaperPath(std::string path) { m_wallpaperPath = std::move(path); }

    // Shell UI font size in points (title bars, taskbar, Start menu).
    int uiFontSize() const { return m_uiFontSize; }
    void setUiFontSize(int points);

    static int clampUiFontSize(int points);

    bool loadFromHostPath(const std::string& hostPath);
    bool saveToHostPath(const std::string& hostPath) const;

private:
    RGB m_desktopBackground = kDefaultDesktopBackground;
    bool m_clock24Hour = false;
    std::string m_wallpaperPath;
    int m_uiFontSize = kDefaultUiFontSize;
};

} // namespace monolith::settings
