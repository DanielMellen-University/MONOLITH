#include "DesktopSettings.hpp"

#include <fstream>
#include <sstream>

namespace monolith::settings {

namespace {

bool parseRgbTriplet(const std::string& value, RGB& out) {
    int r = 0;
    int g = 0;
    int b = 0;
    char comma1 = 0;
    char comma2 = 0;
    std::istringstream iss(value);
    if (!(iss >> r >> comma1 >> g >> comma2 >> b)) return false;
    if (comma1 != ',' || comma2 != ',') return false;
    if (r < 0 || r > 255 || g < 0 || g > 255 || b < 0 || b > 255) return false;
    out.r = static_cast<uint8_t>(r);
    out.g = static_cast<uint8_t>(g);
    out.b = static_cast<uint8_t>(b);
    return true;
}

bool parseBool01(const std::string& value, bool& out) {
    if (value == "1" || value == "true" || value == "True" || value == "yes") {
        out = true;
        return true;
    }
    if (value == "0" || value == "false" || value == "False" || value == "no") {
        out = false;
        return true;
    }
    return false;
}

bool parsePositiveInt(const std::string& value, int& out) {
    if (value.empty()) return false;
    try {
        size_t idx = 0;
        const int parsed = std::stoi(value, &idx);
        if (idx != value.size()) return false;
        out = parsed;
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace

int DesktopSettings::clampUiFontSize(int points) {
    if (points < kMinUiFontSize) return kMinUiFontSize;
    if (points > kMaxUiFontSize) return kMaxUiFontSize;
    // Snap to even sizes used by Settings (12/14/16/18).
    if (points <= 12) return 12;
    if (points <= 14) return 14;
    if (points <= 16) return 16;
    return 18;
}

void DesktopSettings::setUiFontSize(int points) {
    m_uiFontSize = clampUiFontSize(points);
}

bool DesktopSettings::loadFromHostPath(const std::string& hostPath) {
    std::ifstream in(hostPath);
    if (!in) return false;

    bool loadedAny = false;
    std::string line;
    while (std::getline(in, line)) {
        const std::string bgKey = "desktop_background=";
        if (line.rfind(bgKey, 0) == 0) {
            RGB parsed;
            if (parseRgbTriplet(line.substr(bgKey.size()), parsed)) {
                m_desktopBackground = parsed;
                loadedAny = true;
            }
            continue;
        }

        const std::string wallpaperKey = "wallpaper_path=";
        if (line.rfind(wallpaperKey, 0) == 0) {
            m_wallpaperPath = line.substr(wallpaperKey.size());
            loadedAny = true;
            continue;
        }

        const std::string clockKey = "clock_24_hour=";
        if (line.rfind(clockKey, 0) == 0) {
            bool parsed = false;
            if (parseBool01(line.substr(clockKey.size()), parsed)) {
                m_clock24Hour = parsed;
                loadedAny = true;
            }
            continue;
        }

        const std::string fontKey = "ui_font_size=";
        if (line.rfind(fontKey, 0) == 0) {
            int parsed = 0;
            if (parsePositiveInt(line.substr(fontKey.size()), parsed)) {
                m_uiFontSize = clampUiFontSize(parsed);
                loadedAny = true;
            }
            continue;
        }
    }

    return loadedAny;
}

bool DesktopSettings::saveToHostPath(const std::string& hostPath) const {
    std::ofstream out(hostPath, std::ios::trunc);
    if (!out) return false;

    out << "desktop_background="
        << static_cast<int>(m_desktopBackground.r) << ','
        << static_cast<int>(m_desktopBackground.g) << ','
        << static_cast<int>(m_desktopBackground.b) << '\n';
    out << "wallpaper_path=" << m_wallpaperPath << '\n';
    out << "clock_24_hour=" << (m_clock24Hour ? "1" : "0") << '\n';
    out << "ui_font_size=" << m_uiFontSize << '\n';
    return static_cast<bool>(out);
}

} // namespace monolith::settings
