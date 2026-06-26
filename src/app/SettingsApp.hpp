#pragma once

#include "App.hpp"
#include "../fs/Filesystem.hpp"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <array>
#include <string>
#include <vector>

namespace monolith::app {

/**
 * Settings / About panel with a small set of live desktop preferences.
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

    struct BackgroundPreset {
        const char* name;
        uint8_t r;
        uint8_t g;
        uint8_t b;
    };

    void buildInfoLines();
    int renderAppearanceSection(SDL_Renderer* renderer, const SDL_Rect& contentRect, int clientY);
    int renderInfoLines(SDL_Renderer* renderer, const SDL_Rect& contentRect, int clientY);
    void renderFooter(SDL_Renderer* renderer, const SDL_Rect& contentRect);
    void applyBackgroundPreset(const BackgroundPreset& preset);
    int activePresetIndex() const;
    int scrollAreaHeight() const;
    int computeContentHeight() const;
    void clampScrollOffset();

    static constexpr int kPresetCount = 6;
    static constexpr std::array<BackgroundPreset, kPresetCount> kBackgroundPresets{{
        {"Default", 25, 25, 30},
        {"Deep Blue", 18, 24, 42},
        {"Slate", 32, 36, 48},
        {"Forest", 20, 32, 24},
        {"Wine", 36, 20, 28},
        {"Teal", 16, 28, 32},
    }};

    TTF_Font* m_font = nullptr;
    monolith::fs::Filesystem* m_fs = nullptr;

    std::vector<InfoLine> m_lines;
    std::array<SDL_Rect, kPresetCount> m_backgroundSwatches{};
    int m_contentHeight = 0;
    int m_scrollOffset = 0;

    int m_clientWidth = 0;
    int m_clientHeight = 0;
};

} // namespace monolith::app