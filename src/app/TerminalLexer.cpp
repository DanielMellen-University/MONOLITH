#include "TerminalLexer.hpp"

namespace monolith::app {

CommandTokens tokenizeCommandLine(const std::string& line) {
    CommandTokens out;
    std::string cur;
    bool inToken = false;
    enum class Mode { Normal, Double, Single };
    Mode mode = Mode::Normal;

    auto flush = [&]() {
        if (inToken) {
            out.args.push_back(cur);
            cur.clear();
            inToken = false;
        }
    };

    const std::size_t n = line.size();
    for (std::size_t i = 0; i < n; ++i) {
        const char c = line[i];
        if (mode == Mode::Normal) {
            if (c == ' ' || c == '\t') {
                flush();
                continue;
            }
            if (c == '"') {
                inToken = true;
                mode = Mode::Double;
                continue;
            }
            if (c == '\'') {
                inToken = true;
                mode = Mode::Single;
                continue;
            }
            if (c == '\\') {
                inToken = true;
                if (i + 1 < n) {
                    cur.push_back(line[++i]);
                } else {
                    out.error = "trailing backslash";
                    out.args.clear();
                    return out;
                }
                continue;
            }
            inToken = true;
            cur.push_back(c);
            continue;
        }

        if (mode == Mode::Double) {
            if (c == '"') {
                mode = Mode::Normal;
                continue;
            }
            if (c == '\\') {
                if (i + 1 >= n) {
                    out.error = "unterminated double quote";
                    out.args.clear();
                    return out;
                }
                const char next = line[++i];
                if (next == '"' || next == '\\') {
                    cur.push_back(next);
                } else {
                    // Keep unknown escapes as the escaped character only.
                    cur.push_back(next);
                }
                continue;
            }
            cur.push_back(c);
            continue;
        }

        // Mode::Single
        if (c == '\'') {
            mode = Mode::Normal;
            continue;
        }
        cur.push_back(c);
    }

    if (mode == Mode::Double) {
        out.error = "unterminated double quote";
        out.args.clear();
        return out;
    }
    if (mode == Mode::Single) {
        out.error = "unterminated single quote";
        out.args.clear();
        return out;
    }

    flush();
    return out;
}

} // namespace monolith::app
