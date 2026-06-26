#include "SettingsApp.hpp"

#include <algorithm>

namespace monolith::app {

namespace {
constexpr int kPadX = 16;
constexpr int kPadY = 12;
constexpr int kLineH = 20;
constexpr int kSwatchSize = 22;
constexpr int kSwatchGap = 8;
constexpr int kFooterHeight = 28;
constexpr int kScrollStep = 24;

bool pointInRect(int x, int y, const SDL_Rect& rect) {
    return x >= rect.x && x < rect.x + rect.w && y >= rect.y && y < rect.y + rect.h;
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

    // Appearance section
    y += kLineH + 2 + 8;
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

int SettingsApp::renderAppearanceSection(SDL_Renderer* renderer, const SDL_Rect& contentRect, int clientY) {
    if (!m_font) return clientY;

    SDL_Color headerCol = {255, 255, 255, 255};
    SDL_Color labelCol  = {200, 200, 210, 255};
    SDL_Color dimCol    = {160, 160, 170, 255};

    int y = clientY;

    SDL_Surface* header = TTF_RenderUTF8_Blended(m_font, "APPEARANCE", headerCol);
    if (header) {
        SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, header);
        if (tex) {
            SDL_Rect dst = {contentRect.x + kPadX, contentRect.y + y, header->w, header->h};
            SDL_RenderCopy(renderer, tex, nullptr, &dst);
            SDL_DestroyTexture(tex);
        }
        SDL_FreeSurface(header);
    }
    y += kLineH + 2;

    SDL_SetRenderDrawColor(renderer, 70, 70, 80, 255);
    SDL_RenderDrawLine(renderer,
        contentRect.x + kPadX,
        contentRect.y + y - 2,
        contentRect.x + contentRect.w - kPadX,
        contentRect.y + y - 2);
    y += 8;

    SDL_Surface* label = TTF_RenderUTF8_Blended(m_font, "Desktop background:", labelCol);
    if (label) {
        SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, label);
        if (tex) {
            SDL_Rect dst = {contentRect.x + kPadX, contentRect.y + y, label->w, label->h};
            SDL_RenderCopy(renderer, tex, nullptr, &dst);
            SDL_DestroyTexture(tex);
        }
        SDL_FreeSurface(label);
    }
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

    const char* hint = "Background presets.";
    SDL_Surface* hintSurf = TTF_RenderUTF8_Blended(m_font, hint, dimCol);
    if (hintSurf) {
        SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, hintSurf);
        if (tex) {
            SDL_Rect dst = {contentRect.x + kPadX, contentRect.y + y, hintSurf->w, hintSurf->h};
            SDL_RenderCopy(renderer, tex, nullptr, &dst);
            SDL_DestroyTexture(tex);
        }
        SDL_FreeSurface(hintSurf);
    }

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
    if (event.type == SDL_MOUSEWHEEL) {
        m_scrollOffset -= event.wheel.y * kScrollStep;
        clampScrollOffset();
        return;
    }

    if (event.type == SDL_KEYDOWN) {
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
            applyBackgroundPreset(kBackgroundPresets[static_cast<size_t>(i)]);
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