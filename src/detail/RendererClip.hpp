#pragma once

#include <SDL2/SDL.h>

namespace monolith::detail {

struct RendererClipState {
    SDL_Rect rect{};
    bool active = false;
};

struct RendererBlendState {
    SDL_BlendMode mode = SDL_BLENDMODE_NONE;
};

struct RendererDrawColorState {
    Uint8 r = 0;
    Uint8 g = 0;
    Uint8 b = 0;
    Uint8 a = 255;
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

inline RendererBlendState captureRendererBlend(SDL_Renderer* renderer) {
    RendererBlendState state;
    SDL_GetRenderDrawBlendMode(renderer, &state.mode);
    return state;
}

inline void restoreRendererBlend(SDL_Renderer* renderer,
                                 const RendererBlendState& state) {
    SDL_SetRenderDrawBlendMode(renderer, state.mode);
}

inline RendererDrawColorState captureRendererDrawColor(SDL_Renderer* renderer) {
    RendererDrawColorState state;
    SDL_GetRenderDrawColor(renderer, &state.r, &state.g, &state.b, &state.a);
    return state;
}

inline void restoreRendererDrawColor(SDL_Renderer* renderer,
                                     const RendererDrawColorState& state) {
    SDL_SetRenderDrawColor(renderer, state.r, state.g, state.b, state.a);
}

} // namespace monolith::detail
