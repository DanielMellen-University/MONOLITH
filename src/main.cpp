#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <filesystem>
#include <iostream>
#include <vector>

#include "window/WindowManager.hpp"
#include "app/App.hpp"
#include "app/TerminalApp.hpp"
#include "app/TextEditorApp.hpp"
#include "app/FilesystemApp.hpp"
#include "app/SettingsApp.hpp"
#include "fs/Filesystem.hpp"

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

    // Fixed comfortable size (similar to the original default).
    // No extra GNOME header compensation added to the window height anymore —
    // the internal content will start right at the top of the SDL client area
    // (underneath GNOME's title bar). We can tune this later if needed.
    const int LOGICAL_WIDTH = 1280;
    const int LOGICAL_HEIGHT = 720;

    const int WINDOW_WIDTH  = LOGICAL_WIDTH;
    const int WINDOW_HEIGHT = LOGICAL_HEIGHT;   // Clean 1280x720 outer window

    // Outer application window is fixed size. There is deliberately no resizing logic for it.
    SDL_Window* window = SDL_CreateWindow(
        "Monolith",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH,
        WINDOW_HEIGHT,
        SDL_WINDOW_SHOWN
    );

    if (!window) {
        std::cerr << "SDL_CreateWindow Error: " << SDL_GetError() << std::endl;
        SDL_Quit();
        return 1;
    }

    // Explicitly tell SDL/WM this window must not be resizable (more reliable than just omitting the flag on Linux)
    SDL_SetWindowResizable(window, SDL_FALSE);

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

    // Create system cursors for resize feedback (we'll switch these based on hover)
    SDL_Cursor* cursorArrow     = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW);
    SDL_Cursor* cursorSizeWE    = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZEWE);    // left/right
    SDL_Cursor* cursorSizeNS    = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENS);    // top/bottom
    SDL_Cursor* cursorSizeNWSE  = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENWSE);  // top-left / bottom-right
    SDL_Cursor* cursorSizeNESW  = SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENESW);  // top-right / bottom-left

    // === Window Manager Setup ===
    monolith::window::WindowManager wm;

    // Load font for window titles — try several common locations
    TTF_Font* titleFont = nullptr;
    const char* fontCandidates[] = {
        "assets/fonts/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/dejavu/DejaVuSans.ttf",
        "/usr/share/fonts/truetype/DejaVuSans.ttf",
        nullptr
    };

    for (int i = 0; fontCandidates[i] && !titleFont; ++i) {
        titleFont = TTF_OpenFont(fontCandidates[i], 14);
        if (titleFont) {
            std::cout << "Loaded font from: " << fontCandidates[i] << std::endl;
        }
    }

    if (!titleFont) {
        std::cerr << "Warning: Could not find DejaVuSans.ttf in any standard location.\n"
                  << "         Window titles will not render. Copy a .ttf font to assets/fonts/ or install fonts-dejavu.\n";
    } else {
        wm.setFont(titleFont);
    }

    // === Filesystem (shared with Terminal) ===
    const char* home = std::getenv("HOME");
    std::string fsRoot = home ? std::string(home) + "/.monolith/fs" : "./monolith_fs";
    monolith::fs::Filesystem monolithFs(fsRoot);
    bool fsReady = monolithFs.initialize();
    if (fsReady) {
        std::cout << "Filesystem initialized at: " << monolithFs.hostRoot() << std::endl;
        monolithFs.createDirectory("/home/monolith");
        monolithFs.createDirectory("/home/monolith/documents");
        monolithFs.createDirectory("/home/monolith/drawings");
        if (!monolithFs.exists("/home/monolith/welcome.txt")) {
            monolithFs.writeFile("/home/monolith/welcome.txt", "Welcome to Monolith's filesystem!\n");
        }
    } else {
        std::cerr << "Failed to initialize filesystem at: " << fsRoot << std::endl;
    }

    // Resources must be set before using any launch* helpers (they need the font and FS pointer).
    wm.setAppResources(titleFont, fsReady ? &monolithFs : nullptr);

    wm.setLogicalDesktopSize(LOGICAL_WIDTH, LOGICAL_HEIGHT);
    wm.setHeaderOffset(0);   // No artificial offset — content starts right at top of client area

    {
        std::string settingsHostPath = home ? std::string(home) + "/.monolith/desktop_settings.txt" : "./desktop_settings.txt";
        std::error_code ec;
        std::filesystem::create_directories(
            std::filesystem::path(settingsHostPath).parent_path(),
            ec
        );
        wm.loadDesktopSettings(settingsHostPath);
    }

    // Launch initial demo windows through the desktop shell launchers.
    // This exercises the proper app creation paths (including Settings skeleton) and
    // gives consistent titling / positioning / inter-app coordination.
    if (titleFont) {
        wm.launchTerminal();
        wm.launchFilesystem();
        // Open the welcome file through the normal launcher path (singleton editor tracking).
        wm.launchTextEditor("/home/monolith/welcome.txt");
        wm.launchSettings();
    } else {
        // Fallback (rare): plain windows without apps if font/launcher resources unavailable.
        wm.createWindow("Terminal", 100, 100, 520, 380);
        wm.createWindow("Filesystem", 300, 180, 460, 320);
        wm.launchTextEditor("/home/monolith/welcome.txt");
    }
    // No content scaling for now — logical size matches the window content area 1:1.

    bool running = true;
    SDL_Event event;

    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = false;
            }

            // Pass events to the Window Manager (internal windows only)
            wm.handleEvent(event);

            if (wm.shouldQuit()) {
                running = false;
            }

            // Update mouse cursor based on hover over internal window resize zones
            if (event.type == SDL_MOUSEMOTION) {
                auto dir = wm.getResizeDirectionAt(event.motion.x, event.motion.y);

                SDL_Cursor* targetCursor = cursorArrow;

                switch (dir) {
                    case monolith::window::ResizeDirection::Left:
                    case monolith::window::ResizeDirection::Right:
                        targetCursor = cursorSizeWE;
                        break;
                    case monolith::window::ResizeDirection::Top:
                    case monolith::window::ResizeDirection::Bottom:
                        targetCursor = cursorSizeNS;
                        break;
                    case monolith::window::ResizeDirection::TopLeft:
                    case monolith::window::ResizeDirection::BottomRight:
                        targetCursor = cursorSizeNWSE;
                        break;
                    case monolith::window::ResizeDirection::TopRight:
                    case monolith::window::ResizeDirection::BottomLeft:
                        targetCursor = cursorSizeNESW;
                        break;
                    default:
                        targetCursor = cursorArrow;
                        break;
                }

                SDL_SetCursor(targetCursor);
            }
        }

        wm.update();

        // Clear background (desktop color from shell settings)
        {
            auto bg = wm.getDesktopBackground();
            SDL_SetRenderDrawColor(renderer, bg.r, bg.g, bg.b, 255);
        }
        SDL_RenderClear(renderer);

        // Render all windows (title bars + content areas)
        wm.render(renderer);

        SDL_RenderPresent(renderer);
    }

    if (titleFont) {
        TTF_CloseFont(titleFont);
    }
    TTF_Quit();

    // Clean up custom cursors
    SDL_FreeCursor(cursorArrow);
    SDL_FreeCursor(cursorSizeWE);
    SDL_FreeCursor(cursorSizeNS);
    SDL_FreeCursor(cursorSizeNWSE);
    SDL_FreeCursor(cursorSizeNESW);

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}
