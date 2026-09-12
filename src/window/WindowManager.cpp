#include "WindowManager.hpp"
#include "WallpaperImage.hpp"
#include "SessionFormat.hpp"
#include "../app/App.hpp"
#include "../app/FilePath.hpp"
#include "../app/TerminalApp.hpp"
#include "../app/TextEditorApp.hpp"
#include "../app/FilesystemApp.hpp"
#include "../app/SettingsApp.hpp"
#include "../app/DrawingApp.hpp"
#include "../app/SnakeApp.hpp"
#include "../app/MinesweeperApp.hpp"
#include "../app/PongApp.hpp"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <ctime>
#include <cmath>
#include <fstream>
#include <sstream>

namespace monolith::window {

namespace {

struct RendererClipState {
    SDL_Rect rect{};
    bool active = false;
};

RendererClipState captureRendererClip(SDL_Renderer* renderer) {
    RendererClipState state;
    SDL_RenderGetClipRect(renderer, &state.rect);
    state.active = state.rect.w > 0 && state.rect.h > 0;
    return state;
}

void restoreRendererClip(SDL_Renderer* renderer, const RendererClipState& state) {
    SDL_RenderSetClipRect(renderer, state.active ? &state.rect : nullptr);
}

SDL_Rect intersectRendererClip(const SDL_Rect& requested, const RendererClipState& state) {
    SDL_Rect result = requested;
    if (state.active) {
        SDL_IntersectRect(&state.rect, &requested, &result);
    }
    return result;
}

#include "detail/wm_body_01.inc"
#include "detail/wm_body_02.inc"
#include "detail/wm_body_03.inc"
#include "detail/wm_body_04.inc"
#include "detail/wm_body_05.inc"
#include "detail/wm_body_06.inc"
#include "detail/wm_body_07.inc"
#include "detail/wm_body_08.inc"
#include "detail/wm_body_09.inc"
