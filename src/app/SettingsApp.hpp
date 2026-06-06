#pragma once

#include "App.hpp"
#include "../fs/Filesystem.hpp"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <string>
#include <vector>

namespace monolith::app {

/**
 * A simple Settings / About panel.
 * Currently shows build info, filesystem details, and environment info.
 * Acts as the real implementation behind the Start Menu "Settings" entry.
 */
class SettingsApp : public App {
public:
    SettingsApp(TTF_Font* font, monolith::fs::Filesystem* fs = nullptr);
    ~SettingsApp() override = default;

    void render(SDL_Renderer* renderer, const SDL_Rect& contentRect) override;
    void handleEvent(const SDL_Event& event) override;
    void onResize(int clientWidth, int clientHeight) override;

private:
    struct InfoLine {
        std::string label;
        std::string value;
    };

    void buildInfoLines();

    TTF_Font* m_font = nullptr;
    monolith::fs::Filesystem* m_fs = nullptr;

    std::vector<InfoLine> m_lines;

    int m_clientWidth = 0;
    int m_clientHeight = 0;
};

} // namespace monolith::app
