#include "TerminalLexer.hpp"

#include <algorithm>
#include <utility>

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

CompletionContext completionContextAt(const std::string& line, std::size_t cursor) {
    CompletionContext out;
    cursor = std::min(cursor, line.size());

    bool inToken = false;
    bool firstWord = true;
    char quote = '\0';
    std::size_t valueStart = 0;
    std::string decoded;

    for (std::size_t i = 0; i < cursor; ++i) {
        const char c = line[i];

        if (quote == '\'') {
            if (c == '\'') {
                quote = '\0';
            } else {
                decoded.push_back(c);
            }
            continue;
        }

        if (quote == '"') {
            if (c == '"') {
                quote = '\0';
            } else if (c == '\\' && i + 1 < cursor) {
                decoded.push_back(line[++i]);
            } else {
                decoded.push_back(c);
            }
            continue;
        }

        if (c == ' ' || c == '\t') {
            if (inToken) {
                firstWord = false;
                inToken = false;
                decoded.clear();
            }
            valueStart = i + 1;
            continue;
        }

        if (!inToken) {
            inToken = true;
            valueStart = i;
            decoded.clear();
        }

        if ((c == '"' || c == '\'') && decoded.empty() && i == valueStart) {
            quote = c;
            valueStart = i + 1;
        } else if (c == '"' || c == '\'') {
            quote = c;
        } else if (c == '\\' && i + 1 < cursor) {
            decoded.push_back(line[++i]);
        } else {
            decoded.push_back(c);
        }
    }

    out.replacementStart = valueStart;
    out.prefix = std::move(decoded);
    out.firstWord = firstWord;
    out.hasToken = inToken;
    out.quote = quote;
    return out;
}

std::string escapeCompletion(const std::string& value, char quote) {
    std::string escaped;
    escaped.reserve(value.size());

    for (const char c : value) {
        bool needsEscape = false;
        if (quote == '"') {
            needsEscape = c == '\\' || c == '"';
        } else if (quote == '\0') {
            needsEscape = c == '\\' || c == ' ' || c == '\t'
                       || c == '\'' || c == '"';
        }

        if (needsEscape) escaped.push_back('\\');
        escaped.push_back(c);
    }
    return escaped;
}

} // namespace monolith::app
