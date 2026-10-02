#pragma once

#include <SDL.h>
#include <string>

namespace monolith::window {

/**
 * Load a wallpaper image from a host filesystem path into a new SDL_Surface.
 * Supports BMP via SDL_LoadBMP and PNG/JPEG via pinned stb_image fetch.
 * PNG/JPEG inputs above 16,777,216 pixels are rejected before decode.
 * Caller owns the surface. Returns nullptr on failure (fail soft).
 */
SDL_Surface* loadWallpaperSurface(const std::string& hostPath);

} // namespace monolith::window
