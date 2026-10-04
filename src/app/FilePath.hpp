#pragma once

#include <cctype>
#include <cstddef>
#include <string>

namespace monolith::app {

/** Expands the virtual-home shorthand without changing ordinary relative paths. */
inline std::string expandVirtualHomeShorthand(const std::string& path) {
    if (path == "~") return "/home/monolith";
    if (path.size() >= 2 && path.compare(0, 2, "~/") == 0) {
        return "/home/monolith" + path.substr(1);
    }
    return path;
}

/** Adjusts an input caret for the expanded home prefix. */
inline std::size_t expandVirtualHomeCursorPosition(const std::string& path,
                                                   std::size_t cursor) {
    if (cursor > path.size()) cursor = path.size();
    constexpr std::size_t homeLength = sizeof("/home/monolith") - 1;
    if (path == "~") return cursor == 0 ? 0 : homeLength;
    if (cursor > 0 && path.size() >= 2 && path.compare(0, 2, "~/") == 0) {
        return cursor + homeLength - 1;
    }
    return cursor;
}

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
