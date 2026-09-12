// Headless regression test for Drawing file state across window resizes.

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

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

    void close() override {}
    void setTitle(const std::string&) override {}

    void notifyVirtualPathChanged(const std::string& path) override {
        if (drawing) drawing->onVirtualPathChanged(path);
    }
};

struct TestDrawing final : monolith::app::DrawingApp {
    using monolith::app::App::setController;

    TestDrawing(TTF_Font* font, monolith::fs::Filesystem* fs)
        : DrawingApp(font, fs) {}
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

    check(TTF_Init() == 0, "drawing state SDL_ttf initialize");
    TTF_Font* font = TTF_OpenFont("assets/fonts/DejaVuSans.ttf", 14);
    check(font != nullptr, "drawing state loads test font");
    if (!font) {
        TTF_Quit();
        std::filesystem::remove_all(hostRoot, ec);
        return 1;
    }

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

    drawing.onResize(300, 300);
    check(!drawing.m_dirty, "initial blank resize stays clean");
    check(drawing.loadFromPath("/drawings/resize.modr"), "load resize drawing");
    check(!drawing.m_dirty, "loaded drawing starts clean");

    const int baseToolbarHeight = drawing.m_canvasTop;
    const int baseStatusBarHeight = drawing.m_statusBarHeight;
    const int baseCanvasWidth = drawing.m_canvasWidth;
    const int baseCanvasHeight = drawing.m_canvasHeight;
    const std::vector<uint8_t> pixelsBeforeScale = drawing.m_pixels;
    drawing.pushUndoSnapshot();
    const size_t undoCountBeforeScale = drawing.m_undoStack.size();
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
    drawing.onResize(320, 300);
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
    drawing.beginPathPrompt(monolith::app::DrawingApp::PathPromptMode::Save);
    drawing.m_pathPromptBuffer = "/archive/alternate.modr";
    drawing.m_pathPromptCursorPos = drawing.m_pathPromptBuffer.size();
    drawing.onBoundFileRemoved("/archive/alternate.modr");
    check(drawing.m_pathPromptBuffer == "/archive/",
          "Save prompt returns to a valid parent after deletion");

    std::filesystem::remove_all(hostRoot, ec);
    if (failures == 0) {
        std::cout << "ALL DRAWING STATE TESTS PASSED\n";
        TTF_CloseFont(font);
        TTF_Quit();
        return 0;
    }
    std::cerr << failures << " test(s) failed\n";
    TTF_CloseFont(font);
    TTF_Quit();
    return 1;
}
