// Headless checks for Start menu type-ahead filter helpers (no WindowManager).
#include "../src/window/StartMenuFilter.hpp"

#include <algorithm>
#include <iostream>
#include <string>
#include <vector>

int main() {
    using namespace monolith::window;
    int failures = 0;
    auto check = [&](bool ok, const char* message) {
        if (!ok) {
            std::cerr << "FAIL: " << message << '\n';
            ++failures;
        } else {
            std::cout << "ok: " << message << '\n';
        }
    };

    check(startMenuLabelMatches("Terminal", ""), "empty filter matches any label");
    check(startMenuLabelMatches("Terminal", "term"), "case-insensitive substring match");
    check(startMenuLabelMatches("Terminal", "TERM"), "uppercase filter matches");
    check(startMenuLabelMatches("Text Editor", "edit"), "mid-label substring match");
    check(!startMenuLabelMatches("Pong", "snake"), "non-matching label is rejected");
    check(!startMenuLabelMatches(nullptr, "x"), "null label does not match non-empty filter");

    std::size_t longestActionLabel = 0;
    for (const auto& row : buildStartMenuRows()) {
        if (row.action >= 0 && row.label) {
            longestActionLabel = std::max(longestActionLabel, std::string(row.label).size());
        }
    }
    check(longestActionLabel < kMaxStartMenuFilterBytes,
          "filter byte limit exceeds every actionable menu label");

    std::string boundedFilter(kMaxStartMenuFilterBytes - 3, 'x');
    const std::size_t appendedBytes = appendStartMenuFilterInput(
        boundedFilter, "\xC3\xA9" "yz");
    check(appendedBytes == 3 && boundedFilter.size() == kMaxStartMenuFilterBytes
              && boundedFilter.substr(boundedFilter.size() - 3) == "\xC3\xA9" "y",
          "filter input stops at the byte cap without splitting UTF-8 codepoints");
    check(appendStartMenuFilterInput(boundedFilter, "more") == 0,
          "filter input cannot grow beyond its byte cap");
    std::string incompleteFilter;
    check(appendStartMenuFilterInput(incompleteFilter, "\xE2\x82") == 0
              && incompleteFilter.empty(),
          "filter input ignores an incomplete trailing UTF-8 codepoint");

    const auto all = buildStartMenuRows();
    const auto same = filterStartMenuRows(all, "");
    check(same.size() == all.size(), "empty filter returns every row");

    const auto term = filterStartMenuRows(all, "term");
    check(!term.empty(), "term filter is non-empty");
    bool onlyTerminal = true;
    bool sawCategory = false;
    for (const auto& row : term) {
        if (row.action == -1) sawCategory = true;
        if (row.action >= 0 && std::string(row.label) != "Terminal") {
            onlyTerminal = false;
        }
    }
    check(onlyTerminal, "term filter keeps only Terminal among actionable rows");
    check(!sawCategory, "term filter drops Games category with no matching children");

    const auto snake = filterStartMenuRows(all, "snake");
    bool sawGames = false;
    bool sawSnake = false;
    bool sawOtherGame = false;
    for (const auto& row : snake) {
        if (row.action == -1 && row.label && std::string(row.label) == "Games") {
            sawGames = true;
        }
        if (row.action == static_cast<int>(AppAction::Snake)) sawSnake = true;
        if (row.action == static_cast<int>(AppAction::Pong)
            || row.action == static_cast<int>(AppAction::Breakout)
            || row.action == static_cast<int>(AppAction::Minesweeper)) {
            sawOtherGame = true;
        }
    }
    check(sawGames, "snake filter keeps Games header");
    check(sawSnake, "snake filter keeps Snake");
    check(!sawOtherGame, "snake filter drops non-matching games");

    const auto none = filterStartMenuRows(all, "zzzz-no-match");
    check(none.empty(), "unmatched filter yields empty rows");

    const int fullH = startMenuContentHeightLogical(6, 26, 2, 20, 10, 8);
    const int filteredH = startMenuRowsContentHeightLogical(
        term, 6, 26, 2, 20, 10, 8);
    check(filteredH > 0 && filteredH < fullH,
          "filtered content height is smaller than full menu");

    // Separators should not lead or trail the filtered list.
    const auto shut = filterStartMenuRows(all, "shut");
    check(!shut.empty() && shut.front().action != -2, "no leading separator");
    check(!shut.empty() && shut.back().action != -2, "no trailing separator");
    check(shut.back().action == static_cast<int>(AppAction::ShutDown),
          "shut filter keeps Shut Down");

    if (failures == 0) {
        std::cout << "ALL START MENU FILTER TESTS PASSED\n";
        return 0;
    }
    return 1;
}
