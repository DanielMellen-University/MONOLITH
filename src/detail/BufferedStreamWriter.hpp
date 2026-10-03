#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <ostream>
#include <string_view>

namespace monolith::detail {

/** Buffers serialized text in bounded chunks and propagates stream failures. */
class BufferedStreamWriter {
public:
    explicit BufferedStreamWriter(std::ostream& output) : m_output(output) {}

    bool append(std::string_view text) {
        if (m_failed) return false;

        while (!text.empty()) {
            const std::size_t count = std::min(m_buffer.size() - m_buffered, text.size());
            std::memcpy(m_buffer.data() + m_buffered, text.data(), count);
            m_buffered += count;
            text.remove_prefix(count);
            if (m_buffered == m_buffer.size() && !flush()) return false;
        }
        return true;
    }

    bool finish() {
        return !m_failed && flush();
    }

private:
    bool flush() {
        if (m_buffered == 0) return true;
        m_output.write(m_buffer.data(), static_cast<std::streamsize>(m_buffered));
        if (!m_output) {
            m_failed = true;
            return false;
        }
        m_buffered = 0;
        return true;
    }

    std::ostream& m_output;
    std::array<char, 16 * 1024> m_buffer{};
    std::size_t m_buffered = 0;
    bool m_failed = false;
};

} // namespace monolith::detail
