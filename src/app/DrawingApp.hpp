#pragma once

#include "App.hpp"
#include "../detail/TextTextureCache.hpp"
#include "../fs/Filesystem.hpp"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <cstddef>
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
    DrawingApp(TTF_Font* font, monolith::fs::Filesystem* fs = nullptr,
               const std::string& initialPath = "");
    ~DrawingApp() override;

    void render(SDL_Renderer* renderer, const SDL_Rect& contentRect) override;
    void handleEvent(const SDL_Event& event) override;
    void onFocusLost() override;
    void onResize(int clientWidth, int clientHeight) override;
    void onUiScaleChanged() override;
    void onBoundFileMoved(const std::string& oldPath,
                          const std::string& newPath) override;
    void onVirtualPathMoved(const std::string& oldPath,
                            const std::string& newPath) override;
    void onVirtualPathChanged(const std::string& changedPath) override;
    void onBoundFileRemoved(const std::string& removedPath) override;
    bool allowClose() override;

    // True only after a .modr file has loaded successfully.
    bool hasFilePath() const { return !m_filePath.empty(); }

private:
    enum class Tool { Pen, Eraser, Fill, Eyedropper, Line, Rect };
    enum class BrushSize { Small, Medium, Large };
    enum class PathPromptMode { None, Save, Open, Rgb };
    enum class DiscardKind { None, Close, New, Open };

    bool requestDiscard(DiscardKind kind, const char* statusMessage,
                        bool explicitlyConfirmed = false);
    void clearDiscardArm();
    void startNewSketch(bool explicitlyConfirmed = false);
    void completePendingSaveAction();

    struct ColorSwatch {
        const char* name;
        uint8_t r, g, b;
    };

    struct CanvasSnapshot {
        int width = 0;
        int height = 0;
        std::vector<uint8_t> pixels;
    };

    struct CanvasTileSnapshot {
        int x = 0;
        int y = 0;
        int width = 0;
        int height = 0;
        std::vector<uint8_t> pixels;
    };

    struct CanvasHistoryEntry {
        int width = 0;
        int height = 0;
        std::size_t pixelBytes = 0;
        std::vector<CanvasTileSnapshot> tiles;
    };

    // === Canvas ===
    static constexpr int kHistoryTileSize = 32;
    void resizeCanvas(int width, int height, bool preserveContent);
    void captureSavedSnapshot();
    void clearCanvas(bool recordUndo = true);
    void markTextureDirty();
    void markTextureRegionDirty(int x, int y, int width, int height);
    void syncTexture(SDL_Renderer* renderer);
    void pushUndoHistoryEntry(CanvasHistoryEntry entry);
    void clearHistory();
    void beginSparseHistory();
    void recordSparseHistoryChange();
    void finishSparseHistory();
    void endActiveStroke();
    static void observeSparseHistoryPixelWrite(void* context, int x, int y);
    void captureSparseHistoryTile(int x, int y);
    void toggleSparseHistoryTiles(CanvasHistoryEntry& entry);
    void undoCanvas();
    void redoCanvas();
    void refreshDirtyState();
    void rebuildDirtyTiles();
    void clearDirtyTiles();
    void markAllDirtyTiles();
    void markDirtyTile(std::size_t index);
    void refreshDirtyTile(const CanvasTileSnapshot& tile);
    void updateDirtyFlag();
    void beginDirtyTileTracking();
    static void observeFillSpanWrite(void* context, int y, int left, int right);
    void noteDirtyTileWrite(int x, int y);
    void noteDirtySpanWrite(int y, int left, int right);
    void finishDirtyTileTracking();
    bool setPixel(int x, int y, uint8_t r, uint8_t g, uint8_t b);
    bool stampBrush(int x, int y);
    bool drawStroke(int x0, int y0, int x1, int y1);
    bool commitShape(int x0, int y0, int x1, int y1);
    void floodFill(int x, int y);
    void pickColorAt(int x, int y);
    int brushRadius() const;
    uint8_t activeRed() const;
    uint8_t activeGreen() const;
    uint8_t activeBlue() const;

    // === File I/O ===
    bool saveToPath(const std::string& virtualPath,
                    bool confirmedExternalOverwrite = false);
    bool savedCanvasMatchesFile(const std::string& path, bool& matches);
    bool loadFromPath(const std::string& virtualPath);
    std::string defaultSavePath();

    // === UI ===
    void drawToolbar(SDL_Renderer* renderer, const SDL_Rect& contentRect);
    void drawStatusBar(SDL_Renderer* renderer, const SDL_Rect& contentRect);
    int measurePromptCursorWidth(const std::string& text);
    int getToolbarButtonHeight() const;
    int getToolbarHeight() const;
    int getStatusBarHeight() const;
    void updateLayoutMetrics();
    void ensureHitTargets();
    void invalidateHitTargets();
    void handleToolbarClick(int x, int y);
    bool isInCanvas(int x, int y) const;
    void canvasPointFromClient(int clientX, int clientY, int& outX, int& outY) const;
    void setStatus(const std::string& message);
    void beginPathPrompt(PathPromptMode mode);
    void remapPathPrompt(const std::string& oldPath,
                         const std::string& newPath);
    void finishPathPrompt(bool commit, bool confirmDiscard = false);
    void completePathPrompt();
    void handlePathPromptKey(const SDL_Keysym& keysym);
    void handlePathPromptText(const char* text);

    TTF_Font* m_font = nullptr;
    monolith::fs::Filesystem* m_fs = nullptr;
    mutable monolith::detail::TextTextureCache m_textTextureCache;

    std::vector<uint8_t> m_pixels; // R,G,B,A byte order per pixel
    CanvasSnapshot m_savedSnapshot;
    std::vector<uint8_t> m_dirtyTiles;
    std::size_t m_dirtyTileColumns = 0;
    std::size_t m_dirtyTileCount = 0;
    int m_dirtyTileWidth = 0;
    int m_dirtyTileHeight = 0;
    std::vector<uint8_t> m_dirtyTrackingTiles;
    std::vector<std::size_t> m_dirtyTrackedTileIndices;
    std::vector<CanvasHistoryEntry> m_undoStack;
    std::vector<CanvasHistoryEntry> m_redoStack;
    std::size_t m_undoHistoryBytes = 0;
    std::size_t m_redoHistoryBytes = 0;
    int m_canvasWidth = 0;
    int m_canvasHeight = 0;
    SDL_Texture* m_canvasTexture = nullptr;
    bool m_textureDirty = true;
    SDL_Rect m_dirtyTextureRect{};

    Tool m_tool = Tool::Pen;
    BrushSize m_brush = BrushSize::Medium;
    int m_colorIndex = 0;
    bool m_usingCustomColor = false;
    uint8_t m_customR = 220;
    uint8_t m_customG = 70;
    uint8_t m_customB = 70;
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
    int m_shapeAnchorX = -1;
    int m_shapeAnchorY = -1;
    bool m_sparseHistoryPending = false;
    bool m_sparseHistoryChanged = false;
    CanvasHistoryEntry m_sparseHistoryEntry;
    std::vector<uint8_t> m_sparseHistoryCapturedTiles;
    std::vector<std::size_t> m_sparseHistoryCapturedTileIndices;
    std::size_t m_sparseHistoryTileColumns = 0;
    std::size_t m_sparseHistoryBytes = 0;
    bool m_sparseHistoryOverflowed = false;

    std::string m_filePath;
    bool m_hasSavedFileBaseline = false;
    monolith::fs::FileStamp m_savedFileStamp;
    bool m_hasSavedFileStamp = false;
    bool m_dirty = false;
    bool m_externalChangePending = false;
    bool m_overwriteConfirmationPending = false;
    bool m_suppressChangedNotification = false;
    DiscardKind m_discardKind = DiscardKind::None;
    std::string m_discardPath;
    bool m_closeDiscardAuthorized = false;
    bool m_closeAfterSave = false;
    bool m_newAfterSave = false;

    int m_clientWidth = 0;
    int m_clientHeight = 0;
    int m_canvasTop = 96;
    int m_statusBarHeight = 22;

    std::string m_statusMessage;
    std::string m_renderStatusText;

    PathPromptMode m_pathPromptMode = PathPromptMode::None;
    std::string m_pathPromptBuffer;
    std::size_t m_pathPromptCursorPos = 0;
    int m_pathPromptScrollPx = 0;
    std::string m_promptCursorMeasureText;
    int m_promptCursorPixelWidth = 0;
    bool m_promptCursorMeasureValid = false;
    std::string m_pendingInitialPath;

    // Toolbar hit areas (client-relative coordinates)
    SDL_Rect m_btnNew{0, 0, 0, 0};
    SDL_Rect m_btnSave{0, 0, 0, 0};
    SDL_Rect m_btnOpen{0, 0, 0, 0};
    SDL_Rect m_btnUndo{0, 0, 0, 0};
    SDL_Rect m_btnRedo{0, 0, 0, 0};
    SDL_Rect m_btnPen{0, 0, 0, 0};
    SDL_Rect m_btnEraser{0, 0, 0, 0};
    SDL_Rect m_btnFill{0, 0, 0, 0};
    SDL_Rect m_btnEyedropper{0, 0, 0, 0};
    SDL_Rect m_btnLine{0, 0, 0, 0};
    SDL_Rect m_btnRect{0, 0, 0, 0};
    SDL_Rect m_btnRgb{0, 0, 0, 0};
    SDL_Rect m_btnClear{0, 0, 0, 0};
    SDL_Rect m_btnBrushSmall{0, 0, 0, 0};
    SDL_Rect m_btnBrushMedium{0, 0, 0, 0};
    SDL_Rect m_btnBrushLarge{0, 0, 0, 0};
    SDL_Rect m_colorSwatches[kColorCount]{};
    bool m_hitTargetsValid = false;
};

} // namespace monolith::app
