#pragma once

#include <iomanip>
#include <istream>
#include <ostream>
#include <string>

namespace monolith::window::session {

// Quote paths in new session files while keeping the legacy '-' sentinel.
inline void writePath(std::ostream& out, const std::string& path) {
    const std::string value = path.empty() ? "-" : path;
    out << std::quoted(value);
}

// std::quoted accepts both quoted values and legacy whitespace-free tokens.
inline bool readPath(std::istream& in, std::string& path) {
    if (!(in >> std::quoted(path))) return false;
    if (path == "-") path.clear();
    return true;
}

} // namespace monolith::window::session
