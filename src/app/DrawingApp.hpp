#pragma once

#include "App.hpp"
#include "../fs/Filesystem.hpp"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <cstdint>
#include <string>
#include <vector>

namespace monolith::app {

/**
 * A simple native drawing program.
 * Supports pen/eraser tools, color and brush size selection, and save/load
 * of drawings to the internal filesystem (.modr format).
 */
class DrawingApp : public App {
public:
    DrawingApp(TTF_Font* font, monolith::fs::Filesystem* fs = nullptr);
    ~DrawingApp() override;

    void render(SDL_Renderer* renderer, const SDL_Rect& contentRect) override;
    void handleEvent(const SDL_Event& event) override;
    void onResize(int clientWidth, int clientHeight) override;

private:
    enum class Tool { Pen, Eraser };
    enum class BrushSize { Small, Medium, Large };
    enum class PathPromptMode { None, Save, Open };

    struct ColorSwatch {
        const char* name;
        uint8_t r, g, b;
    };

    struct CanvasSnapshot {
        int width = 0;
        int height = 0;
        std::vector<uint8_t> pixels;
    };

    // === Canvas ===
    void resizeCanvas(int width, int height, bool preserveContent);
    void clearCanvas(bool recordUndo = true);
    void markTextureDirty();
    void syncTexture(SDL_Renderer* renderer);
    void pushUndoSnapshot();
    void restoreCanvasSnapshot(const CanvasSnapshot& snapshot);
    void undoCanvas();
    void redoCanvas();
    void setPixel(int x, int y, uint8_t r, uint8_t g, uint8_t b);
    void stampBrush(int x, int y);
    void drawStroke(int x0, int y0, int x1, int y1);
    int brushRadius() const;
    uint8_t activeRed() const;
    uint8_t activeGreen() const;
    uint8_t activeBlue() const;

    // === File I/O ===
    bool saveToPath(const std::string& virtualPath);
    bool loadFromPath(const std::string& virtualPath);
    std::string defaultSavePath();

    // === UI ===
    void drawToolbar(SDL_Renderer* renderer, const SDL_Rect& contentRect);
    void drawStatusBar(SDL_Renderer* renderer, const SDL_Rect& contentRect);
    void handleToolbarClick(int x, int y);
    bool isInCanvas(int x, int y) const;
    void canvasPointFromClient(int clientX, int clientY, int& outX, int& outY) const;
    void setStatus(const std::string& message);
    void beginPathPrompt(PathPromptMode mode);
    void finishPathPrompt(bool commit);
    void handlePathPromptKey(const SDL_Keysym& keysym);
    void handlePathPromptText(const char* text);

    TTF_Font* m_font = nullptr;
    monolith::fs::Filesystem* m_fs = nullptr;

    std::vector<uint8_t> m_pixels; // R,G,B,A byte order per pixel
    std::vector<CanvasSnapshot> m_undoStack;
    std::vector<CanvasSnapshot> m_redoStack;
    int m_canvasWidth = 0;
    int m_canvasHeight = 0;
    SDL_Texture* m_canvasTexture = nullptr;
    bool m_textureDirty = true;

    Tool m_tool = Tool::Pen;
    BrushSize m_brush = BrushSize::Medium;
    int m_colorIndex = 0;
    static constexpr ColorSwatch kColors[] = {
        {"Black",  20,  20,  24},
        {"White",  245, 245, 248},
        {"Red",    220, 70,  70},
        {"Green",  70,  180, 90},
        {"Blue",   70,  120, 220},
        {"Yellow", 230, 200, 60},
        {"Orange", 230, 140, 50},
        {"Purple", 150, 80,  200},
    };
    static constexpr int kColorCount = 8;

    bool m_drawing = false;
    int m_lastCanvasX = -1;
    int m_lastCanvasY = -1;

    std::string m_filePath;
    bool m_dirty = false;

    int m_clientWidth = 0;
    int m_clientHeight = 0;
    int m_canvasTop = 40;
    int m_statusBarHeight = 22;

    std::string m_statusMessage;

    PathPromptMode m_pathPromptMode = PathPromptMode::None;
    std::string m_pathPromptBuffer;

    // Toolbar hit areas (client-relative coordinates)
    SDL_Rect m_btnPen{0, 0, 0, 0};
    SDL_Rect m_btnEraser{0, 0, 0, 0};
    SDL_Rect m_btnClear{0, 0, 0, 0};
    SDL_Rect m_btnBrushSmall{0, 0, 0, 0};
    SDL_Rect m_btnBrushMedium{0, 0, 0, 0};
    SDL_Rect m_btnBrushLarge{0, 0, 0, 0};
    SDL_Rect m_colorSwatches[kColorCount]{};
};

} // namespace monolith::app