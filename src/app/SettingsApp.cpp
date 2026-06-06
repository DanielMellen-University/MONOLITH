#include "SettingsApp.hpp"

#include <sstream>

namespace monolith::app {

SettingsApp::SettingsApp(TTF_Font* font, monolith::fs::Filesystem* fs)
    : m_font(font), m_fs(fs)
{
    buildInfoLines();
}

void SettingsApp::buildInfoLines() {
    m_lines.clear();

    // About
    m_lines.push_back({"", "MONOLITH"});
    m_lines.push_back({"Version", "0.1 (June 2026)"});
    m_lines.push_back({"Engine", "SDL2 + custom window manager"});
    m_lines.push_back({"", ""});  // spacer

    // Environment
    m_lines.push_back({"", "ENVIRONMENT"});
    m_lines.push_back({"Logical desktop", "1280 × 720"});
    if (m_fs) {
        m_lines.push_back({"Filesystem root", m_fs->hostRoot()});
    } else {
        m_lines.push_back({"Filesystem root", "(not available)"});
    }
    m_lines.push_back({"Virtual home", "/home/monolith"});
    m_lines.push_back({"", ""});

    // Notes
    m_lines.push_back({"", "NOTES"});
    m_lines.push_back({"Status", "Personal environment - early development"});
    m_lines.push_back({"", "Use the Start menu or taskbar to"});
    m_lines.push_back({"", "launch apps."});
}

void SettingsApp::render(SDL_Renderer* renderer, const SDL_Rect& contentRect) {
    if (!m_font) return;

    const int padX = 16;
    const int padY = 12;
    const int lineH = 20;

    int y = contentRect.y + padY;

    SDL_Color headerCol = {255, 255, 255, 255};
    SDL_Color labelCol  = {200, 200, 210, 255};
    SDL_Color valueCol  = {230, 230, 240, 255};
    SDL_Color dimCol    = {160, 160, 170, 255};

    for (const auto& line : m_lines) {
        if (line.label.empty() && line.value.empty()) {
            y += 4; // small spacer line
            continue;
        }

        if (line.label.empty() && !line.value.empty()) {
            // Distinguish real section headers (all caps like "MONOLITH", "NOTES")
            // from body/note text (sentence case). Body text should not get underline
            // or header styling, and long lines would previously truncate badly.
            bool isHeader = true;
            for (char c : line.value) {
                if (c >= 'a' && c <= 'z') {
                    isHeader = false;
                    break;
                }
            }

            if (isHeader) {
                // Section header (white + underline)
                SDL_Surface* s = TTF_RenderUTF8_Blended(m_font, line.value.c_str(), headerCol);
                if (s) {
                    SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
                    if (t) {
                        SDL_Rect dst = {contentRect.x + padX, y, s->w, s->h};
                        SDL_RenderCopy(renderer, t, nullptr, &dst);
                        SDL_DestroyTexture(t);
                    }
                    SDL_FreeSurface(s);
                }
                y += lineH + 2;
                // subtle underline
                SDL_SetRenderDrawColor(renderer, 70, 70, 80, 255);
                SDL_RenderDrawLine(renderer,
                    contentRect.x + padX,
                    y - 2,
                    contentRect.x + contentRect.w - padX,
                    y - 2);
                y += 4;
            } else {
                // Body / note text (dim color, no underline, left aligned)
                SDL_Surface* s = TTF_RenderUTF8_Blended(m_font, line.value.c_str(), dimCol);
                if (s) {
                    SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
                    if (t) {
                        SDL_Rect dst = {contentRect.x + padX, y, s->w, s->h};
                        // clamp if somehow still too wide
                        if (dst.x + dst.w > contentRect.x + contentRect.w - padX) {
                            dst.w = contentRect.x + contentRect.w - padX - dst.x;
                        }
                        SDL_RenderCopy(renderer, t, nullptr, &dst);
                        SDL_DestroyTexture(t);
                    }
                    SDL_FreeSurface(s);
                }
                y += lineH;
            }
            continue;
        }

        // label + value row
        std::string left = line.label + ":";
        SDL_Surface* lab = TTF_RenderUTF8_Blended(m_font, left.c_str(), labelCol);
        SDL_Surface* val = TTF_RenderUTF8_Blended(m_font, line.value.c_str(), valueCol);

        int textY = y + (lineH - (lab ? lab->h : lineH)) / 2;

        if (lab) {
            SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, lab);
            if (t) {
                SDL_Rect dst = {contentRect.x + padX, textY, lab->w, lab->h};
                SDL_RenderCopy(renderer, t, nullptr, &dst);
                SDL_DestroyTexture(t);
            }
            SDL_FreeSurface(lab);
        }

        if (val) {
            SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, val);
            if (t) {
                int labelWidth = 140; // rough alignment
                SDL_Rect dst = {contentRect.x + padX + labelWidth, textY, val->w, val->h};
                // clamp to content
                if (dst.x + dst.w > contentRect.x + contentRect.w - padX) {
                    dst.w = contentRect.x + contentRect.w - padX - dst.x;
                }
                SDL_RenderCopy(renderer, t, nullptr, &dst);
                SDL_DestroyTexture(t);
            }
            SDL_FreeSurface(val);
        }

        y += lineH;
    }

    // Bottom hint
    if (m_font) {
        SDL_Color hintCol = dimCol;
        const char* hint = "Changes take effect immediately where applicable.";
        SDL_Surface* s = TTF_RenderUTF8_Blended(m_font, hint, hintCol);
        if (s) {
            SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
            if (t) {
                int hx = contentRect.x + padX;
                int hy = contentRect.y + contentRect.h - s->h - 8;
                SDL_Rect dst = {hx, hy, s->w, s->h};
                if (dst.x + dst.w > contentRect.x + contentRect.w - padX) {
                    dst.w = (contentRect.x + contentRect.w - padX) - dst.x;
                }
                SDL_RenderCopy(renderer, t, nullptr, &dst);
                SDL_DestroyTexture(t);
            }
            SDL_FreeSurface(s);
        }
    }
}

void SettingsApp::handleEvent(const SDL_Event& /*event*/) {
    // Read-only info panel for now. No interactive controls.
}

void SettingsApp::onResize(int clientWidth, int clientHeight) {
    m_clientWidth = clientWidth;
    m_clientHeight = clientHeight;
}

} // namespace monolith::app
