#pragma once

#include <cstddef>
#include <istream>
#include <string>

namespace monolith::detail {

inline constexpr std::size_t kMaxPersistedLineBytes = 16 * 1024;

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
        if (line.size() == kMaxPersistedLineBytes) {
            line.clear();
            return BoundedLineResult::TooLong;
        }
        line.push_back(static_cast<char>(next));
    }
}

} // namespace monolith::detail
