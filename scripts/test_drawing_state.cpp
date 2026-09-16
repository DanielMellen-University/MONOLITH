// Headless regression test for Drawing file state across window resizes.

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <string>
#include <unistd.h>
#include <vector>

#include "../src/app/App.hpp"
#include "../src/app/DrawingRaster.hpp"
#include "../src/fs/Filesystem.hpp"

#define private public
#include "../src/app/DrawingApp.hpp"
#undef private

namespace {

struct TestController final : monolith::app::IWindowController {
    monolith::app::DrawingApp* drawing = nullptr;
    std::string occupiedDrawingPath;
    std::vector<std::string> lifecycleEvents;

    void close() override {}
    void setTitle(const std::string&) override {}

    void bindDrawingFile(const std::string& path) override {
        lifecycleEvents.push_back("bind:" + path);
    }

    void notifyVirtualPathCreated(const std::string& path) override {
        lifecycleEvents.push_back("created:" + path);
    }

    bool focusDrawingForFile(const std::string& path) override {
        return !occupiedDrawingPath.empty() && occupiedDrawingPath == path;
    }

    void notifyVirtualPathChanged(const std::string& path) override {
        lifecycleEvents.push_back("changed:" + path);
        if (drawing) drawing->onVirtualPathChanged(path);
    }
};

struct TestDrawing final : monolith::app::DrawingApp {
    using monolith::app::App::setController;

    TestDrawing(TTF_Font* font, monolith::fs::Filesystem* fs,
                const std::string& initialPath = {})
        : DrawingApp(font, fs, initialPath) {}
};

} // namespace

int main() {
    int failures = 0;
    auto check = [&](bool ok, const char* message) {
        if (!ok) {
            std::cerr << "FAIL: " << message << '\n';
            ++failures;
        } else {
            std::cout << "ok: " << message << '\n';
        }
    };

    const std::filesystem::path hostRoot = std::filesystem::temp_directory_path()
        / ("monolith-drawing-state-" + std::to_string(getpid()));
    std::error_code ec;
    std::filesystem::remove_all(hostRoot, ec);

    monolith::fs::Filesystem fs(hostRoot.string());
    check(fs.initialize(), "drawing state filesystem initialize");
    std::vector<uint8_t> pixels(2 * 2 * 4, 255);
    check(fs.writeFile("/drawings/resize.modr",
                       monolith::drawing::encodeModr(2, 2, pixels)),
          "write resize drawing");
    check(fs.createDirectory("/drawings/unicode"),
          "create Unicode Drawing completion directory");
    check(fs.writeFile("/drawings/unicode/\xC3\xA9" "clair.modr", "placeholder"),
          "write first Unicode Drawing completion candidate");
    check(fs.writeFile("/drawings/unicode/\xC3\xAA" "cole.modr", "placeholder"),
          "write second Unicode Drawing completion candidate");

    check(SDL_Init(SDL_INIT_VIDEO) == 0, "drawing state SDL initialize");
    check(TTF_Init() == 0, "drawing state SDL_ttf initialize");
    TTF_Font* font = TTF_OpenFont("assets/fonts/DejaVuSans.ttf", 14);
    check(font != nullptr, "drawing state loads test font");
    if (!font) {
        TTF_Quit();
        SDL_Quit();
        std::filesystem::remove_all(hostRoot, ec);
        return 1;
    }

    TestDrawing failedInitialDrawing(font, &fs, "/drawings/missing.modr");
    failedInitialDrawing.onResize(300, 300);
    check(failedInitialDrawing.m_filePath.empty() && !failedInitialDrawing.m_dirty,
          "failed initial Drawing open falls back to a clean untitled canvas");
    failedInitialDrawing.pushUndoSnapshot();
    failedInitialDrawing.setPixel(0, 0, 9, 8, 7);
    failedInitialDrawing.m_dirty = true;
    failedInitialDrawing.undoCanvas();
    check(!failedInitialDrawing.m_dirty,
          "undoing after a failed initial Drawing open clears the modified state");

    TestDrawing drawing(font, &fs);
    bool occupiedSketchNames = true;
    for (int i = 1; i <= 999; ++i) {
        std::string path = "/home/monolith/drawings/sketch";
        if (i > 1) path += "_" + std::to_string(i);
        path += ".modr";
        occupiedSketchNames = fs.writeFile(path, "occupied") && occupiedSketchNames;
    }
    check(occupiedSketchNames, "occupy the first 999 default Drawing names");
    check(drawing.defaultSavePath() == "/home/monolith/drawings/sketch_1000.modr",
          "default Drawing save name continues past sketch_999");

    drawing.beginPathPrompt(monolith::app::DrawingApp::PathPromptMode::Open);
    drawing.m_pathPromptBuffer = "/drawings/unicode/";
    drawing.m_pathPromptCursorPos = drawing.m_pathPromptBuffer.size();
    drawing.completePathPrompt();
    check(drawing.m_pathPromptBuffer == "/drawings/unicode/",
          "Drawing completion does not insert a partial UTF-8 codepoint");
    drawing.finishPathPrompt(false);

    drawing.onResize(300, 300);
    check(!drawing.m_dirty, "initial blank resize stays clean");
    const size_t clearUndoCount = drawing.m_undoStack.size();
    drawing.clearCanvas();
    check(!drawing.m_dirty
              && drawing.m_undoStack.size() == clearUndoCount
              && drawing.m_statusMessage == "Canvas already clear.",
          "clearing an already blank Drawing stays clean and out of undo history");
    drawing.m_tool = monolith::app::DrawingApp::Tool::Eraser;
    const std::vector<uint8_t> blankPixels = drawing.m_pixels;
    SDL_Event noOpStrokeDown{};
    noOpStrokeDown.type = SDL_MOUSEBUTTONDOWN;
    noOpStrokeDown.button.button = SDL_BUTTON_LEFT;
    noOpStrokeDown.button.x = 1;
    noOpStrokeDown.button.y = drawing.m_canvasTop + 1;
    drawing.handleEvent(noOpStrokeDown);
    SDL_Event noOpStrokeUp = noOpStrokeDown;
    noOpStrokeUp.type = SDL_MOUSEBUTTONUP;
    drawing.handleEvent(noOpStrokeUp);
    check(!drawing.m_dirty
              && drawing.m_pixels == blankPixels
              && drawing.m_undoStack.size() == clearUndoCount,
          "an eraser stroke on a blank Drawing stays clean and out of undo history");

    TestDrawing redoDrawing(font, &fs);
    redoDrawing.onResize(300, 300);
    redoDrawing.m_tool = monolith::app::DrawingApp::Tool::Pen;
    SDL_Event changedStrokeDown = noOpStrokeDown;
    changedStrokeDown.button.x = 20;
    changedStrokeDown.button.y = redoDrawing.m_canvasTop + 20;
    redoDrawing.handleEvent(changedStrokeDown);
    SDL_Event changedStrokeUp = changedStrokeDown;
    changedStrokeUp.type = SDL_MOUSEBUTTONUP;
    redoDrawing.handleEvent(changedStrokeUp);
    check(redoDrawing.m_dirty && redoDrawing.m_undoStack.size() == 1,
          "a changed Drawing stroke records one undo state");
    redoDrawing.undoCanvas();
    check(!redoDrawing.m_dirty && redoDrawing.m_undoStack.empty()
              && redoDrawing.m_redoStack.size() == 1,
          "undoing a Drawing stroke exposes its redo state");
    redoDrawing.m_tool = monolith::app::DrawingApp::Tool::Eraser;
    changedStrokeDown.button.x = 20;
    changedStrokeDown.button.y = redoDrawing.m_canvasTop + 20;
    redoDrawing.handleEvent(changedStrokeDown);
    changedStrokeUp = changedStrokeDown;
    changedStrokeUp.type = SDL_MOUSEBUTTONUP;
    redoDrawing.handleEvent(changedStrokeUp);
    check(!redoDrawing.m_dirty && redoDrawing.m_undoStack.empty()
              && redoDrawing.m_redoStack.size() == 1,
          "a no-op Drawing stroke preserves redo history");
    redoDrawing.redoCanvas();
    check(redoDrawing.m_dirty && redoDrawing.m_redoStack.empty(),
          "preserved Drawing redo history still reapplies the stroke");

    TestDrawing capturedLine(font, &fs);
    capturedLine.onResize(300, 300);
    capturedLine.m_tool = monolith::app::DrawingApp::Tool::Line;
    SDL_Event capturedDown{};
    capturedDown.type = SDL_MOUSEBUTTONDOWN;
    capturedDown.button.button = SDL_BUTTON_LEFT;
    capturedDown.button.x = 20;
    capturedDown.button.y = capturedLine.m_canvasTop + 20;
    capturedLine.handleEvent(capturedDown);
    int expectedLineX = 0;
    int expectedLineY = 0;
    capturedLine.canvasPointFromClient(
        capturedLine.m_clientWidth + 80,
        capturedLine.m_canvasTop + 40,
        expectedLineX,
        expectedLineY);
    SDL_Event capturedMotion = capturedDown;
    capturedMotion.type = SDL_MOUSEMOTION;
    capturedMotion.motion.x = capturedLine.m_clientWidth + 80;
    capturedMotion.motion.y = capturedLine.m_canvasTop + 40;
    capturedLine.handleEvent(capturedMotion);
    check(capturedLine.m_lastCanvasX == expectedLineX
              && capturedLine.m_lastCanvasY == expectedLineY,
          "Drawing clamps captured shape motion to the canvas edge");
    SDL_Event capturedUp{};
    capturedUp.type = SDL_MOUSEBUTTONUP;
    capturedUp.button.button = SDL_BUTTON_LEFT;
    capturedUp.button.x = capturedMotion.motion.x;
    capturedUp.button.y = capturedMotion.motion.y;
    capturedLine.handleEvent(capturedUp);
    const size_t lineEdgePixel =
        (static_cast<size_t>(expectedLineY) * static_cast<size_t>(capturedLine.m_canvasWidth)
         + static_cast<size_t>(expectedLineX)) * 4;
    check(capturedLine.m_pixels[lineEdgePixel] != 245
              || capturedLine.m_pixels[lineEdgePixel + 1] != 245
              || capturedLine.m_pixels[lineEdgePixel + 2] != 248,
          "Drawing commits a captured shape at the clamped endpoint");

    TestDrawing capturedPen(font, &fs);
    capturedPen.onResize(300, 300);
    capturedPen.m_tool = monolith::app::DrawingApp::Tool::Pen;
    capturedPen.m_brush = monolith::app::DrawingApp::BrushSize::Small;
    capturedPen.handleEvent(capturedDown);
    int expectedPenX = 0;
    int expectedPenY = 0;
    capturedPen.canvasPointFromClient(
        -40,
        capturedPen.m_canvasTop + 30,
        expectedPenX,
        expectedPenY);
    SDL_Event capturedPenMotion = capturedDown;
    capturedPenMotion.type = SDL_MOUSEMOTION;
    capturedPenMotion.motion.x = -40;
    capturedPenMotion.motion.y = capturedPen.m_canvasTop + 30;
    capturedPen.handleEvent(capturedPenMotion);
    check(capturedPen.m_lastCanvasX == expectedPenX
              && capturedPen.m_lastCanvasY == expectedPenY,
          "Drawing clamps captured pen motion to the canvas edge");
    capturedUp.button.x = capturedPenMotion.motion.x;
    capturedUp.button.y = capturedPenMotion.motion.y;
    capturedPen.handleEvent(capturedUp);

    drawing.m_tool = monolith::app::DrawingApp::Tool::Pen;
    drawing.m_usingCustomColor = true;
    drawing.m_customR = 12;
    drawing.m_customG = 34;
    drawing.m_customB = 56;
    drawing.floodFill(0, 0);
    bool fillCoveredCanvas = true;
    for (size_t i = 0; i + 3 < drawing.m_pixels.size(); i += 4) {
        fillCoveredCanvas &= drawing.m_pixels[i + 0] == drawing.m_customR
            && drawing.m_pixels[i + 1] == drawing.m_customG
            && drawing.m_pixels[i + 2] == drawing.m_customB
            && drawing.m_pixels[i + 3] == 255;
    }
    check(fillCoveredCanvas, "Drawing fill covers a connected canvas without gaps");
    check(drawing.loadFromPath("/drawings/resize.modr"), "load resize drawing");
    check(!drawing.m_dirty, "loaded drawing starts clean");

    const int baseToolbarHeight = drawing.m_canvasTop;
    const int baseStatusBarHeight = drawing.m_statusBarHeight;
    const int baseCanvasWidth = drawing.m_canvasWidth;
    const int baseCanvasHeight = drawing.m_canvasHeight;
    const std::vector<uint8_t> pixelsBeforeScale = drawing.m_pixels;
    drawing.pushUndoSnapshot();
    const size_t undoCountBeforeScale = drawing.m_undoStack.size();
    drawing.m_btnNew = {10, 10, 40, 20};
    drawing.m_colorSwatches[0] = {10, 40, 12, 12};
    check(TTF_SetFontSize(font, 22) == 0, "drawing state applies larger test font");
    drawing.onUiScaleChanged();
    check(drawing.m_canvasTop > baseToolbarHeight
              && drawing.m_statusBarHeight > baseStatusBarHeight,
          "Drawing chrome grows with the shared interface font");
    check(drawing.m_canvasWidth == baseCanvasWidth
              && drawing.m_canvasHeight == baseCanvasHeight
              && drawing.m_pixels == pixelsBeforeScale
              && drawing.m_undoStack.size() == undoCountBeforeScale
              && !drawing.m_dirty,
          "Drawing text scaling preserves canvas data and history");
    check(drawing.m_btnNew.w == 0 && drawing.m_colorSwatches[0].w == 0,
          "Drawing scaling invalidates stale toolbar hit targets");
    drawing.m_btnSave = {10, 10, 40, 20};
    drawing.m_colorSwatches[1] = {10, 40, 12, 12};
    drawing.onResize(320, 300);
    check(drawing.m_btnSave.w == 0 && drawing.m_colorSwatches[1].w == 0,
          "Drawing resize invalidates stale toolbar hit targets");
    drawing.ensureHitTargets();
    const SDL_Rect scaledLineButton = drawing.m_btnLine;
    const SDL_Rect scaledBlueSwatch = drawing.m_colorSwatches[4];
    check(scaledLineButton.w > 0 && scaledBlueSwatch.w > 0,
          "Drawing rebuilds toolbar and swatch targets on demand");
    drawing.invalidateHitTargets();
    drawing.m_tool = monolith::app::DrawingApp::Tool::Pen;
    SDL_Event click{};
    click.type = SDL_MOUSEBUTTONDOWN;
    click.button.button = SDL_BUTTON_LEFT;
    click.button.x = scaledLineButton.x + scaledLineButton.w / 2;
    click.button.y = scaledLineButton.y + scaledLineButton.h / 2;
    drawing.handleEvent(click);
    check(drawing.m_tool == monolith::app::DrawingApp::Tool::Line,
          "Drawing accepts a queued scaled toolbar click before render");
    drawing.invalidateHitTargets();
    click.button.x = scaledBlueSwatch.x + scaledBlueSwatch.w / 2;
    click.button.y = scaledBlueSwatch.y + scaledBlueSwatch.h / 2;
    drawing.handleEvent(click);
    check(drawing.m_colorIndex == 4 && !drawing.m_usingCustomColor,
          "Drawing accepts a queued scaled swatch click before render");
    const int scaledDisplayHeight = drawing.m_clientHeight
        - drawing.m_canvasTop - drawing.m_statusBarHeight;
    int mappedX = 0;
    int mappedY = 0;
    drawing.canvasPointFromClient(
        drawing.m_clientWidth - 1,
        drawing.m_canvasTop + std::max(0, scaledDisplayHeight - 1),
        mappedX,
        mappedY);
    check(scaledDisplayHeight > 0
              && mappedX == drawing.m_canvasWidth - 1
              && mappedY == drawing.m_canvasHeight - 1,
          "Drawing maps scaled canvas clicks to the preserved raster edge");
    check(drawing.loadFromPath("/drawings/resize.modr"),
          "reload Drawing fixture after scaled mapping coverage");
    check(!drawing.m_dirty, "reloaded Drawing fixture starts clean");

    TestController controller;
    drawing.setController(&controller);
    controller.drawing = &drawing;
    const std::vector<uint8_t> loadedPixels = drawing.m_pixels;
    const std::vector<uint8_t> externalPixels(1 * 1 * 4, 255);
    check(fs.writeFile("/drawings/resize.modr",
                       monolith::drawing::encodeModr(1, 1, externalPixels)),
          "overwrite the Drawing file outside the app");
    drawing.onVirtualPathChanged("/drawings/resize.modr");
    check(drawing.m_pixels == loadedPixels
              && drawing.m_canvasWidth == 2
              && drawing.m_canvasHeight == 2
              && drawing.m_statusMessage.find("changed externally") != std::string::npos,
          "external overwrite warns without replacing the Drawing canvas");
    check(drawing.saveToPath("/drawings/resize.modr"),
          "Drawing save succeeds after an external overwrite");
    check(drawing.m_statusMessage == "Saved: /drawings/resize.modr"
              && !drawing.m_suppressChangedNotification,
          "Drawing ignores its own synchronous change notification");

    drawing.startNewSketch();
    check(drawing.m_filePath.empty() && !drawing.m_dirty
              && drawing.m_savedSnapshot.pixels == drawing.m_pixels,
          "New Drawing starts with a clean blank baseline");
    drawing.pushUndoSnapshot();
    drawing.setPixel(0, 0, 9, 8, 7);
    drawing.m_dirty = true;
    drawing.undoCanvas();
    check(!drawing.m_dirty,
          "undoing a new Drawing edit clears the modified state");
    drawing.redoCanvas();
    check(drawing.m_dirty,
          "redoing a new Drawing edit restores the modified state");
    check(drawing.loadFromPath("/drawings/resize.modr"),
          "reload Drawing fixture after new-sketch baseline coverage");
    check(!drawing.m_dirty, "reloaded Drawing remains clean after baseline coverage");

    drawing.pushUndoSnapshot();
    drawing.setPixel(0, 0, 1, 2, 3);
    drawing.m_dirty = true;
    drawing.undoCanvas();
    check(!drawing.m_dirty && drawing.m_pixels == loadedPixels,
          "undoing back to the saved canvas clears the modified state");
    drawing.redoCanvas();
    check(drawing.m_dirty && drawing.m_pixels[0] == 1
              && drawing.m_pixels[1] == 2 && drawing.m_pixels[2] == 3,
          "redoing an undone drawing edit restores the modified state");

    drawing.onResize(320, 300);
    check(drawing.m_dirty, "resizing a loaded drawing marks it modified");
    check(drawing.m_undoStack.empty() && drawing.m_redoStack.empty(),
          "resizing a loaded drawing clears incompatible history");

    check(fs.createDirectory("/blocked.modr"), "create blocked drawing save target");
    drawing.m_filePath = "/blocked.modr";
    drawing.m_dirty = true;
    check(!drawing.allowClose(), "first close arms the dirty drawing guard");
    check(!drawing.saveToPath("/blocked.modr"), "drawing save failure is reported");
    check(!drawing.allowClose(), "failed drawing save clears the stale dirty guard arm");

    drawing.m_dirty = true;
    drawing.beginPathPrompt(monolith::app::DrawingApp::PathPromptMode::Open);
    drawing.m_pathPromptBuffer = "/drawings/resize.modr";
    drawing.m_pathPromptCursorPos = drawing.m_pathPromptBuffer.size();
    drawing.finishPathPrompt(true);
    check(drawing.m_discardKind == monolith::app::DrawingApp::DiscardKind::Open
              && drawing.m_pathPromptMode == monolith::app::DrawingApp::PathPromptMode::Open,
          "dirty drawing open arms a discard confirmation and keeps the prompt active");
    drawing.finishPathPrompt(false);
    check(drawing.m_discardKind == monolith::app::DrawingApp::DiscardKind::None
              && drawing.m_pathPromptMode == monolith::app::DrawingApp::PathPromptMode::None,
          "canceling a dirty drawing open clears its discard arm");
    drawing.m_dirty = true;
    drawing.beginPathPrompt(monolith::app::DrawingApp::PathPromptMode::Open);
    drawing.m_pathPromptBuffer = "/drawings/resize.modr";
    drawing.m_pathPromptCursorPos = drawing.m_pathPromptBuffer.size();
    drawing.finishPathPrompt(true);
    check(drawing.m_discardKind == monolith::app::DrawingApp::DiscardKind::Open
              && drawing.m_pathPromptMode == monolith::app::DrawingApp::PathPromptMode::Open
              && drawing.m_filePath == "/blocked.modr",
          "a later dirty drawing open requires a fresh confirmation after cancellation");

    std::vector<uint8_t> alternatePixels(1 * 1 * 4, 255);
    check(fs.writeFile("/drawings/alternate.modr",
                       monolith::drawing::encodeModr(1, 1, alternatePixels)),
          "write alternate dirty drawing target");
    drawing.m_dirty = true;
    drawing.beginPathPrompt(monolith::app::DrawingApp::PathPromptMode::Open);
    drawing.m_pathPromptBuffer = "/drawings/resize.modr";
    drawing.m_pathPromptCursorPos = drawing.m_pathPromptBuffer.size();
    drawing.finishPathPrompt(true);
    drawing.m_pathPromptBuffer = "/drawings/alternate.modr";
    drawing.m_pathPromptCursorPos = drawing.m_pathPromptBuffer.size();
    drawing.finishPathPrompt(true);
    check(drawing.m_discardKind == monolith::app::DrawingApp::DiscardKind::Open
              && drawing.m_pathPromptMode == monolith::app::DrawingApp::PathPromptMode::Open
              && drawing.m_filePath == "/blocked.modr",
          "changing a dirty drawing target requires a fresh confirmation");
    drawing.finishPathPrompt(true);
    check(drawing.m_filePath == "/drawings/alternate.modr" && !drawing.m_dirty,
          "confirming the changed dirty drawing target loads it");

    std::vector<uint8_t> occupiedPixels(1 * 1 * 4, 128);
    check(fs.writeFile("/drawings/occupied.modr",
                       monolith::drawing::encodeModr(1, 1, occupiedPixels)),
          "write occupied Drawing singleton target");
    controller.occupiedDrawingPath = "/drawings/occupied.modr";
    const std::string boundPathBeforeDuplicate = drawing.m_filePath;
    drawing.beginPathPrompt(monolith::app::DrawingApp::PathPromptMode::Open);
    drawing.m_pathPromptBuffer = controller.occupiedDrawingPath;
    drawing.m_pathPromptCursorPos = drawing.m_pathPromptBuffer.size();
    drawing.finishPathPrompt(true);
    check(drawing.m_filePath == boundPathBeforeDuplicate
              && drawing.m_statusMessage == "Already open: /drawings/occupied.modr",
          "Drawing focuses an existing singleton instead of opening a duplicate");

    drawing.beginPathPrompt(monolith::app::DrawingApp::PathPromptMode::Save);
    drawing.m_pathPromptBuffer = "/drawings/occupied";
    drawing.m_pathPromptCursorPos = drawing.m_pathPromptBuffer.size();
    drawing.finishPathPrompt(true);
    check(drawing.m_filePath == boundPathBeforeDuplicate
              && drawing.m_statusMessage == "Save failed: file already open",
          "Drawing rejects Save As to an existing singleton");
    controller.occupiedDrawingPath.clear();

    drawing.beginPathPrompt(monolith::app::DrawingApp::PathPromptMode::Open);
    drawing.m_pathPromptScrollPx = 42;
    drawing.onUiScaleChanged();
    check(drawing.m_pathPromptScrollPx == 0,
          "scaling resets the Drawing path prompt offset");

    drawing.beginPathPrompt(monolith::app::DrawingApp::PathPromptMode::Save);
    drawing.m_pathPromptBuffer = "/drawings/alternate.modr/child.modr";
    drawing.m_pathPromptCursorPos = std::string("/drawings/alternate.modr/").size();
    drawing.onBoundFileMoved(
        "/drawings/alternate.modr", "/archive/../archive/alternate.modr");
    check(drawing.m_pathPromptBuffer == "/archive/alternate.modr/child.modr"
              && drawing.m_filePath == "/archive/alternate.modr"
              && drawing.m_pathPromptCursorPos == std::string("/archive/alternate.modr/").size(),
          "Save prompt canonicalizes a moved bound drawing file and preserves its caret");
    drawing.beginPathPrompt(monolith::app::DrawingApp::PathPromptMode::Open);
    drawing.m_pathPromptBuffer = "/drawings/nested/child.modr";
    drawing.m_pathPromptCursorPos = std::string("/drawings/nested/").size();
    drawing.onVirtualPathMoved("/drawings", "/archive/../archive");
    check(drawing.m_pathPromptBuffer == "/archive/nested/child.modr"
              && drawing.m_pathPromptCursorPos == std::string("/archive/nested/").size(),
          "Open prompt follows a moved Drawing directory and preserves its caret");
    drawing.beginPathPrompt(monolith::app::DrawingApp::PathPromptMode::Save);
    drawing.m_pathPromptBuffer = "/archive/alternate.modr";
    drawing.m_pathPromptCursorPos = drawing.m_pathPromptBuffer.size();
    drawing.onBoundFileRemoved("/archive/alternate.modr");
    check(drawing.m_pathPromptBuffer == "/archive/",
          "Save prompt returns to a valid parent after deletion");

    controller.lifecycleEvents.clear();
    check(drawing.saveToPath("/drawings/created"),
          "Drawing creates a new file through its save path");
    check(controller.lifecycleEvents.size() >= 2
              && controller.lifecycleEvents[0] == "bind:/drawings/created.modr"
              && controller.lifecycleEvents[1] == "created:/drawings/created.modr",
          "Drawing claims a new file before broadcasting its creation");

    SDL_Surface* surface = SDL_CreateRGBSurfaceWithFormat(
        0, 320, 320, 32, SDL_PIXELFORMAT_RGBA32);
    SDL_Renderer* renderer = surface ? SDL_CreateSoftwareRenderer(surface) : nullptr;
    check(renderer != nullptr, "drawing state creates a software renderer");
    if (renderer) {
        const SDL_Rect expectedClip{5, 6, 180, 160};
        SDL_RenderSetClipRect(renderer, &expectedClip);
        drawing.render(renderer, {0, 0, 280, 280});
        SDL_Rect restoredClip{};
        SDL_RenderGetClipRect(renderer, &restoredClip);
        check(restoredClip.x == expectedClip.x
                  && restoredClip.y == expectedClip.y
                  && restoredClip.w == expectedClip.w
                  && restoredClip.h == expectedClip.h,
              "Drawing restores the caller renderer clip after rendering");
        SDL_DestroyRenderer(renderer);
    }
    if (surface) SDL_FreeSurface(surface);

    std::filesystem::remove_all(hostRoot, ec);
    if (failures == 0) {
        std::cout << "ALL DRAWING STATE TESTS PASSED\n";
        TTF_CloseFont(font);
        TTF_Quit();
        SDL_Quit();
        return 0;
    }
    std::cerr << failures << " test(s) failed\n";
    TTF_CloseFont(font);
    TTF_Quit();
    SDL_Quit();
    return 1;
}
