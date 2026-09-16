#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <string>
#include <unordered_map>
#include <utility>

namespace monolith::detail {

class TextSurfaceCache {
public:
    TextSurfaceCache() = default;
    ~TextSurfaceCache() { clear(); }

    TextSurfaceCache(const TextSurfaceCache&) = delete;
    TextSurfaceCache& operator=(const TextSurfaceCache&) = delete;

    SDL_Surface* get(TTF_Font* font, const char* text, SDL_Color color) const {
        if (!font || !text || !*text) return nullptr;

        std::string key(text);
        key.push_back('\0');
        key.push_back(static_cast<char>(color.r));
        key.push_back(static_cast<char>(color.g));
        key.push_back(static_cast<char>(color.b));
        key.push_back(static_cast<char>(color.a));

        auto cached = m_entries.find(key);
        if (cached != m_entries.end()) return cached->second;

        SDL_Surface* surface = TTF_RenderUTF8_Blended(font, text, color);
        if (!surface) return nullptr;
        m_entries.emplace(std::move(key), surface);
        return surface;
    }

    void clear() {
        for (auto& [key, surface] : m_entries) {
            (void)key;
            SDL_FreeSurface(surface);
        }
        m_entries.clear();
    }

private:
    mutable std::unordered_map<std::string, SDL_Surface*> m_entries;
};

} // namespace monolith::detail
