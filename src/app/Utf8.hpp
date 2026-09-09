#pragma once

#include <cstddef>
#include <string>

namespace monolith::app {

// Cursor columns are byte offsets, but editing moves by UTF-8 codepoint.
inline std::size_t utf8CodepointByteLen(const std::string& value, std::size_t index) {
    if (index >= value.size()) return 0;
    const unsigned char c = static_cast<unsigned char>(value[index]);
    if ((c & 0x80) == 0x00) return 1;
    if ((c & 0xE0) == 0xC0) return 2;
    if ((c & 0xF0) == 0xE0) return 3;
    if ((c & 0xF8) == 0xF0) return 4;
    return 1;
}

inline std::size_t utf8PrevCodepointStart(const std::string& value, std::size_t offset) {
    if (offset == 0 || offset > value.size()) return 0;
    std::size_t index = offset - 1;
    while (index > 0 && (static_cast<unsigned char>(value[index]) & 0xC0) == 0x80) {
        --index;
    }
    return index;
}

inline void popLastUtf8Codepoint(std::string& value) {
    if (value.empty()) return;
    value.erase(utf8PrevCodepointStart(value, value.size()));
}

} // namespace monolith::app
