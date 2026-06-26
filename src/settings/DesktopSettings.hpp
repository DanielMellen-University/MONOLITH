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

    bool loadFromHostPath(const std::string& hostPath);
    bool saveToHostPath(const std::string& hostPath) const;

private:
    RGB m_desktopBackground = kDefaultDesktopBackground;
};

} // namespace monolith::settings