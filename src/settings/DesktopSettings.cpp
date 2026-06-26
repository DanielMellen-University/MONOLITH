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

} // namespace

bool DesktopSettings::loadFromHostPath(const std::string& hostPath) {
    std::ifstream in(hostPath);
    if (!in) return false;

    std::string line;
    while (std::getline(in, line)) {
        const std::string key = "desktop_background=";
        if (line.rfind(key, 0) != 0) continue;

        RGB parsed;
        if (parseRgbTriplet(line.substr(key.size()), parsed)) {
            m_desktopBackground = parsed;
            return true;
        }
    }

    return false;
}

bool DesktopSettings::saveToHostPath(const std::string& hostPath) const {
    std::ofstream out(hostPath, std::ios::trunc);
    if (!out) return false;

    out << "desktop_background="
        << static_cast<int>(m_desktopBackground.r) << ','
        << static_cast<int>(m_desktopBackground.g) << ','
        << static_cast<int>(m_desktopBackground.b) << '\n';
    return static_cast<bool>(out);
}

} // namespace monolith::settings