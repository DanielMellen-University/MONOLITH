#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <list>
#include <string>
#include <unordered_map>

namespace monolith::detail {

class TextTextureCache {
public:
    static constexpr std::size_t kMaxEntries = 256;
    static constexpr std::size_t kMaxEstimatedBytes = 16u * 1024u * 1024u;

    struct Texture {
        SDL_Texture* handle = nullptr;
        int width = 0;
        int height = 0;

        explicit operator bool() const { return handle != nullptr; }
    };

    TextTextureCache() = default;
    ~TextTextureCache() { clear(); }

    TextTextureCache(const TextTextureCache&) = delete;
    TextTextureCache& operator=(const TextTextureCache&) = delete;

    Texture get(SDL_Renderer* renderer, TTF_Font* font, const char* text,
                SDL_Color color) const {
        if (!renderer || !font || !text || !*text) return {};

        if (m_renderer != renderer) {
            clear();
            m_renderer = renderer;
        }

        std::string key = makeKey(font, text, color);
        auto cached = m_entries.find(key);
        if (cached != m_entries.end()) {
            m_lru.splice(m_lru.begin(), m_lru, cached->second.lruPosition);
            cached->second.lruPosition = m_lru.begin();
            return {cached->second.handle, cached->second.width, cached->second.height};
        }

        SDL_Surface* surface = TTF_RenderUTF8_Blended(font, text, color);
        if (!surface) return {};

        const int width = surface->w;
        const int height = surface->h;
        const std::uint64_t estimatedBytes = estimateBytes(width, height);
        SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);
        SDL_FreeSurface(surface);
        if (!texture) return {};

        m_lru.push_front(key);
        auto inserted = m_entries.emplace(m_lru.front(), Entry{
            texture, width, height, estimatedBytes, m_lru.begin()});
        if (!inserted.second) {
            m_lru.pop_front();
            SDL_DestroyTexture(texture);
            return {};
        }
        m_estimatedBytes += estimatedBytes;
        trim();
        return {texture, width, height};
    }

    void clear() const {
        for (auto& [key, entry] : m_entries) {
            (void)key;
            SDL_DestroyTexture(entry.handle);
        }
        m_entries.clear();
        m_lru.clear();
        m_estimatedBytes = 0;
        m_renderer = nullptr;
    }

    std::size_t size() const { return m_entries.size(); }
    std::uint64_t estimatedBytes() const { return m_estimatedBytes; }

private:
    struct Entry {
        SDL_Texture* handle = nullptr;
        int width = 0;
        int height = 0;
        std::uint64_t estimatedBytes = 0;
        std::list<std::string>::iterator lruPosition;
    };

    static std::string makeKey(TTF_Font* font, const char* text, SDL_Color color) {
        std::string key;
        key.append(reinterpret_cast<const char*>(&font), sizeof(font));
        key.append(text);
        key.push_back('\0');
        key.push_back(static_cast<char>(color.r));
        key.push_back(static_cast<char>(color.g));
        key.push_back(static_cast<char>(color.b));
        key.push_back(static_cast<char>(color.a));
        return key;
    }

    static std::uint64_t estimateBytes(int width, int height) {
        if (width <= 0 || height <= 0) return 0;
        const std::uint64_t overBudget = kMaxEstimatedBytes + 1;
        const std::uint64_t w = static_cast<std::uint64_t>(width);
        const std::uint64_t h = static_cast<std::uint64_t>(height);
        if (w > kMaxEstimatedBytes / 4 / h) return overBudget;
        return w * h * 4;
    }

    void trim() const {
        while (m_entries.size() > kMaxEntries
               || m_estimatedBytes > kMaxEstimatedBytes) {
            if (m_entries.size() <= 1) break;
            auto oldest = std::prev(m_lru.end());
            auto entry = m_entries.find(*oldest);
            if (entry != m_entries.end()) {
                m_estimatedBytes -= entry->second.estimatedBytes;
                SDL_DestroyTexture(entry->second.handle);
                m_entries.erase(entry);
            }
            m_lru.erase(oldest);
        }
    }

    mutable SDL_Renderer* m_renderer = nullptr;
    mutable std::unordered_map<std::string, Entry> m_entries;
    mutable std::list<std::string> m_lru;
    mutable std::uint64_t m_estimatedBytes = 0;
};

} // namespace monolith::detail
