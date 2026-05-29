#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <iostream>

#include "window/WindowManager.hpp"

int main(int /*argc*/, char* /*argv*/[])
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::cerr << "SDL_Init Error: " << SDL_GetError() << std::endl;
        return 1;
    }

    if (TTF_Init() != 0) {
        std::cerr << "TTF_Init Error: " << TTF_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        "Monolith",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        1280, 720,
        SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE
    );

    if (!window) {
        std::cerr << "SDL_CreateWindow Error: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }

    // Set a reasonable minimum size for the application window (like Linux DEs)
    SDL_SetWindowMinimumSize(window, 800, 600);

    SDL_Renderer* renderer = SDL_CreateRenderer(
        window,
        -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );

    if (!renderer) {
        std::cerr << "SDL_CreateRenderer Error: " << SDL_GetError() << std::endl;
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    // === Window Manager Setup ===
    monolith::window::WindowManager wm;

    // Load font for window titles
    // TODO: Move this path to a config or make it more robust later
    const char* fontPath = "assets/fonts/DejaVuSans.ttf";
    TTF_Font* titleFont = TTF_OpenFont(fontPath, 14);
    if (!titleFont) {
        std::cerr << "Warning: Failed to load font at '" << fontPath << "'\n"
                  << "         Window titles will not render until a font is provided.\n"
                  << "         Error: " << TTF_GetError() << std::endl;
    } else {
        wm.setFont(titleFont);
    }

    // Create some test windows so we can see the system working
    wm.createWindow("Terminal", 100, 100, 500, 350);
    wm.createWindow("Filesystem", 300, 180, 420, 280);
    wm.createWindow("Editor", 550, 80, 480, 400);

    // Track current desktop size (adapts when user resizes the application window)
    int desktopWidth = 1280;
    int desktopHeight = 720;
    wm.setDesktopSize(desktopWidth, desktopHeight);

    bool running = true;
    SDL_Event event;

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            }

            // Handle outer window resizing
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_RESIZED) {
                desktopWidth = event.window.data1;
                desktopHeight = event.window.data2;
                wm.setDesktopSize(desktopWidth, desktopHeight);
            }

            // Pass events to the Window Manager
            wm.handleEvent(event);
        }

        wm.update();

        // Clear background (desktop color)
        SDL_SetRenderDrawColor(renderer, 25, 25, 30, 255);
        SDL_RenderClear(renderer);

        // Render all windows (title bars + content areas)
        wm.render(renderer);

        SDL_RenderPresent(renderer);
    }

    if (titleFont) {
        TTF_CloseFont(titleFont);
    }
    TTF_Quit();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
