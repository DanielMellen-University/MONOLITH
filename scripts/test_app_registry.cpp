// Headless checks for the shared App registry (no WindowManager / display).
#include "../src/window/AppRegistry.hpp"
#include "../src/window/DesktopIcons.hpp"

#include <iostream>
#include <set>
#include <string>

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

    check(kAppRegistryCount >= 10, "registry lists the built-in apps plus Shut Down");
    check(kDesktopIconCount == 5, "five apps expose desktop icons");

    const auto rows = buildStartMenuRows();
    check(!rows.empty(), "Start menu rows are non-empty");

    bool sawGames = false;
    bool sawSnakeUnderGames = false;
    bool sawShutDown = false;
    bool sawSepBeforeGames = false;
    for (std::size_t i = 0; i < rows.size(); ++i) {
        const auto& row = rows[i];
        if (row.action == -1 && row.label && std::string(row.label) == "Games") {
            sawGames = true;
            if (i > 0 && rows[i - 1].action == -2) sawSepBeforeGames = true;
        }
        if (row.action == static_cast<int>(AppAction::Snake)) {
            sawSnakeUnderGames = (row.indent == 1);
        }
        if (row.action == static_cast<int>(AppAction::ShutDown)) {
            sawShutDown = true;
            check(i > 0 && rows[i - 1].action == -2,
                  "Shut Down is preceded by a separator");
        }
    }
    check(sawGames, "Games category header is present");
    check(sawSepBeforeGames, "separator precedes Games");
    check(sawSnakeUnderGames, "Snake is nested under Games");
    check(sawShutDown, "Shut Down remains in the Start menu");

    check(findAppBySessionKey("breakout")
              && findAppBySessionKey("breakout")->action == AppAction::Breakout,
          "session key breakout maps to Breakout");
    check(findAppByBaseTitle("Terminal")
              && std::string(findAppByBaseTitle("Terminal")->sessionKey) == "terminal",
          "base title Terminal maps to session key terminal");
    check(findAppByAction(AppAction::Pong) != nullptr, "Pong is registered");
    check(findAppBySessionKey("nope") == nullptr, "unknown session keys miss");

    const auto icons = collectDesktopIconDefs();
    check(static_cast<int>(icons.size()) == kDesktopIconCount,
          "desktop icon collector matches count");
    check(icons.front().action == AppAction::Terminal, "first desktop icon is Terminal");
    check(icons[1].action == AppAction::Filesystem, "second desktop icon is Filesystem");
    check(icons[2].action == AppAction::TextEditor, "third desktop icon is Editor");
    check(icons.back().action == AppAction::Settings, "last desktop icon is Settings");

    std::set<int> actions;
    for (const auto& app : kAppRegistry) {
        check(actions.insert(static_cast<int>(app.action)).second,
              "each registry action id is unique");
    }

    if (failures == 0) {
        std::cout << "ALL APP REGISTRY TESTS PASSED\n";
        return 0;
    }
    return 1;
}
