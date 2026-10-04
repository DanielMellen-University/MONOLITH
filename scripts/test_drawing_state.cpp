// Headless regression test for Drawing file state across window resizes.

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <limits>
#include <string>
#include <unistd.h>
#include <utility>
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

bool recordPixelEdit(TestDrawing& drawing, int x, int y,
                     uint8_t r, uint8_t g, uint8_t b) {
    drawing.beginSparseHistory();
    const bool changed = drawing.setPixel(x, y, r, g, b);
    if (changed) drawing.recordSparseHistoryChange();
    drawing.finishSparseHistory();
    return changed;
}

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
    auto historyAccountingMatches = [](const TestDrawing& drawing) {
        auto stackBytes = [](const auto& stack) {
            size_t bytes = 0;
            for (const auto& state : stack) {
                size_t stateBytes = 0;
                for (const auto& tile : state.tiles) stateBytes += tile.pixels.size();
                if (state.pixelBytes != stateBytes) return std::numeric_limits<size_t>::max();
                bytes += stateBytes;
            }
            return bytes;
        };
        return drawing.m_undoHistoryBytes == stackBytes(drawing.m_undoStack)
            && drawing.m_redoHistoryBytes == stackBytes(drawing.m_redoStack)
            && drawing.m_undoHistoryBytes + drawing.m_redoHistoryBytes
                <= 64 * 1024 * 1024;
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
    check(fs.createDirectory("/home/monolith/drawings"),
          "create virtual-home Drawing prompt directory");
    check(fs.writeFile("/home/monolith/drawings/home.modr",
                       monolith::drawing::encodeModr(2, 2, pixels)),
          "write virtual-home Drawing prompt target");
    check(fs.writeFile("/drawings/corrupt.modr", "not a drawing"),
          "write corrupt drawing retry target");
    check(fs.writeFile("/drawings/oversized.modr", ""),
          "create oversized drawing retry target");
    std::filesystem::resize_file(
        fs.toHostPath("/drawings/oversized.modr"),
        monolith::drawing::kMaxModrEncodedBytes + 1, ec);
    check(!ec, "resize drawing retry target beyond MODR size limit");
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
    recordPixelEdit(failedInitialDrawing, 0, 0, 9, 8, 7);
    failedInitialDrawing.undoCanvas();
    check(!failedInitialDrawing.m_dirty,
          "undoing after a failed initial Drawing open clears the modified state");

    TestDrawing drawing(font, &fs);
    TestDrawing oversizedSave(font, &fs);
    oversizedSave.resizeCanvas(monolith::drawing::kMaxModrDimension + 1, 1, false);
    check(!oversizedSave.saveToPath("/drawings/oversized-canvas/oversized.modr")
              && !fs.exists("/drawings/oversized-canvas")
              && !fs.exists("/drawings/oversized-canvas/oversized.modr")
              && oversizedSave.m_statusMessage
                  == "Save failed: canvas exceeds the .modr dimension limit.",
          "Drawing rejects an oversized save before creating a directory or unreadable file");

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

    TestDrawing homeAliasDrawing(font, &fs);
    homeAliasDrawing.onResize(300, 300);
    homeAliasDrawing.beginPathPrompt(monolith::app::DrawingApp::PathPromptMode::Open);
    homeAliasDrawing.m_pathPromptBuffer = "~/drawings/home";
    homeAliasDrawing.m_pathPromptCursorPos = homeAliasDrawing.m_pathPromptBuffer.size();
    homeAliasDrawing.completePathPrompt();
    check(homeAliasDrawing.m_pathPromptBuffer == "~/drawings/home.modr",
          "Drawing completion searches virtual home and keeps the shorthand");
    homeAliasDrawing.finishPathPrompt(true);
    check(homeAliasDrawing.m_filePath == "/home/monolith/drawings/home.modr"
              && homeAliasDrawing.m_canvasWidth == 2
              && homeAliasDrawing.m_canvasHeight == 2,
          "Drawing Open expands a home-relative prompt path");
    homeAliasDrawing.beginPathPrompt(monolith::app::DrawingApp::PathPromptMode::Save);
    homeAliasDrawing.m_pathPromptBuffer = "~/drawings/home-save";
    homeAliasDrawing.m_pathPromptCursorPos = homeAliasDrawing.m_pathPromptBuffer.size();
    homeAliasDrawing.finishPathPrompt(true);
    check(homeAliasDrawing.m_filePath
                  == "/home/monolith/drawings/home-save.modr"
              && fs.isFile("/home/monolith/drawings/home-save.modr"),
          "Drawing Save expands a home-relative prompt path");

    drawing.onResize(300, 300);
    check(!drawing.m_dirty, "initial blank resize stays clean");
    TestDrawing snapshotReuse(font, &fs);
    snapshotReuse.onResize(300, 300);
    auto* originalSnapshotBuffer = snapshotReuse.m_savedSnapshot.pixels.data();
    const std::size_t originalSnapshotCapacity =
        snapshotReuse.m_savedSnapshot.pixels.capacity();
    snapshotReuse.m_pixels[0] = 17;
    snapshotReuse.captureSavedSnapshot();
    check(snapshotReuse.m_savedSnapshot.pixels.data() == originalSnapshotBuffer
              && snapshotReuse.m_savedSnapshot.pixels.capacity() == originalSnapshotCapacity
              && snapshotReuse.m_savedSnapshot.pixels == snapshotReuse.m_pixels
              && snapshotReuse.m_savedSnapshot.width == snapshotReuse.m_canvasWidth
              && snapshotReuse.m_savedSnapshot.height == snapshotReuse.m_canvasHeight,
          "same-size saved-baseline capture reuses its pixel allocation");
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
    const bool noOpDidNotCapturePixels = drawing.m_sparseHistoryEntry.tiles.empty();
    SDL_Event noOpStrokeUp = noOpStrokeDown;
    noOpStrokeUp.type = SDL_MOUSEBUTTONUP;
    drawing.handleEvent(noOpStrokeUp);
    check(!drawing.m_dirty
              && noOpDidNotCapturePixels
              && drawing.m_pixels == blankPixels
              && drawing.m_undoStack.size() == clearUndoCount,
          "an eraser stroke on a blank Drawing stays clean without capturing undo pixels");

    TestDrawing scratchTracking(font, &fs);
    scratchTracking.resizeCanvas(1024, 1024, false);
    scratchTracking.m_savedSnapshot = {
        scratchTracking.m_canvasWidth,
        scratchTracking.m_canvasHeight,
        scratchTracking.m_pixels};
    scratchTracking.beginSparseHistory();
    const bool firstScratchWrite = scratchTracking.setPixel(33, 65, 1, 2, 3);
    const size_t firstScratchTile = 2 * 32 + 1;
    const bool firstScratchMarked = firstScratchWrite
        && scratchTracking.m_sparseHistoryCapturedTileIndices.size() == 1
        && scratchTracking.m_sparseHistoryCapturedTileIndices.front() == firstScratchTile
        && scratchTracking.m_sparseHistoryCapturedTiles[firstScratchTile] == 1;
    scratchTracking.recordSparseHistoryChange();
    scratchTracking.finishSparseHistory();
    const bool firstScratchCleared =
        scratchTracking.m_sparseHistoryCapturedTileIndices.empty()
        && scratchTracking.m_sparseHistoryCapturedTiles[firstScratchTile] == 0;
    scratchTracking.beginSparseHistory();
    const bool secondScratchWrite = scratchTracking.setPixel(500, 500, 4, 5, 6);
    const size_t secondScratchTile = 15 * 32 + 15;
    const bool secondScratchMarked = secondScratchWrite
        && scratchTracking.m_sparseHistoryCapturedTileIndices.size() == 1
        && scratchTracking.m_sparseHistoryCapturedTileIndices.front() == secondScratchTile
        && scratchTracking.m_sparseHistoryCapturedTiles[firstScratchTile] == 0
        && scratchTracking.m_sparseHistoryCapturedTiles[secondScratchTile] == 1;
    scratchTracking.recordSparseHistoryChange();
    scratchTracking.finishSparseHistory();
    check(firstScratchMarked && firstScratchCleared && secondScratchMarked
              && scratchTracking.m_sparseHistoryCapturedTileIndices.empty()
              && scratchTracking.m_sparseHistoryCapturedTiles[secondScratchTile] == 0,
          "sparse stroke scratch marks reset only their touched indices between edits");

    TestDrawing redoDrawing(font, &fs);
    redoDrawing.onResize(300, 300);
    redoDrawing.m_tool = monolith::app::DrawingApp::Tool::Pen;
    const std::vector<uint8_t> redoBaseline = redoDrawing.m_pixels;
    SDL_Event changedStrokeDown = noOpStrokeDown;
    changedStrokeDown.button.x = 20;
    changedStrokeDown.button.y = redoDrawing.m_canvasTop + 20;
    redoDrawing.handleEvent(changedStrokeDown);
    SDL_Event changedStrokeUp = changedStrokeDown;
    changedStrokeUp.type = SDL_MOUSEBUTTONUP;
    redoDrawing.handleEvent(changedStrokeUp);
    const std::vector<uint8_t> pixelsAfterStroke = redoDrawing.m_pixels;
    size_t strokeHistoryBytes = 0;
    for (const auto& tile : redoDrawing.m_undoStack.front().tiles) {
        strokeHistoryBytes += tile.pixels.size();
    }
    check(redoDrawing.m_dirty && redoDrawing.m_undoStack.size() == 1
              && !redoDrawing.m_undoStack.front().tiles.empty()
              && strokeHistoryBytes < redoBaseline.size()
              && historyAccountingMatches(redoDrawing),
          "a changed Drawing stroke records sparse tile history instead of a full canvas");
    redoDrawing.undoCanvas();
    check(!redoDrawing.m_dirty && redoDrawing.m_undoStack.empty()
              && redoDrawing.m_redoStack.size() == 1
              && redoDrawing.m_pixels == redoBaseline
              && historyAccountingMatches(redoDrawing),
          "undoing a Drawing stroke restores the exact canvas and exposes redo");
    redoDrawing.m_tool = monolith::app::DrawingApp::Tool::Eraser;
    changedStrokeDown.button.x = 20;
    changedStrokeDown.button.y = redoDrawing.m_canvasTop + 20;
    redoDrawing.handleEvent(changedStrokeDown);
    changedStrokeUp = changedStrokeDown;
    changedStrokeUp.type = SDL_MOUSEBUTTONUP;
    redoDrawing.handleEvent(changedStrokeUp);
    check(!redoDrawing.m_dirty && redoDrawing.m_undoStack.empty()
              && redoDrawing.m_redoStack.size() == 1
              && historyAccountingMatches(redoDrawing),
          "a no-op Drawing stroke preserves redo history");
    redoDrawing.redoCanvas();
    check(redoDrawing.m_dirty && redoDrawing.m_redoStack.empty()
              && redoDrawing.m_pixels == pixelsAfterStroke
              && historyAccountingMatches(redoDrawing),
          "preserved Drawing redo history restores the exact stroke pixels");
    redoDrawing.undoCanvas();
    redoDrawing.m_tool = monolith::app::DrawingApp::Tool::Pen;
    changedStrokeDown.button.x = 30;
    changedStrokeDown.button.y = redoDrawing.m_canvasTop + 30;
    redoDrawing.handleEvent(changedStrokeDown);
    changedStrokeUp = changedStrokeDown;
    changedStrokeUp.type = SDL_MOUSEBUTTONUP;
    redoDrawing.handleEvent(changedStrokeUp);
    check(redoDrawing.m_redoStack.empty() && redoDrawing.m_redoHistoryBytes == 0
              && historyAccountingMatches(redoDrawing),
          "a new Drawing edit clears redo history and its cached byte total");

    TestDrawing savedHistory(font, &fs);
    savedHistory.onResize(300, 300);
    const std::vector<uint8_t> savedHistoryBaseline = savedHistory.m_pixels;
    savedHistory.m_tool = monolith::app::DrawingApp::Tool::Pen;
    savedHistory.beginSparseHistory();
    const bool savedHistoryChanged = savedHistory.drawStroke(4, 4, 10, 4);
    savedHistory.recordSparseHistoryChange();
    savedHistory.finishSparseHistory();
    const std::vector<uint8_t> savedHistoryPixels = savedHistory.m_pixels;
    const bool savedHistoryStored = savedHistoryChanged
        && savedHistory.saveToPath("/drawings/saved-history.modr")
        && !savedHistory.m_dirty
        && savedHistory.m_dirtyTileCount == 0;
    check(savedHistoryStored,
          "saving a Drawing stroke establishes a clean mid-history baseline");
    savedHistory.undoCanvas();
    check(savedHistory.m_dirty,
          "sparse undo sets modified state against the saved baseline");
    check(savedHistory.m_dirtyTileCount == 1,
          "sparse undo marks exactly its changed tile dirty");
    check(savedHistory.m_pixels == savedHistoryBaseline,
          "sparse undo restores the pixels from before the saved stroke");
    savedHistory.redoCanvas();
    check(!savedHistory.m_dirty && savedHistory.m_dirtyTileCount == 0
              && savedHistory.m_pixels == savedHistoryPixels,
          "sparse redo clears tile dirtiness against the saved baseline");

    TestDrawing multiTileStroke(font, &fs);
    multiTileStroke.onResize(320, 256);
    multiTileStroke.m_tool = monolith::app::DrawingApp::Tool::Pen;
    multiTileStroke.m_brush = monolith::app::DrawingApp::BrushSize::Small;
    const std::vector<uint8_t> multiTileBaseline = multiTileStroke.m_pixels;
    multiTileStroke.beginSparseHistory();
    const bool multiTileChanged = multiTileStroke.drawStroke(3, 3, 300, 130);
    multiTileStroke.recordSparseHistoryChange();
    multiTileStroke.finishSparseHistory();
    const std::vector<uint8_t> multiTileAfter = multiTileStroke.m_pixels;
    size_t multiTileBytes = 0;
    for (const auto& tile : multiTileStroke.m_undoStack.front().tiles) {
        multiTileBytes += tile.pixels.size();
    }
    check(multiTileChanged && multiTileStroke.m_undoStack.size() == 1
              && multiTileStroke.m_undoStack.front().tiles.size() > 1
              && multiTileBytes < multiTileBaseline.size(),
          "a long stroke captures each touched tile once across tile boundaries");
    multiTileStroke.undoCanvas();
    check(multiTileStroke.m_pixels == multiTileBaseline,
          "multi-tile undo restores every touched pixel");
    multiTileStroke.redoCanvas();
    check(multiTileStroke.m_pixels == multiTileAfter,
          "multi-tile redo reapplies every touched pixel");

    {
        TestDrawing largeCanvasStroke(font, &fs);
        constexpr int largeCanvasWidth = 4096;
        constexpr int largeCanvasHeight = 4097;
        largeCanvasStroke.resizeCanvas(largeCanvasWidth, largeCanvasHeight, false);
        largeCanvasStroke.m_savedSnapshot = {
            largeCanvasWidth, largeCanvasHeight, largeCanvasStroke.m_pixels};
        largeCanvasStroke.m_tool = monolith::app::DrawingApp::Tool::Pen;
        largeCanvasStroke.m_brush = monolith::app::DrawingApp::BrushSize::Small;
        largeCanvasStroke.beginSparseHistory();
        const bool largeCanvasChanged = largeCanvasStroke.drawStroke(100, 100, 106, 100);
        largeCanvasStroke.recordSparseHistoryChange();
        largeCanvasStroke.finishSparseHistory();
        const size_t largeCanvasStrokeBytes =
            largeCanvasStroke.m_undoStack.empty()
            ? 0
            : largeCanvasStroke.m_undoStack.back().tiles.front().pixels.size();
        check(largeCanvasStroke.m_pixels.size() > 64 * 1024 * 1024
                  && largeCanvasChanged
                  && largeCanvasStroke.m_undoStack.size() == 1
                  && largeCanvasStrokeBytes < largeCanvasStroke.m_pixels.size(),
              "sparse strokes remain undoable on canvases larger than the history byte cap");
        largeCanvasStroke.undoCanvas();
        check(largeCanvasStroke.m_pixels == largeCanvasStroke.m_savedSnapshot.pixels,
              "large-canvas sparse undo restores the original image");
        largeCanvasStroke.redoCanvas();
        const size_t changedPixel =
            (static_cast<size_t>(100) * largeCanvasWidth + 100) * 4;
        check(largeCanvasStroke.m_pixels[changedPixel] != 245,
              "large-canvas sparse redo reapplies the stroke");

        largeCanvasStroke.undoCanvas();
        const bool oversizedStrokeStartsWithRedo =
            largeCanvasStroke.m_redoStack.size() == 1;
        largeCanvasStroke.m_tool = monolith::app::DrawingApp::Tool::Pen;
        largeCanvasStroke.beginSparseHistory();
        for (int y = 0; y < largeCanvasHeight; y += 32) {
            largeCanvasStroke.drawStroke(0, y, largeCanvasWidth - 1, y);
        }
        const bool oversizedStrokeDiscardedHistory =
            oversizedStrokeStartsWithRedo
            && largeCanvasStroke.m_sparseHistoryOverflowed
            && largeCanvasStroke.m_sparseHistoryEntry.tiles.empty()
            && largeCanvasStroke.m_undoStack.empty()
            && largeCanvasStroke.m_redoStack.empty();
        largeCanvasStroke.recordSparseHistoryChange();
        largeCanvasStroke.finishSparseHistory();
        check(oversizedStrokeDiscardedHistory
                  && largeCanvasStroke.m_sparseHistoryBytes == 0
                  && largeCanvasStroke.m_undoStack.empty()
                  && largeCanvasStroke.m_redoStack.empty(),
              "canvas-wide strokes discard oversized transient history and stale redo");

        constexpr int sparseFillX = 1000;
        constexpr int sparseFillY = 1009;
        constexpr int preservedLineY = 992;
        const size_t sparseFillPixel =
            (static_cast<size_t>(sparseFillY) * largeCanvasWidth + sparseFillX) * 4;
        const size_t preservedLinePixel =
            (static_cast<size_t>(preservedLineY) * largeCanvasWidth + sparseFillX) * 4;
        const bool largeFillSeedSet =
            largeCanvasStroke.setPixel(sparseFillX, sparseFillY, 1, 2, 3);
        const uint8_t preservedLine[4] = {
            largeCanvasStroke.m_pixels[preservedLinePixel],
            largeCanvasStroke.m_pixels[preservedLinePixel + 1],
            largeCanvasStroke.m_pixels[preservedLinePixel + 2],
            largeCanvasStroke.m_pixels[preservedLinePixel + 3],
        };
        largeCanvasStroke.m_tool = monolith::app::DrawingApp::Tool::Eraser;
        largeCanvasStroke.floodFill(sparseFillX, sparseFillY);
        const uint8_t filledPixel[3] = {
            largeCanvasStroke.m_pixels[sparseFillPixel],
            largeCanvasStroke.m_pixels[sparseFillPixel + 1],
            largeCanvasStroke.m_pixels[sparseFillPixel + 2],
        };
        size_t largeFillHistoryBytes = 0;
        if (!largeCanvasStroke.m_undoStack.empty()) {
            for (const auto& tile : largeCanvasStroke.m_undoStack.back().tiles) {
                largeFillHistoryBytes += tile.pixels.size();
            }
        }
        check(largeFillSeedSet
                  && largeCanvasStroke.m_pixels.size() > 64 * 1024 * 1024
                  && largeCanvasStroke.m_undoStack.size() == 1
                  && largeCanvasStroke.m_undoStack.back().tiles.size() == 1
                  && largeFillHistoryBytes < largeCanvasStroke.m_pixels.size(),
              "a localized Fill stores one sparse tile on a canvas above the history budget");
        largeCanvasStroke.undoCanvas();
        check(largeCanvasStroke.m_pixels[sparseFillPixel] == 1
                  && largeCanvasStroke.m_pixels[sparseFillPixel + 1] == 2
                  && largeCanvasStroke.m_pixels[sparseFillPixel + 2] == 3
                  && std::equal(preservedLine, preservedLine + 4,
                                largeCanvasStroke.m_pixels.begin() + preservedLinePixel),
              "large-canvas sparse Fill undo restores its seed and preserves nearby pixels");
        largeCanvasStroke.redoCanvas();
        check(std::equal(filledPixel, filledPixel + 3,
                         largeCanvasStroke.m_pixels.begin() + sparseFillPixel)
                  && std::equal(preservedLine, preservedLine + 4,
                                largeCanvasStroke.m_pixels.begin() + preservedLinePixel),
              "large-canvas sparse Fill redo reapplies only the filled region");

        std::copy(largeCanvasStroke.m_savedSnapshot.pixels.begin(),
                  largeCanvasStroke.m_savedSnapshot.pixels.end(),
                  largeCanvasStroke.m_pixels.begin());
        largeCanvasStroke.clearHistory();
        largeCanvasStroke.refreshDirtyState();
        constexpr int sparseClearX = 1000;
        constexpr int sparseClearY = 1000;
        constexpr int sparseClearEdgeX = largeCanvasWidth - 1;
        constexpr int sparseClearEdgeY = largeCanvasHeight - 1;
        const size_t sparseClearPixel =
            (static_cast<size_t>(sparseClearY) * largeCanvasWidth + sparseClearX) * 4;
        const size_t sparseClearEdgePixel =
            (static_cast<size_t>(sparseClearEdgeY) * largeCanvasWidth + sparseClearEdgeX) * 4;
        const bool sparseClearSeedsSet =
            largeCanvasStroke.setPixel(sparseClearX, sparseClearY, 12, 34, 56)
            && largeCanvasStroke.setPixel(sparseClearEdgeX, sparseClearEdgeY, 78, 90, 123);
        largeCanvasStroke.clearCanvas();
        size_t sparseClearHistoryBytes = 0;
        if (!largeCanvasStroke.m_undoStack.empty()) {
            for (const auto& tile : largeCanvasStroke.m_undoStack.back().tiles) {
                sparseClearHistoryBytes += tile.pixels.size();
            }
        }
        check(sparseClearSeedsSet
                  && largeCanvasStroke.m_pixels.size() > 64 * 1024 * 1024,
              "large sparse Clear fixture contains separated marks above the history budget");
        check(largeCanvasStroke.m_undoStack.size() == 1
                  && largeCanvasStroke.m_undoStack.back().tiles.size() == 2
                  && sparseClearHistoryBytes == 32 * 32 * 4 + 32 * 4
                  && historyAccountingMatches(largeCanvasStroke),
              "large sparse Clear stores only changed tiles");
        check(largeCanvasStroke.m_sparseHistoryCapturedTileIndices.empty()
                  && std::none_of(largeCanvasStroke.m_sparseHistoryCapturedTiles.begin(),
                                  largeCanvasStroke.m_sparseHistoryCapturedTiles.end(),
                                  [](uint8_t captured) { return captured != 0; })
                  && largeCanvasStroke.m_dirtyTrackedTileIndices.empty()
                  && std::none_of(largeCanvasStroke.m_dirtyTrackingTiles.begin(),
                                  largeCanvasStroke.m_dirtyTrackingTiles.end(),
                                  [](uint8_t tracked) { return tracked != 0; }),
              "Drawing Clear resets only captured and dirty marks it touched");
        check(largeCanvasStroke.m_pixels == largeCanvasStroke.m_savedSnapshot.pixels
                  && !largeCanvasStroke.m_dirty,
              "large sparse Clear returns the modified marker to the saved baseline");
        largeCanvasStroke.undoCanvas();
        check(largeCanvasStroke.m_pixels[sparseClearPixel] == 12
                  && largeCanvasStroke.m_pixels[sparseClearPixel + 1] == 34
                  && largeCanvasStroke.m_pixels[sparseClearPixel + 2] == 56
                  && largeCanvasStroke.m_pixels[sparseClearEdgePixel] == 78
                  && largeCanvasStroke.m_pixels[sparseClearEdgePixel + 1] == 90
                  && largeCanvasStroke.m_pixels[sparseClearEdgePixel + 2] == 123
                  && largeCanvasStroke.m_dirty,
              "large sparse Clear undo restores separated marks and the modified state");
        largeCanvasStroke.redoCanvas();
        check(largeCanvasStroke.m_pixels == largeCanvasStroke.m_savedSnapshot.pixels
                  && !largeCanvasStroke.m_dirty,
              "large sparse Clear redo restores the saved baseline");

        for (size_t i = 0; i < largeCanvasStroke.m_pixels.size(); i += 4) {
            largeCanvasStroke.m_pixels[i + 0] = 1;
            largeCanvasStroke.m_pixels[i + 1] = 2;
            largeCanvasStroke.m_pixels[i + 2] = 3;
            largeCanvasStroke.m_pixels[i + 3] = 255;
        }
        largeCanvasStroke.clearHistory();
        largeCanvasStroke.refreshDirtyState();
        largeCanvasStroke.clearCanvas();
        check(largeCanvasStroke.m_undoStack.empty()
                  && largeCanvasStroke.m_redoStack.empty()
                  && largeCanvasStroke.m_pixels == largeCanvasStroke.m_savedSnapshot.pixels
                  && !largeCanvasStroke.m_dirty
                  && largeCanvasStroke.m_sparseHistoryBytes == 0
                  && largeCanvasStroke.m_undoHistoryBytes == 0
                  && largeCanvasStroke.m_redoHistoryBytes == 0,
              "dense Clear beyond the history budget releases captures and clears the canvas");
    }

    TestDrawing stateLimitedHistory(font, &fs);
    stateLimitedHistory.onResize(300, 300);
    for (int i = 0; i < 40; ++i) {
        recordPixelEdit(stateLimitedHistory, 10, 10,
                        static_cast<uint8_t>(i + 1), 8, 7);
    }
    for (int i = 0; i < 20; ++i) stateLimitedHistory.undoCanvas();
    check(stateLimitedHistory.m_undoStack.size() + stateLimitedHistory.m_redoStack.size() == 32,
          "Drawing keeps undo and redo within 32 combined history states");
    check(historyAccountingMatches(stateLimitedHistory),
          "Drawing history byte totals follow states through undo and redo");

    constexpr int historyWidth = 3072;
    constexpr int historyHeight = 2048;
    TestDrawing byteLimitedHistory(font, &fs);
    byteLimitedHistory.resizeCanvas(historyWidth, historyHeight, false);
    constexpr size_t historyStateBytes = 24 * 1024 * 1024;
    auto makeSparseHistoryEntry = [&](uint8_t value) {
        TestDrawing::CanvasHistoryEntry entry;
        entry.width = historyWidth;
        entry.height = historyHeight;
        for (int y = 0; y < historyHeight; y += 32) {
            for (int x = 0; x < historyWidth; x += 32) {
                TestDrawing::CanvasTileSnapshot tile;
                tile.x = x;
                tile.y = y;
                tile.width = std::min(32, historyWidth - x);
                tile.height = std::min(32, historyHeight - y);
                tile.pixels.resize(static_cast<size_t>(tile.width)
                                   * static_cast<size_t>(tile.height) * 4);
                for (size_t i = 0; i < tile.pixels.size(); i += 4) {
                    tile.pixels[i + 0] = value;
                    tile.pixels[i + 1] = value;
                    tile.pixels[i + 2] = value;
                    tile.pixels[i + 3] = 255;
                }
                entry.tiles.push_back(std::move(tile));
            }
        }
        return entry;
    };
    for (uint8_t value = 1; value <= 3; ++value) {
        byteLimitedHistory.pushUndoHistoryEntry(makeSparseHistoryEntry(value));
    }
    size_t retainedHistoryBytes = 0;
    for (const auto& state : byteLimitedHistory.m_undoStack) {
        for (const auto& tile : state.tiles) {
            retainedHistoryBytes += tile.pixels.size();
        }
    }
    check(byteLimitedHistory.m_undoStack.size() == 2
              && byteLimitedHistory.m_undoStack[0].tiles.front().pixels[0] == 2
              && byteLimitedHistory.m_undoStack[1].tiles.front().pixels[0] == 3
              && retainedHistoryBytes == historyStateBytes * 2
              && retainedHistoryBytes <= 64 * 1024 * 1024
              && historyAccountingMatches(byteLimitedHistory),
          "Drawing evicts oldest sparse states at the 64 MiB budget");

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

    TestDrawing promptGestureDrawing(font, &fs);
    promptGestureDrawing.onResize(300, 300);
    SDL_Event promptStrokeDown{};
    promptStrokeDown.type = SDL_MOUSEBUTTONDOWN;
    promptStrokeDown.button.button = SDL_BUTTON_LEFT;
    promptStrokeDown.button.x = 20;
    promptStrokeDown.button.y = promptGestureDrawing.m_canvasTop + 20;
    promptGestureDrawing.handleEvent(promptStrokeDown);
    promptGestureDrawing.beginPathPrompt(TestDrawing::PathPromptMode::Open);
    check(!promptGestureDrawing.m_drawing
              && !promptGestureDrawing.m_sparseHistoryPending
              && promptGestureDrawing.m_lastCanvasX == -1
              && promptGestureDrawing.m_lastCanvasY == -1,
          "Drawing path prompts end an active mouse stroke");
    promptGestureDrawing.finishPathPrompt(false);

    TestDrawing focusGestureDrawing(font, &fs);
    focusGestureDrawing.onResize(300, 300);
    SDL_Event focusStrokeDown{};
    focusStrokeDown.type = SDL_MOUSEBUTTONDOWN;
    focusStrokeDown.button.button = SDL_BUTTON_LEFT;
    focusStrokeDown.button.x = 20;
    focusStrokeDown.button.y = focusGestureDrawing.m_canvasTop + 20;
    focusGestureDrawing.handleEvent(focusStrokeDown);
    focusGestureDrawing.onFocusLost();
    check(!focusGestureDrawing.m_drawing
              && !focusGestureDrawing.m_sparseHistoryPending
              && focusGestureDrawing.m_lastCanvasX == -1
              && focusGestureDrawing.m_lastCanvasY == -1,
          "Drawing focus loss ends an active mouse stroke");

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

    TestDrawing fillDirtyTracking(font, &fs);
    fillDirtyTracking.onResize(300, 300);
    const std::vector<uint8_t> fillDirtyBaseline = fillDirtyTracking.m_pixels;
    fillDirtyTracking.setPixel(10, 10, 1, 2, 3);
    fillDirtyTracking.setPixel(11, 10, 1, 2, 3);
    fillDirtyTracking.m_savedSnapshot = {
        fillDirtyTracking.m_canvasWidth,
        fillDirtyTracking.m_canvasHeight,
        fillDirtyBaseline};
    fillDirtyTracking.refreshDirtyState();
    fillDirtyTracking.m_tool = monolith::app::DrawingApp::Tool::Eraser;
    fillDirtyTracking.floodFill(10, 10);
    size_t fillHistoryBytes = 0;
    if (!fillDirtyTracking.m_undoStack.empty()) {
        for (const auto& tile : fillDirtyTracking.m_undoStack.back().tiles) {
            fillHistoryBytes += tile.pixels.size();
        }
    }
    check(fillDirtyTracking.m_undoStack.size() == 1
              && fillDirtyTracking.m_undoStack.back().tiles.size() == 1
              && fillHistoryBytes < fillDirtyTracking.m_pixels.size(),
          "localized Drawing Fill history stores touched tiles instead of the full canvas");
    check(fillDirtyTracking.m_dirtyTrackedTileIndices.empty()
              && std::none_of(fillDirtyTracking.m_dirtyTrackingTiles.begin(),
                              fillDirtyTracking.m_dirtyTrackingTiles.end(),
                              [](uint8_t tracked) { return tracked != 0; }),
          "Drawing Fill clears its touched dirty marks while retaining scratch storage");
    check(!fillDirtyTracking.m_dirty && fillDirtyTracking.m_dirtyTileCount == 0,
          "fill that restores saved pixels clears only the touched tile's dirty state");
    fillDirtyTracking.undoCanvas();
    check(fillDirtyTracking.m_dirty && fillDirtyTracking.m_dirtyTileCount == 1,
          "undoing fill recomputes dirty tiles against the saved image");
    fillDirtyTracking.redoCanvas();
    check(!fillDirtyTracking.m_dirty && fillDirtyTracking.m_dirtyTileCount == 0,
          "redoing fill clears tile dirtiness when it restores the saved image");

    const int baseToolbarHeight = drawing.m_canvasTop;
    const int baseStatusBarHeight = drawing.m_statusBarHeight;
    const int baseCanvasWidth = drawing.m_canvasWidth;
    const int baseCanvasHeight = drawing.m_canvasHeight;
    const std::vector<uint8_t> pixelsBeforeScale = drawing.m_pixels;
    recordPixelEdit(drawing, 0, 0, 9, 8, 7);
    drawing.undoCanvas();
    const size_t undoCountBeforeScale = drawing.m_undoStack.size();
    const size_t redoCountBeforeScale = drawing.m_redoStack.size();
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
              && drawing.m_redoStack.size() == redoCountBeforeScale
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

    controller.occupiedDrawingPath = "/drawings/resize.modr";
    check(fs.writeFile("/drawings/resize.modr",
                       monolith::drawing::encodeModr(1, 1, externalPixels)),
          "write another external Drawing version");
    drawing.onVirtualPathChanged("/drawings/resize.modr");
    recordPixelEdit(drawing, 0, 0, 1, 2, 3);
    drawing.beginPathPrompt(monolith::app::DrawingApp::PathPromptMode::Open);
    drawing.m_pathPromptBuffer = "/drawings/resize.modr";
    drawing.m_pathPromptCursorPos = drawing.m_pathPromptBuffer.size();
    drawing.finishPathPrompt(true);
    check(drawing.m_discardKind == monolith::app::DrawingApp::DiscardKind::Open
              && drawing.m_canvasWidth == 2 && drawing.m_dirty,
          "opening the current Drawing path asks before discarding local pixels");
    drawing.finishPathPrompt(true);
    check(drawing.m_filePath == "/drawings/resize.modr"
              && drawing.m_canvasWidth == 1 && drawing.m_canvasHeight == 1
              && drawing.m_pixels == externalPixels && !drawing.m_dirty,
          "confirmed same-file Open reloads the external Drawing version");
    controller.occupiedDrawingPath.clear();
    check(fs.writeFile("/drawings/resize.modr",
                       monolith::drawing::encodeModr(2, 2, loadedPixels)),
          "restore the shared Drawing fixture after same-file reload coverage");

    drawing.startNewSketch();
    check(drawing.m_filePath.empty() && !drawing.m_dirty
              && drawing.m_savedSnapshot.pixels == drawing.m_pixels,
          "New Drawing starts with a clean blank baseline");
    recordPixelEdit(drawing, 0, 0, 9, 8, 7);
    drawing.undoCanvas();
    check(!drawing.m_dirty,
          "undoing a new Drawing edit clears the modified state");
    drawing.redoCanvas();
    check(drawing.m_dirty,
          "redoing a new Drawing edit restores the modified state");
    check(drawing.loadFromPath("/drawings/resize.modr"),
          "reload Drawing fixture after new-sketch baseline coverage");
    check(!drawing.m_dirty, "reloaded Drawing remains clean after baseline coverage");

    recordPixelEdit(drawing, 0, 0, 1, 2, 3);
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

    TestDrawing retryDrawing(nullptr, &fs, "/drawings/resize.modr");
    retryDrawing.onResize(300, 300);
    retryDrawing.beginPathPrompt(monolith::app::DrawingApp::PathPromptMode::Open);
    retryDrawing.m_pathPromptBuffer = "/drawings/oversized.modr";
    retryDrawing.m_pathPromptCursorPos = retryDrawing.m_pathPromptBuffer.size();
    retryDrawing.m_pathPromptScrollPx = 17;
    const auto pixelsBeforeRetry = retryDrawing.m_pixels;
    retryDrawing.finishPathPrompt(true);
    check(retryDrawing.m_pathPromptMode == monolith::app::DrawingApp::PathPromptMode::Open
              && retryDrawing.m_pathPromptBuffer == "/drawings/oversized.modr"
              && retryDrawing.m_pathPromptCursorPos == retryDrawing.m_pathPromptBuffer.size()
              && retryDrawing.m_pathPromptScrollPx == 17
              && retryDrawing.m_filePath == "/drawings/resize.modr"
              && retryDrawing.m_pixels == pixelsBeforeRetry
              && retryDrawing.m_statusMessage == "Open failed: .modr file exceeds the size limit.",
          "oversized Drawing Open rejects before loading and preserves retry state");
    retryDrawing.m_pathPromptBuffer = "/drawings/corrupt.modr";
    retryDrawing.m_pathPromptCursorPos = 10;
    retryDrawing.finishPathPrompt(true);
    check(retryDrawing.m_pathPromptMode == monolith::app::DrawingApp::PathPromptMode::Open
              && retryDrawing.m_pathPromptBuffer == "/drawings/corrupt.modr"
              && retryDrawing.m_pathPromptCursorPos == 10
              && retryDrawing.m_pathPromptScrollPx == 17
              && retryDrawing.m_filePath == "/drawings/resize.modr"
              && retryDrawing.m_pixels == pixelsBeforeRetry,
          "rejected Drawing Open keeps its prompt state and current canvas");
    retryDrawing.m_pathPromptBuffer = "/drawings/alternate.modr";
    retryDrawing.m_pathPromptCursorPos = retryDrawing.m_pathPromptBuffer.size();
    retryDrawing.finishPathPrompt(true);
    check(retryDrawing.m_pathPromptMode == monolith::app::DrawingApp::PathPromptMode::None
              && retryDrawing.m_filePath == "/drawings/alternate.modr",
          "correcting a corrupt Drawing path retries successfully");

    TestDrawing retryRgbDrawing(nullptr, &fs);
    retryRgbDrawing.onResize(300, 300);
    retryRgbDrawing.beginPathPrompt(monolith::app::DrawingApp::PathPromptMode::Rgb);
    retryRgbDrawing.m_pathPromptBuffer = "300,0,0";
    retryRgbDrawing.m_pathPromptCursorPos = 3;
    retryRgbDrawing.m_pathPromptScrollPx = 11;
    retryRgbDrawing.finishPathPrompt(true);
    check(retryRgbDrawing.m_pathPromptMode == monolith::app::DrawingApp::PathPromptMode::Rgb
              && retryRgbDrawing.m_pathPromptBuffer == "300,0,0"
              && retryRgbDrawing.m_pathPromptCursorPos == 3
              && retryRgbDrawing.m_pathPromptScrollPx == 11,
          "invalid custom RGB input remains editable for correction");
    retryRgbDrawing.m_pathPromptBuffer = "12,34,56";
    retryRgbDrawing.m_pathPromptCursorPos = retryRgbDrawing.m_pathPromptBuffer.size();
    retryRgbDrawing.finishPathPrompt(true);
    check(retryRgbDrawing.m_pathPromptMode == monolith::app::DrawingApp::PathPromptMode::None
              && retryRgbDrawing.m_customR == 12
              && retryRgbDrawing.m_customG == 34
              && retryRgbDrawing.m_customB == 56,
          "correcting invalid custom RGB input applies the color");

    TestDrawing retrySaveDrawing(nullptr, &fs, "/blocked.modr");
    retrySaveDrawing.onResize(300, 300);
    retrySaveDrawing.m_filePath = "/blocked.modr";
    retrySaveDrawing.beginPathPrompt(monolith::app::DrawingApp::PathPromptMode::Save);
    retrySaveDrawing.m_pathPromptBuffer = "/blocked";
    retrySaveDrawing.m_pathPromptCursorPos = 4;
    retrySaveDrawing.m_pathPromptScrollPx = 13;
    retrySaveDrawing.finishPathPrompt(true);
    check(retrySaveDrawing.m_pathPromptMode == monolith::app::DrawingApp::PathPromptMode::Save
              && retrySaveDrawing.m_pathPromptBuffer == "/blocked"
              && retrySaveDrawing.m_pathPromptCursorPos == 4
              && retrySaveDrawing.m_pathPromptScrollPx == 13
              && retrySaveDrawing.m_filePath == "/blocked.modr",
          "failed Drawing Save keeps its path prompt and current file binding");
    retrySaveDrawing.m_pathPromptBuffer = "/drawings/retry-save";
    retrySaveDrawing.m_pathPromptCursorPos = retrySaveDrawing.m_pathPromptBuffer.size();
    retrySaveDrawing.finishPathPrompt(true);
    check(retrySaveDrawing.m_pathPromptMode == monolith::app::DrawingApp::PathPromptMode::None
              && retrySaveDrawing.m_filePath == "/drawings/retry-save.modr"
              && fs.isFile("/drawings/retry-save.modr"),
          "correcting a failed Drawing Save path retries successfully");

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
        TestDrawing partialUpload(font, &fs);
        partialUpload.resizeCanvas(128, 96, false);
        partialUpload.syncTexture(renderer);
        check(!partialUpload.m_textureDirty
                  && partialUpload.m_dirtyTextureRect.w == 0
                  && partialUpload.m_dirtyTextureRect.h == 0,
              "Drawing uploads a newly created canvas texture in full");
        check(recordPixelEdit(partialUpload, 9, 13, 1, 2, 3)
                  && partialUpload.m_textureDirty
                  && partialUpload.m_dirtyTextureRect.x == 9
                  && partialUpload.m_dirtyTextureRect.y == 13
                  && partialUpload.m_dirtyTextureRect.w == 1
                  && partialUpload.m_dirtyTextureRect.h == 1,
              "Drawing bounds a sparse pixel edit to its changed texture region");
        partialUpload.syncTexture(renderer);
        check(!partialUpload.m_textureDirty
                  && partialUpload.m_dirtyTextureRect.w == 0
                  && partialUpload.m_dirtyTextureRect.h == 0,
              "Drawing clears the dirty texture region after its partial upload");
        SDL_Rect canvasTarget{0, 0, 128, 96};
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, partialUpload.m_canvasTexture, nullptr, &canvasTarget);
        std::vector<uint8_t> uploadedPixels(128 * 96 * 4);
        const bool readUploadedPixels = SDL_RenderReadPixels(
            renderer, &canvasTarget, SDL_PIXELFORMAT_RGBA32,
            uploadedPixels.data(), 128 * 4) == 0;
        const auto uploadedPixelIs = [&](int x, int y, uint8_t r, uint8_t g, uint8_t b) {
            const size_t offset = (static_cast<size_t>(y) * 128 + x) * 4;
            return uploadedPixels[offset] == r && uploadedPixels[offset + 1] == g
                && uploadedPixels[offset + 2] == b && uploadedPixels[offset + 3] == 255;
        };
        check(readUploadedPixels && uploadedPixelIs(9, 13, 1, 2, 3)
                  && uploadedPixelIs(0, 0, 245, 245, 248),
              "Drawing partial texture uploads update edited pixels and preserve untouched pixels");
        partialUpload.beginSparseHistory();
        partialUpload.setPixel(40, 7, 4, 5, 6);
        partialUpload.setPixel(12, 31, 7, 8, 9);
        partialUpload.recordSparseHistoryChange();
        partialUpload.finishSparseHistory();
        check(partialUpload.m_dirtyTextureRect.x == 12
                  && partialUpload.m_dirtyTextureRect.y == 7
                  && partialUpload.m_dirtyTextureRect.w == 29
                  && partialUpload.m_dirtyTextureRect.h == 25,
              "Drawing merges separated pixel edits into the smallest enclosing upload region");
        partialUpload.undoCanvas();
        check(partialUpload.m_dirtyTextureRect.x == 0
                  && partialUpload.m_dirtyTextureRect.y == 0
                  && partialUpload.m_dirtyTextureRect.w == 64
                  && partialUpload.m_dirtyTextureRect.h == 32,
              "Drawing undo expands the upload region to the restored history tile");
        partialUpload.syncTexture(renderer);
        partialUpload.beginSparseHistory();
        partialUpload.setPixel(60, 50, 90, 91, 92);
        partialUpload.recordSparseHistoryChange();
        partialUpload.finishSparseHistory();
        partialUpload.syncTexture(renderer);
        partialUpload.floodFill(60, 50);
        check(partialUpload.m_dirtyTextureRect.x == 60
                  && partialUpload.m_dirtyTextureRect.y == 50
                  && partialUpload.m_dirtyTextureRect.w == 1
                  && partialUpload.m_dirtyTextureRect.h == 1,
              "Drawing Fill tracks its changed span without dirtying the full canvas");
        partialUpload.markTextureDirty();
        partialUpload.setPixel(70, 60, 3, 4, 5);
        check(partialUpload.m_dirtyTextureRect.x == 0
                  && partialUpload.m_dirtyTextureRect.y == 0
                  && partialUpload.m_dirtyTextureRect.w == 128
                  && partialUpload.m_dirtyTextureRect.h == 96,
              "Drawing preserves a pending full refresh when new pixel edits arrive");
        partialUpload.clearCanvas(false);
        check(partialUpload.m_dirtyTextureRect.x == 0
                  && partialUpload.m_dirtyTextureRect.y == 0
                  && partialUpload.m_dirtyTextureRect.w == 128
                  && partialUpload.m_dirtyTextureRect.h == 96,
              "Drawing Clear marks the full canvas texture for upload");
        check(partialUpload.loadFromPath("/drawings/resize.modr")
                  && partialUpload.m_dirtyTextureRect.x == 0
                  && partialUpload.m_dirtyTextureRect.y == 0
                  && partialUpload.m_dirtyTextureRect.w == 2
                  && partialUpload.m_dirtyTextureRect.h == 2,
              "Drawing load marks the replacement canvas for a full texture upload");
        partialUpload.syncTexture(renderer);

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
        const SDL_Color toolbarTextColor{225, 228, 235, 255};
        const auto toolbarTexture = drawing.m_textTextureCache.get(
            renderer, font, "New", toolbarTextColor);
        const size_t cachedTextureCount = drawing.m_textTextureCache.size();
        drawing.render(renderer, {0, 0, 280, 280});
        const auto repeatedToolbarTexture = drawing.m_textTextureCache.get(
            renderer, font, "New", toolbarTextColor);
        check(toolbarTexture
                  && repeatedToolbarTexture.handle == toolbarTexture.handle
                  && drawing.m_textTextureCache.size() == cachedTextureCount,
              "Drawing reuses cached toolbar text textures between frames");
        drawing.setStatus("cache invalidation");
        drawing.render(renderer, {0, 0, 280, 280});
        const auto retainedToolbarTexture = drawing.m_textTextureCache.get(
            renderer, font, "New", toolbarTextColor);
        check(retainedToolbarTexture.handle == toolbarTexture.handle
                  && drawing.m_textTextureCache.size() > cachedTextureCount,
              "Drawing retains toolbar textures and caches changed status text");

        TTF_Font* promptFont = TTF_OpenFont("assets/fonts/DejaVuSans.ttf", 14);
        check(promptFont != nullptr, "Drawing prompt test loads an independent font");
        if (promptFont) {
            auto verifyPromptMetrics = [&]() {
                TestDrawing promptMetrics(promptFont, &fs);
                promptMetrics.onResize(280, 280);
                auto expectedCursorText = [&]() {
                    const std::size_t cursor = std::min(
                        promptMetrics.m_pathPromptCursorPos,
                        promptMetrics.m_pathPromptBuffer.size());
                    return promptMetrics.m_statusMessage + " "
                        + promptMetrics.m_pathPromptBuffer.substr(0, cursor) + "_";
                };
                auto cachedWidthMatches = [&](const std::string& cursorText) {
                    int expectedWidth = 0;
                    int expectedHeight = 0;
                    return TTF_SizeUTF8(promptFont, cursorText.c_str(),
                                       &expectedWidth, &expectedHeight) == 0
                        && promptMetrics.m_promptCursorMeasureValid
                        && promptMetrics.m_promptCursorMeasureText == cursorText
                        && promptMetrics.m_promptCursorPixelWidth == expectedWidth;
                };

                promptMetrics.beginPathPrompt(TestDrawing::PathPromptMode::Open);
                promptMetrics.render(renderer, {0, 0, 280, 280});
                const std::string openCursorText = expectedCursorText();
                const bool openWidthCached = cachedWidthMatches(openCursorText);
                const int openWidth = promptMetrics.m_promptCursorPixelWidth;
                promptMetrics.render(renderer, {0, 0, 280, 280});
                const bool openWidthRetained = cachedWidthMatches(openCursorText)
                    && promptMetrics.m_promptCursorPixelWidth == openWidth;

                promptMetrics.m_pathPromptCursorPos =
                    promptMetrics.m_pathPromptBuffer.size() / 2;
                promptMetrics.render(renderer, {0, 0, 280, 280});
                const std::string movedOpenCursorText = expectedCursorText();
                const bool caretMovementRefreshes =
                    movedOpenCursorText != openCursorText
                    && cachedWidthMatches(movedOpenCursorText);

                promptMetrics.beginPathPrompt(TestDrawing::PathPromptMode::Save);
                promptMetrics.render(renderer, {0, 0, 280, 280});
                const bool saveWidthCached = cachedWidthMatches(expectedCursorText());
                promptMetrics.beginPathPrompt(TestDrawing::PathPromptMode::Rgb);
                promptMetrics.render(renderer, {0, 0, 280, 280});
                const std::string rgbCursorText = expectedCursorText();
                const bool rgbWidthCached = cachedWidthMatches(rgbCursorText);

                const int oldRgbWidth = promptMetrics.m_promptCursorPixelWidth;
                const bool fontResized = TTF_SetFontSize(promptFont, 22) == 0;
                promptMetrics.onUiScaleChanged();
                const bool promptWidthInvalidated =
                    !promptMetrics.m_promptCursorMeasureValid;
                promptMetrics.render(renderer, {0, 0, 280, 280});
                check(openWidthCached && openWidthRetained && caretMovementRefreshes
                          && saveWidthCached && rgbWidthCached && fontResized
                          && promptWidthInvalidated && cachedWidthMatches(rgbCursorText)
                          && promptMetrics.m_promptCursorPixelWidth != oldRgbWidth,
                      "Drawing reuses prompt caret widths and refreshes them after caret or font changes");
            };
            verifyPromptMetrics();
            TTF_CloseFont(promptFont);
        }

        drawing.onUiScaleChanged();
        check(drawing.m_textTextureCache.size() == 0,
              "Drawing clears cached text textures when UI scale changes");
        drawing.m_textTextureCache.clear();
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
