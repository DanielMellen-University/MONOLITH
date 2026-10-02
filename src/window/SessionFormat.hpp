#pragma once

#include <cstddef>
#include <iomanip>
#include <istream>
#include <ostream>
#include <string>

namespace monolith::window::session {

inline constexpr std::size_t kMaxLineBytes = 16 * 1024;

enum class BoundedLineResult {
    Line,
    TooLong,
    End,
    Error,
};

inline BoundedLineResult readBoundedLine(std::istream& in, std::string& line) {
    line.clear();
    while (true) {
        const int next = in.get();
        if (next == std::char_traits<char>::eof()) {
            if (in.bad()) return BoundedLineResult::Error;
            return line.empty() ? BoundedLineResult::End : BoundedLineResult::Line;
        }
        if (next == '\n') return BoundedLineResult::Line;
        if (line.size() == kMaxLineBytes) {
            line.clear();
            return BoundedLineResult::TooLong;
        }
        line.push_back(static_cast<char>(next));
    }
}

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
