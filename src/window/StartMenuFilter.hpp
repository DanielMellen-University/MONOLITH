#pragma once

#include "AppRegistry.hpp"

#include <cstring>
#include <string>
#include <string_view>
#include <vector>

namespace monolith::window {

inline constexpr std::size_t kMaxStartMenuFilterBytes = 64;

// Appends complete UTF-8 codepoints up to the filter's byte budget.
inline std::size_t appendStartMenuFilterInput(std::string& filter,
                                              std::string_view input) {
    std::size_t remaining = filter.size() < kMaxStartMenuFilterBytes
        ? kMaxStartMenuFilterBytes - filter.size()
        : 0;
    const std::size_t initialSize = filter.size();
    for (std::size_t start = 0; start < input.size();) {
        const unsigned char lead = static_cast<unsigned char>(input[start]);
        const std::size_t codepointBytes = lead < 0x80 ? 1
            : (lead & 0xE0) == 0xC0 ? 2
            : (lead & 0xF0) == 0xE0 ? 3
            : (lead & 0xF8) == 0xF0 ? 4
            : 1;
        if (start + codepointBytes > input.size()) break;

        bool validContinuation = true;
        for (std::size_t offset = 1; offset < codepointBytes; ++offset) {
            if ((static_cast<unsigned char>(input[start + offset]) & 0xC0) != 0x80) {
                validContinuation = false;
                break;
            }
        }
        if (!validContinuation || codepointBytes > remaining) break;
        filter.append(input.data() + start, codepointBytes);
        remaining -= codepointBytes;
        start += codepointBytes;
    }
    return filter.size() - initialSize;
}

inline char startMenuAsciiLower(char c) {
    if (c >= 'A' && c <= 'Z') return static_cast<char>(c - 'A' + 'a');
    return c;
}

// Case-insensitive ASCII substring match on a Start menu label.
inline bool startMenuLabelMatches(const char* label, std::string_view filter) {
    if (filter.empty()) return true;
    if (!label) return false;
    const std::size_t n = std::strlen(label);
    const std::size_t m = filter.size();
    if (m > n) return false;
    for (std::size_t i = 0; i + m <= n; ++i) {
        bool ok = true;
        for (std::size_t j = 0; j < m; ++j) {
            if (startMenuAsciiLower(label[i + j])
                != startMenuAsciiLower(filter[j])) {
                ok = false;
                break;
            }
        }
        if (ok) return true;
    }
    return false;
}

// Keep actionable rows whose label matches filter. Category headers stay only
// when they still have a matching nested child. Separators stay only when they
// still separate kept content. Headers remain action -1 (not hit-testable).
inline std::vector<StartMenuRow> filterStartMenuRows(
    const std::vector<StartMenuRow>& rows, std::string_view filter) {
    if (filter.empty()) return rows;

    std::vector<bool> keep(rows.size(), false);
    for (std::size_t i = 0; i < rows.size(); ++i) {
        if (rows[i].action >= 0
            && startMenuLabelMatches(rows[i].label, filter)) {
            keep[i] = true;
        }
    }

    for (std::size_t i = 0; i < rows.size(); ++i) {
        if (rows[i].action != -1) continue;
        bool hasChild = false;
        for (std::size_t j = i + 1; j < rows.size(); ++j) {
            if (rows[j].action == -1 || rows[j].action == -2) break;
            if (rows[j].indent == 0) break;
            if (keep[j]) {
                hasChild = true;
                break;
            }
        }
        if (hasChild) keep[i] = true;
    }

    std::vector<StartMenuRow> out;
    out.reserve(rows.size());
    for (std::size_t i = 0; i < rows.size(); ++i) {
        if (rows[i].action == -2) {
            const bool before = !out.empty() && out.back().action != -2;
            bool after = false;
            for (std::size_t j = i + 1; j < rows.size(); ++j) {
                if (keep[j] && rows[j].action != -2) {
                    after = true;
                    break;
                }
            }
            if (before && after) out.push_back(rows[i]);
            continue;
        }
        if (keep[i]) out.push_back(rows[i]);
    }
    while (!out.empty() && out.back().action == -2) out.pop_back();
    return out;
}

inline int startMenuRowsContentHeightLogical(
    const std::vector<StartMenuRow>& rows,
    int topPad, int itemH, int itemGap, int categoryH, int separatorH, int bottomPad) {
    int height = topPad;
    for (const auto& row : rows) {
        if (row.action == -2) {
            height += separatorH;
        } else if (row.action == -1) {
            height += categoryH + itemGap;
        } else {
            height += itemH + itemGap;
        }
    }
    return height + bottomPad;
}

} // namespace monolith::window
