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

    struct UiScaleOption {
        const char* label;
        int percent;
    };

    void buildInfoLines();
    int renderAppearanceSection(SDL_Renderer* renderer, const SDL_Rect& contentRect, int clientY);
    int renderInfoLines(SDL_Renderer* renderer, const SDL_Rect& contentRect, int clientY);
    void renderFooter(SDL_Renderer* renderer, const SDL_Rect& contentRect);
    void applyBackgroundPreset(const BackgroundPreset& preset);
    int activePresetIndex() const;
    void applyClock24Hour(bool enabled);
    void applyUiScale(int percent);
    int activeUiScaleIndex() const;
    void applyWallpaperPath();
    void clearWallpaperPath();
    int scrollAreaHeight() const;
    int computeContentHeight() const;
    void clampScrollOffset();
    void syncWallpaperBufferFromShell();

    static constexpr int kPresetCount = 6;
    static constexpr std::array<BackgroundPreset, kPresetCount> kBackgroundPresets{{
        {"Default", 25, 25, 30},
        {"Deep Blue", 18, 24, 42},
        {"Slate", 32, 36, 48},
        {"Forest", 20, 32, 24},
        {"Wine", 36, 20, 28},
        {"Teal", 16, 28, 32},
    }};

    static constexpr int kUiScaleCount = 3;
    static constexpr std::array<UiScaleOption, kUiScaleCount> kUiScaleOptions{{
        {"Small (90%)", 90},
        {"Default (100%)", 100},
        {"Large (115%)", 115},
    }};

    TTF_Font* m_font = nullptr;
    monolith::fs::Filesystem* m_fs = nullptr;

    std::vector<InfoLine> m_lines;
    std::array<SDL_Rect, kPresetCount> m_backgroundSwatches{};
    std::array<SDL_Rect, 2> m_clockFormatHitRects{}; // 0 = 12-hour, 1 = 24-hour
    std::array<SDL_Rect, kUiScaleCount> m_uiScaleHitRects{};

    SDL_Rect m_wallpaperFieldRect{0, 0, 0, 0}; // client-local hit rect
    SDL_Rect m_wallpaperSetRect{0, 0, 0, 0};
    SDL_Rect m_wallpaperClearRect{0, 0, 0, 0};
    std::string m_wallpaperEditBuffer;
    std::size_t m_wallpaperCursorPos = 0;
    int m_wallpaperScrollPx = 0;
    bool m_wallpaperFieldFocused = false;
    bool m_wallpaperBufferSynced = false;

    int m_contentHeight = 0;
    int m_scrollOffset = 0;

    int m_clientWidth = 0;
    int m_clientHeight = 0;
};

} // namespace monolith::app
