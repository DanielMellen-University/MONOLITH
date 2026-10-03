#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include <cstddef>
#include <cstdint>
#include <functional>
#include <iterator>
#include <list>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

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

        const KeyView lookup{font, text, color};
        auto cached = m_entries.find(lookup);
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

        CacheKey key{font, std::string(text), color};
        auto inserted = m_entries.emplace(std::move(key), Entry{
            texture, width, height, estimatedBytes, {}});
        if (!inserted.second) {
            SDL_DestroyTexture(texture);
            return {};
        }
        m_lru.push_front(KeyView{font, inserted.first->first.text, color});
        inserted.first->second.lruPosition = m_lru.begin();
        m_estimatedBytes += estimatedBytes;
        trim();
        return {texture, width, height};
    }

    void clear() const {
        m_lru.clear();
        for (auto& [key, entry] : m_entries) {
            (void)key;
            SDL_DestroyTexture(entry.handle);
        }
        m_entries.clear();
        m_estimatedBytes = 0;
        m_renderer = nullptr;
    }

    std::size_t size() const { return m_entries.size(); }
    std::uint64_t estimatedBytes() const { return m_estimatedBytes; }

private:
    struct CacheKey {
        TTF_Font* font = nullptr;
        std::string text;
        SDL_Color color{};
    };

    struct KeyView {
        TTF_Font* font = nullptr;
        std::string_view text;
        SDL_Color color{};
    };

    struct KeyHash {
        using is_transparent = void;

        template <typename Key>
        std::size_t operator()(const Key& key) const noexcept {
            std::size_t seed = std::hash<TTF_Font*>{}(key.font);
            combine(seed, std::hash<std::string_view>{}(
                std::string_view(key.text)));
            const std::uint32_t packedColor =
                (static_cast<std::uint32_t>(key.color.r) << 24)
                | (static_cast<std::uint32_t>(key.color.g) << 16)
                | (static_cast<std::uint32_t>(key.color.b) << 8)
                | static_cast<std::uint32_t>(key.color.a);
            combine(seed, std::hash<std::uint32_t>{}(packedColor));
            return seed;
        }

    private:
        static void combine(std::size_t& seed, std::size_t value) noexcept {
            seed ^= value + static_cast<std::size_t>(0x9e3779b9)
                + (seed << 6) + (seed >> 2);
        }
    };

    struct KeyEqual {
        using is_transparent = void;

        template <typename Left, typename Right>
        bool operator()(const Left& left, const Right& right) const noexcept {
            return left.font == right.font
                && std::string_view(left.text) == std::string_view(right.text)
                && left.color.r == right.color.r
                && left.color.g == right.color.g
                && left.color.b == right.color.b
                && left.color.a == right.color.a;
        }
    };

    struct Entry {
        SDL_Texture* handle = nullptr;
        int width = 0;
        int height = 0;
        std::uint64_t estimatedBytes = 0;
        std::list<KeyView>::iterator lruPosition;
    };

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
            }
            m_lru.erase(oldest);
            if (entry != m_entries.end()) m_entries.erase(entry);
        }
    }

    mutable SDL_Renderer* m_renderer = nullptr;
    mutable std::unordered_map<CacheKey, Entry, KeyHash, KeyEqual> m_entries;
    // Views point into immutable map keys; rehash preserves references to elements.
    mutable std::list<KeyView> m_lru;
    mutable std::uint64_t m_estimatedBytes = 0;
};

} // namespace monolith::detail
