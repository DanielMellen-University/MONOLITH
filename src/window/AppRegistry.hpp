#pragma once

#include <SDL2/SDL.h>

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <string_view>
#include <vector>

namespace monolith::window {

// Stable action ids shared by Start menu hit targets and desktop icons.
enum class AppAction : int {
    Terminal = 0,
    TextEditor = 1,
    Filesystem = 2,
    Settings = 3,
    Drawing = 4,
    Snake = 5,
    Minesweeper = 6,
    Pong = 7,
    Breakout = 8,
    ShutDown = 9,
};

// One registration drives Start menu, desktop icons, and session restore.
struct AppRegistryEntry {
    const char* id;                 // stable machine id ("terminal")
    const char* displayName;        // short display name
    const char* startMenuLabel;     // Start menu row label
    const char* appBaseTitle;       // Window::appBaseTitle; nullptr for shell-only
    const char* sessionKey;         // session_v1 kind; nullptr if not restored
    AppAction action;
    const char* startMenuCategory;  // nullptr = top-level; e.g. "Games"
    bool showInStartMenu;
    bool showDesktopIcon;
    int desktopOrder;               // order among desktop icons; ignored if none
    const char* desktopLabel;       // nullptr => displayName
    const char* desktopGlyph;       // single letter in the icon tile
    Uint8 iconR;
    Uint8 iconG;
    Uint8 iconB;
};

// Declaration order is Start-menu order (categories insert separators).
inline constexpr AppRegistryEntry kAppRegistry[] = {
    {"terminal", "Terminal", "Terminal", "Terminal", "terminal",
     AppAction::Terminal, nullptr, true, true, 0, nullptr, "T", 55, 85, 140},
    {"editor", "Editor", "Text Editor", "Editor", "editor",
     AppAction::TextEditor, nullptr, true, true, 2, "Editor", "E", 120, 95, 70},
    {"filesystem", "Filesystem", "Filesystem", "Filesystem", "filesystem",
     AppAction::Filesystem, nullptr, true, true, 1, nullptr, "F", 70, 120, 90},
    {"drawing", "Drawing", "Drawing", "Drawing", "drawing",
     AppAction::Drawing, nullptr, true, true, 3, nullptr, "D", 140, 80, 110},
    {"settings", "Settings", "Settings", "Settings", "settings",
     AppAction::Settings, nullptr, true, true, 4, nullptr, "S", 90, 90, 110},
    {"snake", "Snake", "Snake", "Snake", "snake",
     AppAction::Snake, "Games", true, false, -1, nullptr, nullptr, 0, 0, 0},
    {"minesweeper", "Minesweeper", "Minesweeper", "Minesweeper", "minesweeper",
     AppAction::Minesweeper, "Games", true, false, -1, nullptr, nullptr, 0, 0, 0},
    {"pong", "Pong", "Pong", "Pong", "pong",
     AppAction::Pong, "Games", true, false, -1, nullptr, nullptr, 0, 0, 0},
    {"breakout", "Breakout", "Breakout", "Breakout", "breakout",
     AppAction::Breakout, "Games", true, false, -1, nullptr, nullptr, 0, 0, 0},
    {"shutdown", "Shut Down", "Shut Down", nullptr, nullptr,
     AppAction::ShutDown, nullptr, true, false, -1, nullptr, nullptr, 0, 0, 0},
};

inline constexpr std::size_t kAppRegistryCount =
    sizeof(kAppRegistry) / sizeof(kAppRegistry[0]);

inline constexpr int kDesktopIconCount = []() constexpr {
    int n = 0;
    for (std::size_t i = 0; i < kAppRegistryCount; ++i) {
        if (kAppRegistry[i].showDesktopIcon) ++n;
    }
    return n;
}();

struct StartMenuRow {
    const char* label;
    int action; // AppAction, or -1 category, or -2 separator
    int indent; // 0 top-level, 1 nested under a category
};

inline std::vector<StartMenuRow> buildStartMenuRows() {
    std::vector<StartMenuRow> rows;
    rows.reserve(kAppRegistryCount + 4);

    bool emittedSinceSeparator = false;
    const char* openCategory = nullptr;

    for (const auto& app : kAppRegistry) {
        if (!app.showInStartMenu) continue;

        if (app.action == AppAction::ShutDown) {
            if (emittedSinceSeparator) {
                rows.push_back({"", -2, 0});
            }
            rows.push_back({app.startMenuLabel, static_cast<int>(app.action), 0});
            emittedSinceSeparator = false;
            openCategory = nullptr;
            continue;
        }

        if (app.startMenuCategory != nullptr) {
            if (openCategory == nullptr
                || std::strcmp(openCategory, app.startMenuCategory) != 0) {
                if (emittedSinceSeparator) {
                    rows.push_back({"", -2, 0});
                }
                rows.push_back({app.startMenuCategory, -1, 0});
                openCategory = app.startMenuCategory;
            }
            rows.push_back({app.startMenuLabel, static_cast<int>(app.action), 1});
            emittedSinceSeparator = true;
            continue;
        }

        openCategory = nullptr;
        rows.push_back({app.startMenuLabel, static_cast<int>(app.action), 0});
        emittedSinceSeparator = true;
    }
    return rows;
}

inline const AppRegistryEntry* findAppByAction(AppAction action) {
    for (const auto& app : kAppRegistry) {
        if (app.action == action) return &app;
    }
    return nullptr;
}

inline const AppRegistryEntry* findAppBySessionKey(std::string_view key) {
    if (key.empty()) return nullptr;
    for (const auto& app : kAppRegistry) {
        if (app.sessionKey && key == app.sessionKey) return &app;
    }
    return nullptr;
}

inline const AppRegistryEntry* findAppByBaseTitle(std::string_view base) {
    if (base.empty()) return nullptr;
    for (const auto& app : kAppRegistry) {
        if (app.appBaseTitle && base == app.appBaseTitle) return &app;
    }
    return nullptr;
}

inline int startMenuContentHeightLogical(
    int topPad, int itemH, int itemGap, int categoryH, int separatorH, int bottomPad) {
    const auto rows = buildStartMenuRows();
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

struct DesktopIconDef {
    const char* label;
    const char* glyph;
    AppAction action;
    Uint8 r;
    Uint8 g;
    Uint8 b;
};

inline std::vector<DesktopIconDef> collectDesktopIconDefs() {
    struct Item {
        int order;
        DesktopIconDef def;
    };
    std::vector<Item> items;
    items.reserve(static_cast<std::size_t>(kDesktopIconCount));
    for (const auto& app : kAppRegistry) {
        if (!app.showDesktopIcon) continue;
        items.push_back({
            app.desktopOrder,
            DesktopIconDef{
                app.desktopLabel ? app.desktopLabel : app.displayName,
                app.desktopGlyph,
                app.action,
                app.iconR,
                app.iconG,
                app.iconB,
            },
        });
    }
    std::sort(items.begin(), items.end(),
              [](const Item& a, const Item& b) { return a.order < b.order; });
    std::vector<DesktopIconDef> out;
    out.reserve(items.size());
    for (const auto& item : items) out.push_back(item.def);
    return out;
}

} // namespace monolith::window
