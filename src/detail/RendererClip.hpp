#pragma once

#include <SDL2/SDL.h>

namespace monolith::detail {

struct RendererClipState {
    SDL_Rect rect{};
    bool active = false;
};

inline RendererClipState captureRendererClip(SDL_Renderer* renderer) {
    RendererClipState state;
    SDL_RenderGetClipRect(renderer, &state.rect);
    state.active = state.rect.w > 0 && state.rect.h > 0;
    return state;
}

inline void restoreRendererClip(SDL_Renderer* renderer,
                                const RendererClipState& state) {
    SDL_RenderSetClipRect(renderer, state.active ? &state.rect : nullptr);
}

inline SDL_Rect intersectRendererClip(const SDL_Rect& requested,
                                      const RendererClipState& state) {
    SDL_Rect result = requested;
    if (state.active) {
        SDL_IntersectRect(&state.rect, &requested, &result);
    }
    return result;
}

} // namespace monolith::detail
