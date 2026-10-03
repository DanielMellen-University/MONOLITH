#pragma once

#include <cstddef>
#include <istream>
#include <ostream>
#include <string>
#include <string_view>
#include <utility>

namespace monolith::window::session {

inline constexpr std::size_t kMaxSessionRecords = 1024;
inline constexpr std::size_t kMaxRestoredWindows = 128;

// Keep line breaks escaped so quoted paths remain inside their session record.
inline void writePath(std::ostream& out, const std::string& path) {
    const std::string_view value = path.empty() ? std::string_view("-") : path;
    out.put('"');
    for (const char character : value) {
        if (character == '"' || character == '\\') out.put('\\');
        if (character == '\n') {
            out.put('\\');
            out.put('n');
        } else {
            out.put(character);
        }
    }
    out.put('"');
}

inline bool readPath(std::istream& in, std::string& path) {
    in >> std::ws;
    if (!in) return false;

    if (in.peek() != '"') {
        if (!(in >> path)) return false;
    } else {
        in.get();
        std::string decoded;
        char character = 0;
        bool closed = false;
        while (in.get(character)) {
            if (character == '"') {
                closed = true;
                break;
            }
            if (character == '\\') {
                if (!in.get(character)) return false;
                if (character == 'n') {
                    decoded.push_back('\n');
                } else {
                    decoded.push_back(character);
                }
            } else {
                decoded.push_back(character);
            }
        }
        if (!closed) return false;
        path = std::move(decoded);
    }
    if (path == "-") path.clear();
    return true;
}

} // namespace monolith::window::session
