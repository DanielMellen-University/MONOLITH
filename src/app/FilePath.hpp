#pragma once

#include <cctype>
#include <string>

namespace monolith::app {

/** Returns true when value ends with suffix, ignoring ASCII letter case. */
inline bool hasCaseInsensitiveSuffix(const std::string& value,
                                     const std::string& suffix) {
    if (value.size() < suffix.size()) return false;

    const size_t offset = value.size() - suffix.size();
    for (size_t i = 0; i < suffix.size(); ++i) {
        const unsigned char a = static_cast<unsigned char>(value[offset + i]);
        const unsigned char b = static_cast<unsigned char>(suffix[i]);
        if (std::tolower(a) != std::tolower(b)) return false;
    }
    return true;
}

/** True for desktop wallpaper image extensions: .bmp, .png, .jpg, .jpeg. */
inline bool isWallpaperImagePath(const std::string& path) {
    return hasCaseInsensitiveSuffix(path, ".bmp")
        || hasCaseInsensitiveSuffix(path, ".png")
        || hasCaseInsensitiveSuffix(path, ".jpg")
        || hasCaseInsensitiveSuffix(path, ".jpeg");
}

} // namespace monolith::app
