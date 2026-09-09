#include "SettingsApp.hpp"

#include <algorithm>
#include <cctype>

namespace monolith::app {

namespace {
constexpr int kPadX = 16;
constexpr int kPadY = 12;
constexpr int kLineH = 20;
constexpr int kSwatchSize = 22;
constexpr int kSwatchGap = 8;
constexpr int kFooterHeight = 28;
constexpr int kScrollStep = 24;
constexpr int kFieldH = 24;
constexpr int kBtnPadX = 10;

bool pointInRect(int x, int y, const SDL_Rect& rect) {
    return x >= rect.x && x < rect.x + rect.w && y >= rect.y && y < rect.y + rect.h;
}

void drawLabel(SDL_Renderer* renderer, TTF_Font* font, const char* text,
               SDL_Color color, int screenX, int screenY) {
    if (!font || !text) return;
    SDL_Surface* surf = TTF_RenderUTF8_Blended(font, text, color);
    if (!surf) return;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
    if (tex) {
        SDL_Rect dst = {screenX, screenY, surf->w, surf->h};
        SDL_RenderCopy(renderer, tex, nullptr, &dst);
        SDL_DestroyTexture(tex);
    }
    SDL_FreeSurface(surf);
}

int measureTextWidth(TTF_Font* font, const char* text) {
    if (!font || !text) return 0;
    int w = 0;
    int h = 0;
    if (TTF_SizeUTF8(font, text, &w, &h) != 0) return 0;
    return w;
}
} // namespace

SettingsApp::SettingsApp(TTF_Font* font, monolith::fs::Filesystem* fs)
    : m_font(font), m_fs(fs)
{
    buildInfoLines();
}

void SettingsApp::buildInfoLines() {
    m_lines.clear();

    m_lines.push_back({"", "MONOLITH"});
    m_lines.push_back({"Version", "0.1 (June 2026)"});
    m_lines.push_back({"Engine", "SDL2 + custom window manager"});
    m_lines.push_back({"", ""});

    m_lines.push_back({"", "ENVIRONMENT"});
    m_lines.push_back({"Logical desktop", "1280 × 720"});
    if (m_fs) {
        m_lines.push_back({"Filesystem root", m_fs->hostRoot()});
    } else {
        m_lines.push_back({"Filesystem root", "(not available)"});
    }
    m_lines.push_back({"Virtual home", "/home/monolith"});
    m_lines.push_back({"", ""});

    m_lines.push_back({"", "NOTES"});
    m_lines.push_back({"Status", "Personal environment - early development"});
    m_lines.push_back({"", "Use the Start menu or taskbar to launch apps."});
}

int SettingsApp::scrollAreaHeight() const {
    return std::max(0, m_clientHeight - kFooterHeight);
}

void SettingsApp::clampScrollOffset() {
    const int viewport = scrollAreaHeight();
    const int maxScroll = std::max(0, m_contentHeight - viewport);
    if (m_scrollOffset < 0) m_scrollOffset = 0;
    if (m_scrollOffset > maxScroll) m_scrollOffset = maxScroll;
}

int SettingsApp::computeContentHeight() const {
    int y = kPadY;

    // Appearance section (background swatches + wallpaper + clock + text size)
    y += kLineH + 2 + 8;
    y += kLineH + 6;
    y += kSwatchSize + 8;
    y += kLineH + 8;

    // Wallpaper block
    y += kLineH + 6;
    y += kFieldH + 8;
    y += kLineH + 8;

    // Clock block
    y += kLineH + 6;
    y += kSwatchSize + 8;
    y += kLineH + 8;
    y += kLineH + 6;
    y += kSwatchSize + 8;
    y += kLineH + 8;
    y += 4;

    for (const auto& line : m_lines) {
        if (line.label.empty() && line.value.empty()) {
            y += 4;
            continue;
        }

        if (line.label.empty() && !line.value.empty()) {
            bool isHeader = true;
            for (char c : line.value) {
                if (c >= 'a' && c <= 'z') {
                    isHeader = false;
                    break;
                }
            }
            y += isHeader ? (kLineH + 2 + 4) : kLineH;
            continue;
        }

        y += kLineH;
    }

    return y + kPadY;
}

int SettingsApp::activePresetIndex() const {
    auto* ctrl = getController();
    if (!ctrl) return 0;

    uint8_t r = 0;
    uint8_t g = 0;
    uint8_t b = 0;
    ctrl->getDesktopBackgroundColor(r, g, b);

    for (int i = 0; i < kPresetCount; ++i) {
        const auto& preset = kBackgroundPresets[static_cast<size_t>(i)];
        if (preset.r == r && preset.g == g && preset.b == b) {
            return i;
        }
    }
    return -1;
}

void SettingsApp::applyBackgroundPreset(const BackgroundPreset& preset) {
    if (auto* ctrl = getController()) {
        ctrl->setDesktopBackgroundColor(preset.r, preset.g, preset.b);
    }
}

void SettingsApp::applyClock24Hour(bool enabled) {
    if (auto* ctrl = getController()) {
        ctrl->setClock24Hour(enabled);
    }
}

void SettingsApp::applyUiScale(int percent) {
    if (auto* ctrl = getController()) {
        ctrl->setUiScalePercent(percent);
    }
}

int SettingsApp::activeUiScaleIndex() const {
    const auto* ctrl = getController();
    const int current = ctrl ? ctrl->getUiScalePercent() : 100;
    for (int i = 0; i < kUiScaleCount; ++i) {
        if (kUiScaleOptions[static_cast<size_t>(i)].percent == current) {
            return i;
        }
    }
    return 1;
}

void SettingsApp::syncWallpaperBufferFromShell() {
    if (m_wallpaperFieldFocused) return;
    auto* ctrl = getController();
    if (!ctrl) {
        m_wallpaperEditBuffer.clear();
        return;
    }
    m_wallpaperEditBuffer = ctrl->getWallpaperPath();
    m_wallpaperBufferSynced = true;
}

void SettingsApp::applyWallpaperPath() {
    if (auto* ctrl = getController()) {
        // Trim trailing whitespace
        std::string path = m_wallpaperEditBuffer;
        while (!path.empty() && std::isspace(static_cast<unsigned char>(path.back()))) {
            path.pop_back();
        }
        size_t start = 0;
        while (start < path.size() && std::isspace(static_cast<unsigned char>(path[start]))) {
            ++start;
        }
        path = path.substr(start);
        ctrl->setWallpaperPath(path);
        m_wallpaperEditBuffer = ctrl->getWallpaperPath();
        m_wallpaperFieldFocused = false;
    }
}

void SettingsApp::clearWallpaperPath() {
    m_wallpaperEditBuffer.clear();
    if (auto* ctrl = getController()) {
        ctrl->setWallpaperPath("");
    }
    m_wallpaperFieldFocused = false;
}

int SettingsApp::renderAppearanceSection(SDL_Renderer* renderer, const SDL_Rect& contentRect, int clientY) {
    if (!m_font) return clientY;

    SDL_Color headerCol = {255, 255, 255, 255};
    SDL_Color labelCol  = {200, 200, 210, 255};
    SDL_Color dimCol    = {160, 160, 170, 255};
    SDL_Color valueCol  = {230, 230, 240, 255};

    int y = clientY;

    drawLabel(renderer, m_font, "APPEARANCE", headerCol,
              contentRect.x + kPadX, contentRect.y + y);
    y += kLineH + 2;

    SDL_SetRenderDrawColor(renderer, 70, 70, 80, 255);
    SDL_RenderDrawLine(renderer,
        contentRect.x + kPadX,
        contentRect.y + y - 2,
        contentRect.x + contentRect.w - kPadX,
        contentRect.y + y - 2);
    y += 8;

    drawLabel(renderer, m_font, "Desktop background:", labelCol,
              contentRect.x + kPadX, contentRect.y + y);
    y += kLineH + 6;

    const int swatchRowY = y;
    int swatchX = kPadX;
    const int active = activePresetIndex();

    for (int i = 0; i < kPresetCount; ++i) {
        const auto& preset = kBackgroundPresets[static_cast<size_t>(i)];
        SDL_Rect swatch = {
            contentRect.x + swatchX,
            contentRect.y + swatchRowY,
            kSwatchSize,
            kSwatchSize
        };
        m_backgroundSwatches[static_cast<size_t>(i)] = {
            swatchX,
            swatchRowY,
            kSwatchSize,
            kSwatchSize
        };

        SDL_SetRenderDrawColor(renderer, preset.r, preset.g, preset.b, 255);
        SDL_RenderFillRect(renderer, &swatch);

        if (i == active) {
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            SDL_RenderDrawRect(renderer, &swatch);
            SDL_Rect inner = {swatch.x + 1, swatch.y + 1, swatch.w - 2, swatch.h - 2};
            SDL_RenderDrawRect(renderer, &inner);
        } else {
            SDL_SetRenderDrawColor(renderer, 90, 90, 100, 255);
            SDL_RenderDrawRect(renderer, &swatch);
        }

        swatchX += kSwatchSize + kSwatchGap;
    }

    y = swatchRowY + kSwatchSize + 8;

    drawLabel(renderer, m_font, "Background presets.", dimCol,
              contentRect.x + kPadX, contentRect.y + y);
    y += kLineH + 8;

    // === Wallpaper ===
    if (!m_wallpaperBufferSynced || !m_wallpaperFieldFocused) {
        syncWallpaperBufferFromShell();
    }

    drawLabel(renderer, m_font, "Wallpaper image:", labelCol,
              contentRect.x + kPadX, contentRect.y + y);
    y += kLineH + 6;

    const int setW = measureTextWidth(m_font, "Set") + kBtnPadX * 2;
    const int clearW = measureTextWidth(m_font, "Clear") + kBtnPadX * 2;
    const int btnGap = 6;
    const int rightReserve = setW + clearW + btnGap * 2;
    const int fieldW = std::max(80, contentRect.w - kPadX * 2 - rightReserve);

    m_wallpaperFieldRect = {kPadX, y, fieldW, kFieldH};
    m_wallpaperSetRect = {kPadX + fieldW + btnGap, y, setW, kFieldH};
    m_wallpaperClearRect = {kPadX + fieldW + btnGap + setW + btnGap, y, clearW, kFieldH};

    // Field background
    {
        SDL_Rect field = {
            contentRect.x + m_wallpaperFieldRect.x,
            contentRect.y + m_wallpaperFieldRect.y,
            m_wallpaperFieldRect.w,
            m_wallpaperFieldRect.h
        };
        SDL_SetRenderDrawColor(renderer, 30, 30, 36, 255);
        SDL_RenderFillRect(renderer, &field);
        if (m_wallpaperFieldFocused) {
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
        } else {
            SDL_SetRenderDrawColor(renderer, 90, 90, 100, 255);
        }
        SDL_RenderDrawRect(renderer, &field);

        std::string display = m_wallpaperEditBuffer;
        if (display.empty() && !m_wallpaperFieldFocused) {
            display = "(none)";
        }
        if (m_wallpaperFieldFocused) {
            display += "_";
        }

        // Clip text into the field
        SDL_Rect clip = field;
        clip.x += 4;
        clip.w -= 8;
        clip.y += 2;
        clip.h -= 4;
        SDL_RenderSetClipRect(renderer, &clip);
        drawLabel(renderer, m_font, display.c_str(),
                  display == "(none)" ? dimCol : valueCol,
                  field.x + 6, field.y + (kFieldH - kLineH) / 2 + 2);
        // Restore outer clip (appearance section lives inside Settings scroll clip)
        SDL_Rect outerClip = {
            contentRect.x,
            contentRect.y,
            contentRect.w,
            scrollAreaHeight()
        };
        SDL_RenderSetClipRect(renderer, &outerClip);
    }

    auto drawButton = [&](const SDL_Rect& local, const char* label) {
        SDL_Rect btn = {
            contentRect.x + local.x,
            contentRect.y + local.y,
            local.w,
            local.h
        };
        SDL_SetRenderDrawColor(renderer, 40, 40, 48, 255);
        SDL_RenderFillRect(renderer, &btn);
        SDL_SetRenderDrawColor(renderer, 90, 90, 100, 255);
        SDL_RenderDrawRect(renderer, &btn);
        const int tw = measureTextWidth(m_font, label);
        drawLabel(renderer, m_font, label, labelCol,
                  btn.x + (btn.w - tw) / 2,
                  btn.y + (kFieldH - kLineH) / 2 + 2);
    };
    drawButton(m_wallpaperSetRect, "Set");
    drawButton(m_wallpaperClearRect, "Clear");

    y += kFieldH + 8;

    drawLabel(renderer, m_font, "BMP path in the virtual FS (e.g. /Wallpapers/sample.bmp).", dimCol,
              contentRect.x + kPadX, contentRect.y + y);
    y += kLineH + 8;

    // === Clock ===
    drawLabel(renderer, m_font, "Taskbar clock:", labelCol,
              contentRect.x + kPadX, contentRect.y + y);
    y += kLineH + 6;

    const bool use24 = getController() ? getController()->getClock24Hour() : false;
    const char* optionLabels[2] = {"12-hour", "24-hour"};
    int optionX = kPadX;
    const int optionH = kSwatchSize;
    const int optionPadX = 10;

    for (int i = 0; i < 2; ++i) {
        SDL_Surface* optSurf = TTF_RenderUTF8_Blended(m_font, optionLabels[i], labelCol);
        const int textW = optSurf ? optSurf->w : 60;
        const int textH = optSurf ? optSurf->h : kLineH;
        const int optionW = textW + optionPadX * 2;

        SDL_Rect opt = {
            contentRect.x + optionX,
            contentRect.y + y,
            optionW,
            optionH
        };
        m_clockFormatHitRects[static_cast<size_t>(i)] = {
            optionX,
            y,
            optionW,
            optionH
        };

        const bool isActive = (i == 1) == use24;
        SDL_SetRenderDrawColor(renderer, 40, 40, 48, 255);
        SDL_RenderFillRect(renderer, &opt);
        if (isActive) {
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            SDL_RenderDrawRect(renderer, &opt);
            SDL_Rect inner = {opt.x + 1, opt.y + 1, opt.w - 2, opt.h - 2};
            SDL_RenderDrawRect(renderer, &inner);
        } else {
            SDL_SetRenderDrawColor(renderer, 90, 90, 100, 255);
            SDL_RenderDrawRect(renderer, &opt);
        }

        if (optSurf) {
            SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, optSurf);
            if (tex) {
                SDL_Rect dst = {
                    opt.x + optionPadX,
                    opt.y + (optionH - textH) / 2,
                    textW,
                    textH
                };
                SDL_RenderCopy(renderer, tex, nullptr, &dst);
                SDL_DestroyTexture(tex);
            }
            SDL_FreeSurface(optSurf);
        }

        optionX += optionW + kSwatchGap;
    }

    y += optionH + 8;

    drawLabel(renderer, m_font, "12-hour (default) or 24-hour time on the taskbar.", dimCol,
              contentRect.x + kPadX, contentRect.y + y);

    y += kLineH + 8;

    drawLabel(renderer, m_font, "Interface text size:", labelCol,
              contentRect.x + kPadX, contentRect.y + y);
    y += kLineH + 6;

    int scaleX = kPadX;
    const int activeScale = activeUiScaleIndex();
    const int scaleOptionH = kSwatchSize;
    const int scaleOptionPadX = 10;
    for (int i = 0; i < kUiScaleCount; ++i) {
        const auto& option = kUiScaleOptions[static_cast<size_t>(i)];
        SDL_Surface* optSurf = TTF_RenderUTF8_Blended(m_font, option.label, labelCol);
        const int textW = optSurf ? optSurf->w : 80;
        const int textH = optSurf ? optSurf->h : kLineH;
        const int optionW = textW + scaleOptionPadX * 2;

        SDL_Rect opt = {
            contentRect.x + scaleX,
            contentRect.y + y,
            optionW,
            scaleOptionH
        };
        m_uiScaleHitRects[static_cast<size_t>(i)] = {
            scaleX,
            y,
            optionW,
            scaleOptionH
        };

        const bool isActive = i == activeScale;
        SDL_SetRenderDrawColor(renderer, 40, 40, 48, 255);
        SDL_RenderFillRect(renderer, &opt);
        if (isActive) {
            SDL_SetRenderDrawColor(renderer, 255, 255, 255, 255);
            SDL_RenderDrawRect(renderer, &opt);
            SDL_Rect inner = {opt.x + 1, opt.y + 1, opt.w - 2, opt.h - 2};
            SDL_RenderDrawRect(renderer, &inner);
        } else {
            SDL_SetRenderDrawColor(renderer, 90, 90, 100, 255);
            SDL_RenderDrawRect(renderer, &opt);
        }

        if (optSurf) {
            SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, optSurf);
            if (tex) {
                SDL_Rect dst = {
                    opt.x + scaleOptionPadX,
                    opt.y + (scaleOptionH - textH) / 2,
                    textW,
                    textH
                };
                SDL_RenderCopy(renderer, tex, nullptr, &dst);
                SDL_DestroyTexture(tex);
            }
            SDL_FreeSurface(optSurf);
        }

        scaleX += optionW + kSwatchGap;
    }

    y += scaleOptionH + 8;

    drawLabel(renderer, m_font, "Applies immediately and persists for the next launch.", dimCol,
              contentRect.x + kPadX, contentRect.y + y);

    return y + kLineH + 8;
}

int SettingsApp::renderInfoLines(SDL_Renderer* renderer, const SDL_Rect& contentRect, int clientY) {
    if (!m_font) return clientY;

    int y = clientY;

    SDL_Color headerCol = {255, 255, 255, 255};
    SDL_Color labelCol  = {200, 200, 210, 255};
    SDL_Color valueCol  = {230, 230, 240, 255};
    SDL_Color dimCol    = {160, 160, 170, 255};

    for (const auto& line : m_lines) {
        if (line.label.empty() && !line.value.empty()) {
            bool isHeader = true;
            for (char c : line.value) {
                if (c >= 'a' && c <= 'z') {
                    isHeader = false;
                    break;
                }
            }

            if (isHeader) {
                SDL_Surface* s = TTF_RenderUTF8_Blended(m_font, line.value.c_str(), headerCol);
                if (s) {
                    SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
                    if (t) {
                        SDL_Rect dst = {contentRect.x + kPadX, contentRect.y + y, s->w, s->h};
                        SDL_RenderCopy(renderer, t, nullptr, &dst);
                        SDL_DestroyTexture(t);
                    }
                    SDL_FreeSurface(s);
                }
                y += kLineH + 2;
                SDL_SetRenderDrawColor(renderer, 70, 70, 80, 255);
                SDL_RenderDrawLine(renderer,
                    contentRect.x + kPadX,
                    contentRect.y + y - 2,
                    contentRect.x + contentRect.w - kPadX,
                    contentRect.y + y - 2);
                y += 4;
            } else {
                SDL_Surface* s = TTF_RenderUTF8_Blended(m_font, line.value.c_str(), dimCol);
                if (s) {
                    SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
                    if (t) {
                        SDL_Rect dst = {contentRect.x + kPadX, contentRect.y + y, s->w, s->h};
                        if (dst.x + dst.w > contentRect.x + contentRect.w - kPadX) {
                            dst.w = contentRect.x + contentRect.w - kPadX - dst.x;
                        }
                        SDL_RenderCopy(renderer, t, nullptr, &dst);
                        SDL_DestroyTexture(t);
                    }
                    SDL_FreeSurface(s);
                }
                y += kLineH;
            }
            continue;
        }

        if (line.label.empty() && line.value.empty()) {
            y += 4;
            continue;
        }

        std::string left = line.label + ":";
        SDL_Surface* lab = TTF_RenderUTF8_Blended(m_font, left.c_str(), labelCol);
        SDL_Surface* val = TTF_RenderUTF8_Blended(m_font, line.value.c_str(), valueCol);

        int textY = y + (kLineH - (lab ? lab->h : kLineH)) / 2;

        if (lab) {
            SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, lab);
            if (t) {
                SDL_Rect dst = {contentRect.x + kPadX, contentRect.y + textY, lab->w, lab->h};
                SDL_RenderCopy(renderer, t, nullptr, &dst);
                SDL_DestroyTexture(t);
            }
            SDL_FreeSurface(lab);
        }

        if (val) {
            SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, val);
            if (t) {
                int labelWidth = 140;
                SDL_Rect dst = {
                    contentRect.x + kPadX + labelWidth,
                    contentRect.y + textY,
                    val->w,
                    val->h
                };
                if (dst.x + dst.w > contentRect.x + contentRect.w - kPadX) {
                    dst.w = contentRect.x + contentRect.w - kPadX - dst.x;
                }
                SDL_RenderCopy(renderer, t, nullptr, &dst);
                SDL_DestroyTexture(t);
            }
            SDL_FreeSurface(val);
        }

        y += kLineH;
    }

    return y + kPadY;
}

void SettingsApp::renderFooter(SDL_Renderer* renderer, const SDL_Rect& contentRect) {
    if (!m_font || m_clientHeight <= 0) return;

    SDL_Color dimCol = {160, 160, 170, 255};
    const char* hint = "Changes take effect immediately.";
    SDL_Surface* s = TTF_RenderUTF8_Blended(m_font, hint, dimCol);
    if (!s) return;

    SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
    if (t) {
        int hx = contentRect.x + kPadX;
        int hy = contentRect.y + m_clientHeight - s->h - 8;
        SDL_Rect dst = {hx, hy, s->w, s->h};
        if (dst.x + dst.w > contentRect.x + contentRect.w - kPadX) {
            dst.w = (contentRect.x + contentRect.w - kPadX) - dst.x;
        }
        SDL_RenderCopy(renderer, t, nullptr, &dst);
        SDL_DestroyTexture(t);
    }
    SDL_FreeSurface(s);

    const int separatorY = m_clientHeight - kFooterHeight;
    SDL_SetRenderDrawColor(renderer, 55, 55, 62, 255);
    SDL_RenderDrawLine(renderer,
        contentRect.x + kPadX,
        contentRect.y + separatorY,
        contentRect.x + contentRect.w - kPadX,
        contentRect.y + separatorY);
}

void SettingsApp::render(SDL_Renderer* renderer, const SDL_Rect& contentRect) {
    if (!m_font) return;

    m_contentHeight = computeContentHeight();
    clampScrollOffset();

    const int viewport = scrollAreaHeight();
    SDL_Rect clip = {
        contentRect.x,
        contentRect.y,
        contentRect.w,
        viewport
    };
    SDL_RenderSetClipRect(renderer, &clip);

    int clientY = kPadY - m_scrollOffset;
    clientY = renderAppearanceSection(renderer, contentRect, clientY);
    clientY += 4;
    renderInfoLines(renderer, contentRect, clientY);

    SDL_RenderSetClipRect(renderer, nullptr);
    renderFooter(renderer, contentRect);
}

void SettingsApp::handleEvent(const SDL_Event& event) {
    if (m_wallpaperFieldFocused) {
        if (event.type == SDL_TEXTINPUT) {
            const char* text = event.text.text;
            for (const char* p = text; *p; ++p) {
                const unsigned char c = static_cast<unsigned char>(*p);
                if (c >= 32 && c != 127) {
                    m_wallpaperEditBuffer.push_back(static_cast<char>(c));
                }
            }
            return;
        }
        if (event.type == SDL_KEYDOWN) {
            switch (event.key.keysym.sym) {
                case SDLK_RETURN:
                case SDLK_KP_ENTER:
                    applyWallpaperPath();
                    return;
                case SDLK_ESCAPE:
                    m_wallpaperFieldFocused = false;
                    syncWallpaperBufferFromShell();
                    return;
                case SDLK_BACKSPACE:
                    if (!m_wallpaperEditBuffer.empty()) {
                        m_wallpaperEditBuffer.pop_back();
                    }
                    return;
                default:
                    break;
            }
        }
    }

    if (event.type == SDL_MOUSEWHEEL) {
        m_scrollOffset -= event.wheel.y * kScrollStep;
        clampScrollOffset();
        return;
    }

    if (event.type == SDL_KEYDOWN && !m_wallpaperFieldFocused) {
        switch (event.key.keysym.sym) {
            case SDLK_PAGEUP:
                m_scrollOffset -= kScrollStep * 3;
                clampScrollOffset();
                return;
            case SDLK_PAGEDOWN:
                m_scrollOffset += kScrollStep * 3;
                clampScrollOffset();
                return;
            case SDLK_HOME:
                m_scrollOffset = 0;
                return;
            case SDLK_END:
                m_scrollOffset = std::max(0, m_contentHeight - scrollAreaHeight());
                return;
            default:
                break;
        }
    }

    if (event.type != SDL_MOUSEBUTTONDOWN || event.button.button != SDL_BUTTON_LEFT) {
        return;
    }

    const int x = event.button.x;
    const int y = event.button.y + m_scrollOffset;

    for (int i = 0; i < kPresetCount; ++i) {
        if (pointInRect(x, y, m_backgroundSwatches[static_cast<size_t>(i)])) {
            m_wallpaperFieldFocused = false;
            applyBackgroundPreset(kBackgroundPresets[static_cast<size_t>(i)]);
            return;
        }
    }

    if (pointInRect(x, y, m_wallpaperSetRect)) {
        applyWallpaperPath();
        return;
    }
    if (pointInRect(x, y, m_wallpaperClearRect)) {
        clearWallpaperPath();
        return;
    }
    if (pointInRect(x, y, m_wallpaperFieldRect)) {
        m_wallpaperFieldFocused = true;
        if (m_wallpaperEditBuffer.empty()) {
            syncWallpaperBufferFromShell();
            // When focusing an empty "(none)" display, start with empty buffer for typing.
            if (auto* ctrl = getController()) {
                m_wallpaperEditBuffer = ctrl->getWallpaperPath();
            }
        }
        return;
    }

    // Click elsewhere in the panel drops wallpaper focus.
    if (m_wallpaperFieldFocused) {
        m_wallpaperFieldFocused = false;
        syncWallpaperBufferFromShell();
    }

    for (int i = 0; i < 2; ++i) {
        if (pointInRect(x, y, m_clockFormatHitRects[static_cast<size_t>(i)])) {
            applyClock24Hour(i == 1);
            return;
        }
    }

    for (int i = 0; i < kUiScaleCount; ++i) {
        if (pointInRect(x, y, m_uiScaleHitRects[static_cast<size_t>(i)])) {
            applyUiScale(kUiScaleOptions[static_cast<size_t>(i)].percent);
            return;
        }
    }
}

void SettingsApp::onResize(int clientWidth, int clientHeight) {
    m_clientWidth = clientWidth;
    m_clientHeight = clientHeight;
    clampScrollOffset();
}

} // namespace monolith::app
