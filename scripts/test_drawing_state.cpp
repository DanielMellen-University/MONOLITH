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

    monolith::app::DrawingApp drawing(nullptr, &fs);
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

    drawing.beginPathPrompt(monolith::app::DrawingApp::PathPromptMode::Save);
    drawing.m_pathPromptBuffer = "/drawings/alternate.modr";
    drawing.m_pathPromptCursorPos = drawing.m_pathPromptBuffer.size();
    drawing.onBoundFileMoved(
        "/drawings/alternate.modr", "/archive/../archive/alternate.modr");
    check(drawing.m_pathPromptBuffer == "/archive/alternate.modr"
              && drawing.m_filePath == "/archive/alternate.modr",
          "Save prompt canonicalizes a moved bound drawing file");
    drawing.beginPathPrompt(monolith::app::DrawingApp::PathPromptMode::Save);
    drawing.m_pathPromptBuffer = "/archive/alternate.modr";
    drawing.m_pathPromptCursorPos = drawing.m_pathPromptBuffer.size();
    drawing.onBoundFileRemoved("/archive/alternate.modr");
    check(drawing.m_pathPromptBuffer == "/archive/",
          "Save prompt returns to a valid parent after deletion");

    std::filesystem::remove_all(hostRoot, ec);
    if (failures == 0) {
        std::cout << "ALL DRAWING STATE TESTS PASSED\n";
        return 0;
    }
    std::cerr << failures << " test(s) failed\n";
    return 1;
}
