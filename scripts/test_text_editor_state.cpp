// Headless regression test for Text Editor Save As identity handling.

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "TestTempDir.hpp"
#include "../src/app/App.hpp"
#include "../src/fs/Filesystem.hpp"

#define private public
#include "../src/app/TextEditorApp.hpp"
#undef private

namespace {

struct TestController final : monolith::app::IWindowController {
    std::string blockedPath;
    std::string focusedPath;
    std::string boundPath;
    std::vector<std::string> lifecycleEvents;
    monolith::app::TextEditorApp* editor = nullptr;
    int closeRequests = 0;

    void close() override { ++closeRequests; }
    void setTitle(const std::string&) override {}

    bool focusEditorForFile(const std::string& path) override {
        focusedPath = path;
        return path == blockedPath;
    }

    void bindEditorFile(const std::string& path) override {
        boundPath = path;
        lifecycleEvents.push_back("bind:" + path);
    }

    void notifyVirtualPathCreated(const std::string& path) override {
        lifecycleEvents.push_back("created:" + path);
    }

    void notifyVirtualPathChanged(const std::string& path) override {
        lifecycleEvents.push_back("changed:" + path);
        if (editor) editor->onVirtualPathChanged(path);
    }
};

struct TestEditor final : monolith::app::TextEditorApp {
    using monolith::app::App::setController;

    TestEditor(TTF_Font* font, monolith::fs::Filesystem* fs, const std::string& path)
        : TextEditorApp(font, fs, path) {}
};

void prepareSaveAs(TestEditor& editor, const std::string& path) {
    editor.m_pathPromptMode = TestEditor::PathPromptMode::SaveAs;
    editor.m_pathPromptBuffer = path;
    editor.m_pathPromptCursorPos = path.size();
}

void prepareOpen(TestEditor& editor, const std::string& path) {
    editor.m_pathPromptMode = TestEditor::PathPromptMode::Open;
    editor.m_pathPromptBuffer = path;
    editor.m_pathPromptCursorPos = path.size();
}

void sendKey(TestEditor& editor, SDL_Keycode key, SDL_Keymod modifiers = KMOD_NONE) {
    SDL_Event event{};
    event.type = SDL_KEYDOWN;
    event.key.keysym.sym = key;
    event.key.keysym.mod = modifiers;
    editor.handleEvent(event);
}

void setEditorLines(TestEditor& editor, std::vector<std::string> lines) {
    editor.m_lines = std::move(lines);
    size_t bytes = editor.m_lines.empty() ? 0 : editor.m_lines.size() - 1;
    for (const auto& line : editor.m_lines) bytes += line.size();
    editor.m_documentSerializedBytes = bytes;
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

    monolith::test::ScopedTempDirectory temp("monolith-text-editor-state");
    if (!temp) {
        std::cerr << "FAIL: could not create Text Editor state directory\n";
        return 1;
    }
    const std::filesystem::path hostRoot = temp.path();

    monolith::fs::Filesystem fs(hostRoot.string());
    check(fs.initialize(), "editor state filesystem initialize");
    const bool videoReady = SDL_Init(SDL_INIT_VIDEO) == 0;
    check(videoReady, "editor state SDL initialize");
    const bool ttfReady = TTF_Init() == 0;
    check(ttfReady, "editor state SDL_ttf initialize");
    TestEditor welcomeEditor(nullptr, &fs, "");
    size_t welcomeSerializedBytes = 0;
    const bool welcomeFits = TestEditor::documentFitsFileLimits(
        welcomeEditor.m_lines, &welcomeSerializedBytes);
    check(welcomeEditor.m_lines.back()
                  == "Use Ctrl+F to find text or Ctrl+H to replace it."
              && welcomeFits
              && welcomeEditor.m_documentSerializedBytes == welcomeSerializedBytes
              && !welcomeEditor.m_dirty,
          "welcome text advertises current Find/Replace shortcuts and keeps a clean size baseline");
    TTF_Font* scaleFont = ttfReady
        ? TTF_OpenFont("assets/fonts/DejaVuSans.ttf", 14)
        : nullptr;
    check(scaleFont != nullptr, "editor state loads test font");
    if (scaleFont) {
        TestEditor hitTestEditor(scaleFont, &fs, "");
        hitTestEditor.m_lines = {"A\xC3\xA9\xF0\x9F\x99\x82Z"};
        hitTestEditor.onResize(320, 120);
        int unicodePrefixWidth = 0;
        int unicodePrefixHeight = 0;
        const bool unicodePrefixMeasured = TTF_SizeUTF8(
            scaleFont, "A\xC3\xA9", &unicodePrefixWidth, &unicodePrefixHeight) == 0;
        int hitRow = -1;
        int hitCol = -1;
        const bool unicodeHitMapped = hitTestEditor.clientToDocument(
            TestEditor::kPadding + TestEditor::kLineNumWidth + unicodePrefixWidth + 1,
            TestEditor::kPadding + 1, hitRow, hitCol);
        check(unicodePrefixMeasured && unicodeHitMapped && hitRow == 0 && hitCol == 3,
              "Text Editor maps UTF-8 hit-test positions to byte offsets");

        TestEditor prefixMeasureEditor(scaleFont, &fs, "");
        const std::string prefixMeasureLine(2048, 'm');
        int expectedPrefixWidth = 0;
        int expectedPrefixHeight = 0;
        const std::string expectedPrefix = prefixMeasureLine.substr(0, 1536);
        const bool expectedPrefixMeasured = TTF_SizeUTF8(
            scaleFont, expectedPrefix.c_str(), &expectedPrefixWidth,
            &expectedPrefixHeight) == 0;
        const int firstPrefixWidth = prefixMeasureEditor.measureTextPrefixWidth(
            prefixMeasureLine, 1536);
        const size_t prefixScratchCapacity =
            prefixMeasureEditor.m_textPrefixMeasureScratch.capacity();
        const int repeatedPrefixWidth = prefixMeasureEditor.measureTextPrefixWidth(
            prefixMeasureLine, 1536);
        check(expectedPrefixMeasured && firstPrefixWidth == expectedPrefixWidth
                  && repeatedPrefixWidth == expectedPrefixWidth
                  && prefixMeasureEditor.m_textPrefixMeasureScratch == expectedPrefix
                  && prefixScratchCapacity >= expectedPrefix.size()
                  && prefixMeasureEditor.m_textPrefixMeasureScratch.capacity()
                      == prefixScratchCapacity,
              "Text Editor reuses prefix measurement storage without changing measured width");
        const size_t retainedPrefixCapacity =
            prefixMeasureEditor.m_textPrefixMeasureScratch.capacity();
        const std::string oversizedMeasureLine(
            TestEditor::kMaxRetainedTextPrefixBytes * 2, 'x');
        check(prefixMeasureEditor.measureTextPrefixWidth(
                      oversizedMeasureLine, static_cast<int>(oversizedMeasureLine.size()))
                      > 0
                  && prefixMeasureEditor.m_textPrefixMeasureScratch.capacity()
                      == retainedPrefixCapacity,
              "Text Editor bounds retained prefix scratch for exceptionally long lines");

        constexpr int longLineHitColumn = 70'000;
        const std::string longHitTestLine(100'000, 'x');
        int longPrefixWidth = 0;
        int longPrefixHeight = 0;
        const bool longPrefixMeasured = TTF_SizeUTF8(
            scaleFont, longHitTestLine.substr(0, longLineHitColumn).c_str(),
            &longPrefixWidth, &longPrefixHeight) == 0;
        hitTestEditor.m_lines = {longHitTestLine};
        hitTestEditor.onResize(320, 120);
        hitTestEditor.m_horizontalScrollOffset = longPrefixWidth;
        const bool longLineHitMapped = hitTestEditor.clientToDocument(
            TestEditor::kPadding + TestEditor::kLineNumWidth + 1,
            TestEditor::kPadding + 1, hitRow, hitCol);
        check(longPrefixMeasured && longLineHitMapped && hitRow == 0
                  && hitCol == longLineHitColumn,
              "Text Editor maps far-scrolled long-line clicks in one text measurement");

        TestEditor scaleEditor(scaleFont, &fs, "/old.txt");
        const int baseStatusBarHeight = scaleEditor.getStatusBarHeight();
        check(TTF_SetFontSize(scaleFont, 22) == 0,
              "Text Editor state scales test font");
        scaleEditor.onUiScaleChanged();
        check(scaleEditor.getStatusBarHeight() > baseStatusBarHeight,
              "Text Editor status bar grows with the shared interface font");
        scaleEditor.m_lines.assign(20, "line");
        scaleEditor.m_cursorRow = 19;
        scaleEditor.m_scrollOffset = 19;
        scaleEditor.onResize(320, 80);
        const int resizedVisibleLines = std::max(
            1,
            scaleEditor.getVisibleLineCount({0, 0, 320, 80}));
        check(scaleEditor.m_scrollOffset == 20 - resizedVisibleLines,
              "Text Editor resize clamps vertical scrollback to the visible lines");
        check(scaleEditor.getVisibleLineCount({0, 0, 320, 40}) == 0,
              "Text Editor reports no rows when the status bar fills a tiny client");
        scaleEditor.m_lines = {"line"};
        scaleEditor.m_cursorRow = 0;
        scaleEditor.m_cursorCol = 0;
        scaleEditor.m_horizontalScrollOffset = 800;
        scaleEditor.onResize(1200, 80);
        check(scaleEditor.m_horizontalScrollOffset == 0,
              "Text Editor clamps horizontal scroll after widening the client");
        scaleEditor.m_cursorRow = 0;
        scaleEditor.m_scrollOffset = 0;
        scaleEditor.m_selectingWithMouse = false;
        const int visibleForGapTest = scaleEditor.getVisibleLineCount({0, 0, 320, 80});
        const int gapY = TestEditor::kPadding
            + visibleForGapTest * scaleEditor.getLineHeight();
        SDL_Event gapEvent{};
        gapEvent.type = SDL_MOUSEBUTTONDOWN;
        gapEvent.button.button = SDL_BUTTON_LEFT;
        gapEvent.button.clicks = 1;
        gapEvent.button.x = TestEditor::kPadding + TestEditor::kLineNumWidth + 4;
        gapEvent.button.y = gapY;
        scaleEditor.handleEvent(gapEvent);
        check(!scaleEditor.m_selectingWithMouse && scaleEditor.m_cursorRow == 0,
              "Text Editor ignores clicks in the gap below the last rendered row");

        scaleEditor.m_lines = {"first", "second", "third"};
        scaleEditor.onResize(320, 160);
        scaleEditor.m_cursorRow = 0;
        scaleEditor.m_cursorCol = 0;
        scaleEditor.m_scrollOffset = 0;
        scaleEditor.m_selectingWithMouse = false;
        SDL_Event selectionStart{};
        selectionStart.type = SDL_MOUSEBUTTONDOWN;
        selectionStart.button.button = SDL_BUTTON_LEFT;
        selectionStart.button.clicks = 1;
        selectionStart.button.x = TestEditor::kPadding + TestEditor::kLineNumWidth + 2;
        selectionStart.button.y = TestEditor::kPadding + 1;
        scaleEditor.handleEvent(selectionStart);
        SDL_Event selectionMotion = selectionStart;
        selectionMotion.type = SDL_MOUSEMOTION;
        selectionMotion.motion.x = 320;
        selectionMotion.motion.y = 200;
        scaleEditor.handleEvent(selectionMotion);
        check(scaleEditor.m_selectingWithMouse
                  && scaleEditor.m_cursorRow == static_cast<int>(scaleEditor.m_lines.size()) - 1
                  && scaleEditor.m_cursorCol == static_cast<int>(scaleEditor.m_lines.back().size()),
              "Text Editor extends a captured selection to the nearest document edge");
        SDL_Event selectionEnd{};
        selectionEnd.type = SDL_MOUSEBUTTONUP;
        selectionEnd.button.button = SDL_BUTTON_LEFT;
        scaleEditor.handleEvent(selectionEnd);

        TestEditor focusGestureEditor(nullptr, &fs, "/old.txt");
        focusGestureEditor.m_lines = {"focus gesture"};
        focusGestureEditor.onResize(320, 120);
        focusGestureEditor.handleEvent(selectionStart);
        focusGestureEditor.onFocusLost();
        check(!focusGestureEditor.m_selectingWithMouse,
              "Text Editor focus loss ends an active mouse selection");

        TestEditor promptGestureEditor(nullptr, &fs, "/old.txt");
        promptGestureEditor.m_lines = {"prompt gesture"};
        promptGestureEditor.onResize(320, 120);
        SDL_Event promptSelectionStart{};
        promptSelectionStart.type = SDL_MOUSEBUTTONDOWN;
        promptSelectionStart.button.button = SDL_BUTTON_LEFT;
        promptSelectionStart.button.x = TestEditor::kPadding + TestEditor::kLineNumWidth + 2;
        promptSelectionStart.button.y = TestEditor::kPadding + 1;
        promptGestureEditor.handleEvent(promptSelectionStart);
        promptGestureEditor.beginPathPrompt(TestEditor::PathPromptMode::Open);
        check(!promptGestureEditor.m_selectingWithMouse,
              "Text Editor path prompts end an active mouse selection");
        promptGestureEditor.finishPathPrompt(false);

        SDL_Surface* surface = videoReady
            ? SDL_CreateRGBSurfaceWithFormat(0, 240, 200, 32, SDL_PIXELFORMAT_RGBA32)
            : nullptr;
        SDL_Renderer* renderer = surface ? SDL_CreateSoftwareRenderer(surface) : nullptr;
        check(renderer != nullptr, "editor state creates a software renderer");
        if (renderer) {
            const SDL_Rect expectedClip{5, 6, 140, 120};
            SDL_RenderSetClipRect(renderer, &expectedClip);
            scaleEditor.render(renderer, {0, 0, 200, 160});
            SDL_Rect restoredClip{};
            SDL_RenderGetClipRect(renderer, &restoredClip);
            check(restoredClip.x == expectedClip.x
                      && restoredClip.y == expectedClip.y
                      && restoredClip.w == expectedClip.w
                      && restoredClip.h == expectedClip.h,
                  "Text Editor restores the caller renderer clip after rendering");
            scaleEditor.m_externalChangePending = true;
            scaleEditor.setStatus("Copied selection.");
            scaleEditor.render(renderer, {0, 0, 200, 160});
            check(scaleEditor.m_renderStatusText.find("[external change]")
                      != std::string::npos,
                  "Text Editor keeps the external-change marker in its status bar");
            scaleEditor.m_externalChangePending = false;
            scaleEditor.render(renderer, {0, 0, 200, 160});
            const auto firstTextTexture = scaleEditor.m_textTextureCache.get(
                renderer, scaleFont, "first", {200, 205, 210, 255});
            const size_t cachedTextureCount = scaleEditor.m_textTextureCache.size();
            scaleEditor.render(renderer, {0, 0, 200, 160});
            const auto repeatedTextTexture = scaleEditor.m_textTextureCache.get(
                renderer, scaleFont, "first", {200, 205, 210, 255});
            check(firstTextTexture && repeatedTextTexture.handle == firstTextTexture.handle
                      && scaleEditor.m_textTextureCache.size() == cachedTextureCount,
                  "Text Editor reuses renderer textures between unchanged frames");
            const std::size_t statusTextCapacity = scaleEditor.m_renderStatusText.capacity();
            scaleEditor.render(renderer, {0, 0, 200, 160});
            check(scaleEditor.m_renderStatusText.capacity() == statusTextCapacity,
                  "Text Editor reuses status text storage between unchanged frames");

            TTF_Font* promptFont = TTF_OpenFont("assets/fonts/DejaVuSans.ttf", 14);
            check(promptFont != nullptr, "Text Editor prompt test loads an independent font");
            if (promptFont) {
                TestEditor promptMetricEditor(promptFont, &fs, "/prompt-metrics.txt");
                promptMetricEditor.onResize(240, 200);
                promptMetricEditor.m_searchMode = TestEditor::SearchMode::Find;
                promptMetricEditor.m_findQuery = "needle";
                promptMetricEditor.m_findCursorPos = 3;
                promptMetricEditor.render(renderer, {0, 0, 240, 200});
                const std::string findCursorText = "Find: nee_";
                const std::size_t findStatusCapacity =
                    promptMetricEditor.m_renderStatusText.capacity();
                const std::size_t findCursorCapacity =
                    promptMetricEditor.m_renderCursorText.capacity();
                const std::string findStatusText = promptMetricEditor.m_renderStatusText;
                int expectedCursorWidth = 0;
                int expectedCursorHeight = 0;
                const bool findWidthMeasured = TTF_SizeUTF8(
                    promptFont, findCursorText.c_str(), &expectedCursorWidth,
                    &expectedCursorHeight) == 0;
                const bool findPromptCached = findWidthMeasured
                    && promptMetricEditor.m_statusCursorMeasureValid
                    && promptMetricEditor.m_statusCursorMeasureText == findCursorText
                    && promptMetricEditor.m_statusCursorPixelWidth == expectedCursorWidth;
                promptMetricEditor.render(renderer, {0, 0, 240, 200});
                const bool findPromptRetained = findPromptCached
                    && promptMetricEditor.m_statusCursorMeasureText == findCursorText
                    && promptMetricEditor.m_statusCursorPixelWidth == expectedCursorWidth;
                check(findPromptRetained
                          && promptMetricEditor.m_renderStatusText == findStatusText
                          && promptMetricEditor.m_renderStatusText.capacity() == findStatusCapacity
                          && promptMetricEditor.m_renderCursorText.capacity() == findCursorCapacity,
                      "Text Editor reuses Find prompt and caret buffers between unchanged frames");
                promptMetricEditor.m_findCursorPos = promptMetricEditor.m_findQuery.size();
                promptMetricEditor.render(renderer, {0, 0, 240, 200});
                const std::string movedFindCursorText = "Find: needle_";
                expectedCursorWidth = 0;
                const bool movedFindWidthMeasured = TTF_SizeUTF8(
                    promptFont, movedFindCursorText.c_str(), &expectedCursorWidth,
                    &expectedCursorHeight) == 0;
                const bool findCursorMovementRefreshes = movedFindWidthMeasured
                    && promptMetricEditor.m_statusCursorMeasureText == movedFindCursorText
                    && promptMetricEditor.m_statusCursorPixelWidth == expectedCursorWidth;

                promptMetricEditor.m_searchMode = TestEditor::SearchMode::Replace;
                promptMetricEditor.m_searchField = TestEditor::SearchField::Replacement;
                promptMetricEditor.m_replaceText = "value";
                promptMetricEditor.m_replaceCursorPos = 2;
                promptMetricEditor.render(renderer, {0, 0, 240, 200});
                const std::string replaceCursorText = "Find: needle  Repl: va_";
                expectedCursorWidth = 0;
                const bool replaceWidthMeasured = TTF_SizeUTF8(
                    promptFont, replaceCursorText.c_str(), &expectedCursorWidth,
                    &expectedCursorHeight) == 0;
                const bool replacePromptCached = replaceWidthMeasured
                    && promptMetricEditor.m_statusCursorMeasureText == replaceCursorText
                    && promptMetricEditor.m_statusCursorPixelWidth == expectedCursorWidth;

                promptMetricEditor.m_searchMode = TestEditor::SearchMode::None;
                promptMetricEditor.m_pathPromptMode = TestEditor::PathPromptMode::GoToLine;
                promptMetricEditor.m_pathPromptBuffer = "128";
                promptMetricEditor.m_pathPromptCursorPos = 1;
                promptMetricEditor.render(renderer, {0, 0, 240, 200});
                const std::string goToLineCursorText = "Go to line: 1_";
                expectedCursorWidth = 0;
                const bool goToLineWidthMeasured = TTF_SizeUTF8(
                    promptFont, goToLineCursorText.c_str(), &expectedCursorWidth,
                    &expectedCursorHeight) == 0;
                const bool goToLinePromptCached = goToLineWidthMeasured
                    && promptMetricEditor.m_statusCursorMeasureText == goToLineCursorText
                    && promptMetricEditor.m_statusCursorPixelWidth == expectedCursorWidth;

                promptMetricEditor.m_pathPromptMode = TestEditor::PathPromptMode::Open;
                promptMetricEditor.m_pathPromptBuffer = "/home/monolith/";
                promptMetricEditor.m_pathPromptCursorPos =
                    promptMetricEditor.m_pathPromptBuffer.size();
                promptMetricEditor.render(renderer, {0, 0, 240, 200});
                const std::string openCursorText = "Open: /home/monolith/_";
                const std::size_t openStatusCapacity =
                    promptMetricEditor.m_renderStatusText.capacity();
                const std::size_t openCursorCapacity =
                    promptMetricEditor.m_renderCursorText.capacity();
                const std::string openStatusText = promptMetricEditor.m_renderStatusText;
                expectedCursorWidth = 0;
                const bool openWidthMeasured = TTF_SizeUTF8(
                    promptFont, openCursorText.c_str(), &expectedCursorWidth,
                    &expectedCursorHeight) == 0;
                const bool openPromptCached = openWidthMeasured
                    && promptMetricEditor.m_statusCursorMeasureText == openCursorText
                    && promptMetricEditor.m_statusCursorPixelWidth == expectedCursorWidth;
                promptMetricEditor.render(renderer, {0, 0, 240, 200});
                check(openPromptCached
                          && promptMetricEditor.m_renderStatusText == openStatusText
                          && promptMetricEditor.m_renderStatusText.capacity() == openStatusCapacity
                          && promptMetricEditor.m_renderCursorText.capacity() == openCursorCapacity,
                      "Text Editor reuses Open prompt and caret buffers between unchanged frames");

                promptMetricEditor.m_pathPromptMode = TestEditor::PathPromptMode::SaveAs;
                promptMetricEditor.render(renderer, {0, 0, 240, 200});
                const std::string saveAsCursorText = "Save as: /home/monolith/_";
                expectedCursorWidth = 0;
                const bool saveAsWidthMeasured = TTF_SizeUTF8(
                    promptFont, saveAsCursorText.c_str(), &expectedCursorWidth,
                    &expectedCursorHeight) == 0;
                const bool saveAsPromptCached = saveAsWidthMeasured
                    && promptMetricEditor.m_statusCursorMeasureText == saveAsCursorText
                    && promptMetricEditor.m_statusCursorPixelWidth == expectedCursorWidth;

                const int oldPromptWidth = promptMetricEditor.m_statusCursorPixelWidth;
                const bool fontResized = TTF_SetFontSize(promptFont, 22) == 0;
                promptMetricEditor.onUiScaleChanged();
                const bool promptMetricsInvalidated = !promptMetricEditor.m_statusCursorMeasureValid;
                promptMetricEditor.render(renderer, {0, 0, 240, 200});
                expectedCursorWidth = 0;
                const bool scaledWidthMeasured = TTF_SizeUTF8(
                    promptFont, saveAsCursorText.c_str(), &expectedCursorWidth,
                    &expectedCursorHeight) == 0;
                const bool scaledPromptMetricsRetained = scaledWidthMeasured
                    && promptMetricEditor.m_statusCursorMeasureText == saveAsCursorText
                    && promptMetricEditor.m_statusCursorPixelWidth == expectedCursorWidth;
                promptMetricEditor.m_searchMode = TestEditor::SearchMode::Replace;
                promptMetricEditor.m_searchField = TestEditor::SearchField::Replacement;
                promptMetricEditor.m_findQuery.assign(
                    TestEditor::kMaxDocumentBytes - 4, 'q');
                promptMetricEditor.m_findQuery.append("\xF0\x9F\x99\x82");
                promptMetricEditor.m_findCursorPos = promptMetricEditor.m_findQuery.size();
                promptMetricEditor.m_replaceText.assign(
                    TestEditor::kMaxDocumentBytes, 'r');
                promptMetricEditor.m_replaceCursorPos = 256;
                promptMetricEditor.updateFindMatches();
                promptMetricEditor.render(renderer, {0, 0, 240, 200});
                const bool largeSearchPromptBounded =
                    promptMetricEditor.m_renderStatusText.size() < 512
                    && promptMetricEditor.m_renderCursorText.size() < 256
                    && promptMetricEditor.m_statusCursorMeasureText.size() < 256
                    && promptMetricEditor.m_renderStatusText.find("...")
                        != std::string::npos
                    && promptMetricEditor.m_renderStatusText.find("\xF0\x9F\x99\x82")
                        != std::string::npos;
                check(findPromptRetained && findCursorMovementRefreshes
                          && replacePromptCached && goToLinePromptCached
                          && openPromptCached && saveAsPromptCached
                          && fontResized && promptMetricsInvalidated && scaledWidthMeasured
                          && scaledPromptMetricsRetained
                          && expectedCursorWidth != oldPromptWidth,
                      "Text Editor reuses prompt caret widths across Find, Replace, path prompts, and UI scaling");
                check(largeSearchPromptBounded,
                      "Text Editor renders bounded UTF-8 excerpts for maximum-size search fields");
                TTF_CloseFont(promptFont);
            }

            TestEditor findViewportEditor(scaleFont, &fs, "/find-viewport.txt");
            findViewportEditor.m_lines.assign(256, "target target");
            findViewportEditor.m_findQuery = "target";
            findViewportEditor.updateFindMatches();
            findViewportEditor.onResize(240, 200);
            findViewportEditor.m_scrollOffset = 128;
            findViewportEditor.m_cursorRow = 0;
            findViewportEditor.m_currentFindMatch = 257;
            findViewportEditor.m_hasCurrentFindMatch = true;
            findViewportEditor.m_currentFindPosition =
                findViewportEditor.findMatchAtIndex(257);
            SDL_BlendMode oldDrawBlend = SDL_BLENDMODE_NONE;
            SDL_GetRenderDrawBlendMode(renderer, &oldDrawBlend);
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderClear(renderer);
            findViewportEditor.render(renderer, {0, 0, 240, 200});
            const int visibleFindRows = findViewportEditor.getVisibleLineCount({0, 0, 240, 200});
            const bool cachedFindGeometry =
                findViewportEditor.m_renderedFindStartRow == 128
                && findViewportEditor.m_renderedFindLineCount == visibleFindRows
                && findViewportEditor.m_renderedFindPrefixWidths.size()
                    == static_cast<size_t>(visibleFindRows * 2)
                && findViewportEditor.m_renderedFindVisibleMatches.size()
                    == findViewportEditor.m_renderedFindPrefixWidths.size()
                && findViewportEditor.m_renderedFindVisibleWidths.size()
                    == findViewportEditor.m_renderedFindPrefixWidths.size();
            const auto* findGeometryStorage =
                findViewportEditor.m_renderedFindPrefixWidths.data();
            const auto* findHighlightWidthStorage =
                findViewportEditor.m_renderedFindVisibleWidths.data();
            findViewportEditor.render(renderer, {0, 0, 240, 200});
            const bool retainedFindGeometry = cachedFindGeometry
                && findGeometryStorage
                    == findViewportEditor.m_renderedFindPrefixWidths.data()
                && findHighlightWidthStorage
                    == findViewportEditor.m_renderedFindVisibleWidths.data();
            int matchPrefixWidth = 0;
            int matchPrefixHeight = 0;
            TTF_SizeUTF8(scaleFont, "target ", &matchPrefixWidth, &matchPrefixHeight);
            const int matchRowY = TestEditor::kPadding + 1;
            Uint8 inactiveMatchPixel[4]{};
            Uint8 activeMatchPixel[4]{};
            const SDL_Rect inactiveSample = {
                TestEditor::kPadding + TestEditor::kLineNumWidth + 1,
                matchRowY, 1, 1
            };
            const SDL_Rect activeSample = {
                TestEditor::kPadding + TestEditor::kLineNumWidth + matchPrefixWidth + 1,
                matchRowY, 1, 1
            };
            const bool readMatchSamples =
                SDL_RenderReadPixels(renderer, &inactiveSample, SDL_PIXELFORMAT_RGBA32,
                                     inactiveMatchPixel, sizeof(inactiveMatchPixel)) == 0
                && SDL_RenderReadPixels(renderer, &activeSample, SDL_PIXELFORMAT_RGBA32,
                                        activeMatchPixel, sizeof(activeMatchPixel)) == 0;
            SDL_SetRenderDrawBlendMode(renderer, oldDrawBlend);
            check(findViewportEditor.m_findMatchCount == 512
                      && findViewportEditor.findMatchAtIndex(257)
                          == std::pair<int, int>{128, 7}
                      && readMatchSamples
                      && inactiveMatchPixel[0] == 36 && inactiveMatchPixel[1] == 48
                      && inactiveMatchPixel[2] == 58
                      && activeMatchPixel[0] == 54 && activeMatchPixel[1] == 92
                      && activeMatchPixel[2] == 116
                      && retainedFindGeometry,
                  "Find reuses viewport highlight geometry and preserves active styling after scrolling");
            findViewportEditor.moveFindMatch(1);
            findViewportEditor.render(renderer, {0, 0, 240, 200});
            const bool navigationReusesGeometry =
                findViewportEditor.m_currentFindMatch == 258
                && findViewportEditor.m_renderedFindStartRow == 128
                && findGeometryStorage
                    == findViewportEditor.m_renderedFindPrefixWidths.data()
                && findHighlightWidthStorage
                    == findViewportEditor.m_renderedFindVisibleWidths.data();
            findViewportEditor.m_scrollOffset = 129;
            findViewportEditor.render(renderer, {0, 0, 240, 200});
            const bool viewportGeometryRebuilt =
                findViewportEditor.m_renderedFindStartRow == 129
                && findViewportEditor.m_renderedFindLineCount == visibleFindRows
                      && !findViewportEditor.m_renderedFindPrefixWidths.empty();
            findViewportEditor.m_findQuery = "target ";
            findViewportEditor.updateFindMatches();
            const bool queryGeometryInvalidated =
                findViewportEditor.m_renderedFindStartRow == -1
                && findViewportEditor.m_renderedFindPrefixWidths.empty()
                && findViewportEditor.m_renderedFindVisibleWidths.empty();
            findViewportEditor.render(renderer, {0, 0, 240, 200});
            const bool queryGeometryRebuilt =
                findViewportEditor.m_renderedFindStartRow == findViewportEditor.m_scrollOffset
                && !findViewportEditor.m_renderedFindPrefixWidths.empty();
            findViewportEditor.onUiScaleChanged();
            const bool fontGeometryInvalidated =
                findViewportEditor.m_renderedFindStartRow == -1
                && findViewportEditor.m_renderedFindPrefixWidths.empty()
                && findViewportEditor.m_renderedFindVisibleWidths.empty();
            check(navigationReusesGeometry && viewportGeometryRebuilt
                      && queryGeometryInvalidated && queryGeometryRebuilt
                      && fontGeometryInvalidated,
                  "Find query and font changes invalidate cached highlight geometry");

            TestEditor multilineRenderEditor(scaleFont, &fs, "");
            multilineRenderEditor.m_lines = {"A", "B", "A", "B"};
            multilineRenderEditor.m_searchMode = TestEditor::SearchMode::Find;
            multilineRenderEditor.m_findQuery = "A\nB";
            multilineRenderEditor.updateFindMatches();
            multilineRenderEditor.onResize(240, 200);
            SDL_BlendMode oldMultilineBlend = SDL_BLENDMODE_NONE;
            SDL_GetRenderDrawBlendMode(renderer, &oldMultilineBlend);
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
            multilineRenderEditor.render(renderer, {0, 0, 240, 200});
            const auto* multilineGeometryStorage =
                multilineRenderEditor.m_renderedFindVisibleMatches.data();
            const int multilineLineHeight = multilineRenderEditor.getLineHeight();
            const SDL_Rect firstMultilineSample = {
                TestEditor::kPadding + TestEditor::kLineNumWidth + 1,
                TestEditor::kPadding + 1, 1, 1
            };
            const SDL_Rect secondMultilineSample = {
                TestEditor::kPadding + TestEditor::kLineNumWidth + 1,
                TestEditor::kPadding + 2 * multilineLineHeight + 1, 1, 1
            };
            Uint8 firstActivePixel[4]{};
            Uint8 secondInactivePixel[4]{};
            const bool firstMultilineSamplesRead =
                SDL_RenderReadPixels(renderer, &firstMultilineSample,
                                     SDL_PIXELFORMAT_RGBA32, firstActivePixel,
                                     sizeof(firstActivePixel)) == 0
                && SDL_RenderReadPixels(renderer, &secondMultilineSample,
                                        SDL_PIXELFORMAT_RGBA32, secondInactivePixel,
                                        sizeof(secondInactivePixel)) == 0;
            multilineRenderEditor.moveFindMatch(1);
            multilineRenderEditor.render(renderer, {0, 0, 240, 200});
            Uint8 firstInactivePixel[4]{};
            Uint8 secondActivePixel[4]{};
            const bool secondMultilineSamplesRead =
                SDL_RenderReadPixels(renderer, &firstMultilineSample,
                                     SDL_PIXELFORMAT_RGBA32, firstInactivePixel,
                                     sizeof(firstInactivePixel)) == 0
                && SDL_RenderReadPixels(renderer, &secondMultilineSample,
                                        SDL_PIXELFORMAT_RGBA32, secondActivePixel,
                                        sizeof(secondActivePixel)) == 0;
            SDL_SetRenderDrawBlendMode(renderer, oldMultilineBlend);
            check(multilineRenderEditor.m_renderedFindVisibleMatches.size() == 4
                      && multilineRenderEditor.m_renderedFindVisibleMatches.data()
                          == multilineGeometryStorage,
                  "multiline Find caches visible fragments for each row without rebuilding on navigation");
            check(multilineRenderEditor.m_renderStatusText.find("Find: _A\\nB")
                          != std::string::npos
                      && multilineRenderEditor.m_renderStatusText.find(
                             "Ctrl+Enter newline") != std::string::npos,
                  "multiline Find escapes query line breaks and advertises Ctrl+Enter in the status field");
            check(firstMultilineSamplesRead && secondMultilineSamplesRead
                      && firstActivePixel[0] == 54 && firstActivePixel[1] == 92
                      && firstActivePixel[2] == 116
                      && secondInactivePixel[0] == 36 && secondInactivePixel[1] == 48
                      && secondInactivePixel[2] == 58,
                  "multiline Find initially highlights every visible fragment of the active match");
            check(multilineRenderEditor.m_currentFindPosition
                          == std::pair<int, int>{2, 0}
                      && firstInactivePixel[0] == 36 && firstInactivePixel[1] == 48
                      && firstInactivePixel[2] == 58
                      && secondActivePixel[0] == 54 && secondActivePixel[1] == 92
                      && secondActivePixel[2] == 116,
                  "multiline Find moves active styling between cached matches");

            TestEditor denseViewportEditor(scaleFont, &fs, "/dense-find.txt");
            denseViewportEditor.m_lines = {std::string(100'000, 'x')};
            denseViewportEditor.m_findQuery = "x";
            denseViewportEditor.updateFindMatches();
            denseViewportEditor.onResize(240, 200);
            denseViewportEditor.render(renderer, {0, 0, 240, 200});
            const std::size_t denseViewportHighlightLimit =
                static_cast<std::size_t>(denseViewportEditor.getVisibleLineCount(
                    {0, 0, 240, 200}) * 64);
            check(denseViewportEditor.m_findMatchCount == 100'000
                      && !denseViewportEditor.m_findCheckpoints.empty()
                      && denseViewportEditor.m_renderedFindVisibleMatches.size()
                          <= denseViewportHighlightLimit,
                  "Find caches only viewport-intersecting highlights for dense long lines");

            TestEditor clippedFindEditor(scaleFont, &fs, "/clipped-find.txt");
            clippedFindEditor.m_lines = {std::string(12'000, 'x')};
            clippedFindEditor.m_findQuery.assign(4096, 'x');
            clippedFindEditor.updateFindMatches();
            clippedFindEditor.onResize(240, 200);
            int farPrefixWidth = 0;
            int farPrefixHeight = 0;
            const std::string farPrefix(6000, 'x');
            const bool measuredFarOffset = TTF_SizeUTF8(
                scaleFont, farPrefix.c_str(), &farPrefixWidth, &farPrefixHeight) == 0;
            clippedFindEditor.m_horizontalScrollOffset = farPrefixWidth;
            clippedFindEditor.render(renderer, {0, 0, 240, 200});
            const int clippedTextWidth = 240 - 2 * TestEditor::kPadding
                - TestEditor::kLineNumWidth;
            check(measuredFarOffset && clippedFindEditor.m_findMatchCount == 2
                      && clippedFindEditor.m_renderedFindVisibleMatches.size() == 1,
                  "long Find retains only the match intersecting the scrolled viewport");
            check(clippedFindEditor.m_renderedFindVisibleWidths.size() == 1
                      && clippedFindEditor.m_renderedFindVisibleWidths[0] > 0
                      && clippedFindEditor.m_renderedFindVisibleWidths[0] <= clippedTextWidth,
                  "long Find highlights are measured only over their visible slice");

            TestEditor syntaxCacheEditor(scaleFont, &fs, "");
            syntaxCacheEditor.m_syntaxMode = TestEditor::SyntaxMode::Code;
            setEditorLines(syntaxCacheEditor, {"int x; /* open", "return 4; */"});
            syntaxCacheEditor.onResize(240, 200);
            syntaxCacheEditor.render(renderer, {0, 0, 240, 200});
            const bool cachedVisibleSyntax =
                syntaxCacheEditor.m_renderedSyntaxSpans.size() == 2
                && !syntaxCacheEditor.m_renderedSyntaxSpans[1].empty()
                && syntaxCacheEditor.m_renderedSyntaxSpans[1][0].color.g == 145;
            const auto* syntaxCacheStorage =
                syntaxCacheEditor.m_renderedSyntaxSpans.data();
            syntaxCacheEditor.render(renderer, {0, 0, 240, 200});
            const bool retainedVisibleSyntax = cachedVisibleSyntax
                && syntaxCacheStorage == syntaxCacheEditor.m_renderedSyntaxSpans.data()
                && syntaxCacheEditor.m_renderedSyntaxSpans[1][0].color.g == 145;
            syntaxCacheEditor.m_cursorRow = 0;
            syntaxCacheEditor.m_cursorCol = 7;
            syntaxCacheEditor.deleteForward();
            const bool editInvalidatedVisibleSyntax =
                syntaxCacheEditor.m_renderedSyntaxStartRow == -1
                && syntaxCacheEditor.m_renderedSyntaxSpans.empty();
            syntaxCacheEditor.render(renderer, {0, 0, 240, 200});
            check(retainedVisibleSyntax && editInvalidatedVisibleSyntax
                      && !syntaxCacheEditor.m_renderedSyntaxSpans[1].empty()
                      && syntaxCacheEditor.m_renderedSyntaxSpans[1][0].color.b == 225,
                  "Text Editor reuses viewport syntax spans and refreshes downstream colors after edits");
            syntaxCacheEditor.m_filePath = "/plain.txt";
            syntaxCacheEditor.refreshSyntaxMode();
            const bool modeInvalidatedVisibleSyntax =
                syntaxCacheEditor.m_syntaxMode == TestEditor::SyntaxMode::Light
                && syntaxCacheEditor.m_renderedSyntaxStartRow == -1
                && syntaxCacheEditor.m_renderedSyntaxSpans.empty();
            syntaxCacheEditor.render(renderer, {0, 0, 240, 200});
            check(modeInvalidatedVisibleSyntax
                      && syntaxCacheEditor.m_renderedSyntaxSpans.size() == 2
                      && !syntaxCacheEditor.m_renderedSyntaxSpans[1].empty()
                      && syntaxCacheEditor.m_renderedSyntaxSpans[1][0].color.r == 200
                      && syntaxCacheEditor.m_renderedSyntaxSpans[1][0].color.g == 205,
                  "Text Editor rebuilds viewport spans when the file switches syntax modes");

            const size_t beforeStatus = scaleEditor.m_textTextureCache.size();
            scaleEditor.setStatus("cache invalidation");
            scaleEditor.render(renderer, {0, 0, 200, 160});
            check(scaleEditor.m_textTextureCache.size() > beforeStatus,
                  "Text Editor caches changed status text without flushing document textures");
            const size_t beforeEdit = scaleEditor.m_textTextureCache.size();
            scaleEditor.m_cursorRow = 0;
            scaleEditor.m_cursorCol = 0;
            scaleEditor.insertText("x");
            scaleEditor.render(renderer, {0, 0, 200, 160});
            check(scaleEditor.m_textTextureCache.size() > beforeEdit,
                  "Text Editor caches changed document text without flushing prior textures");
            scaleEditor.onUiScaleChanged();
            check(scaleEditor.m_textTextureCache.size() == 0
                      && scaleEditor.m_renderedTextSlices.empty()
                      && scaleEditor.m_renderedTextSliceStartRow == -1,
                  "Text Editor clears renderer textures and viewport measurements when UI scale changes");

            TestEditor longLineEditor(scaleFont, &fs, "/long.txt");
            std::string longLine;
            longLine.reserve(12000);
            for (int i = 0; i < 1200; ++i) longLine += "abcdefghi\xC3\xA9";
            longLineEditor.m_lines = {longLine};
            longLineEditor.onResize(200, 160);
            longLineEditor.render(renderer, {0, 0, 200, 160});
            const bool measuredInitialTextSlice =
                longLineEditor.m_renderedTextSlices.size() == 1
                && longLineEditor.m_renderedTextSlices[0].measured
                && longLineEditor.m_renderedTextSlices[0].valid
                && longLineEditor.m_renderedTextSlices[0].firstVisibleByte == 0
                && longLineEditor.m_renderedTextSlices[0].visibleEndByte > 0
                && longLineEditor.m_renderedTextSlices[0].visibleEndByte < longLine.size();
            const auto* textSliceStorage = longLineEditor.m_renderedTextSlices.data();
            longLineEditor.render(renderer, {0, 0, 200, 160});
            const bool retainedTextSlice = measuredInitialTextSlice
                && textSliceStorage == longLineEditor.m_renderedTextSlices.data();
            const int longLineViewportWidth = 200
                - 2 * TestEditor::kPadding - TestEditor::kLineNumWidth;
            auto firstViewportText = std::find_if(
                longLineEditor.m_textTextureCache.m_entries.begin(),
                longLineEditor.m_textTextureCache.m_entries.end(),
                [&longLine](const auto& entry) {
                    return entry.first.text.size() >= 9
                        && entry.first.text.compare(0, 9, longLine, 0, 9) == 0;
                });
            check(firstViewportText != longLineEditor.m_textTextureCache.m_entries.end()
                      && firstViewportText->first.text.size()
                          < longLine.size() / 20 + 5
                      && firstViewportText->second.width
                          < longLineViewportWidth + 2 * TTF_FontHeight(scaleFont),
                  "Text Editor caches only viewport-sized textures for very long lines");
            check(retainedTextSlice,
                  "Text Editor reuses UTF-8 viewport measurements across unchanged frames");

            const auto firstViewportKey = firstViewportText
                != longLineEditor.m_textTextureCache.m_entries.end()
                ? firstViewportText->first
                : monolith::detail::TextTextureCache::CacheKey{};
            SDL_Texture* firstViewportTexture = firstViewportText
                != longLineEditor.m_textTextureCache.m_entries.end()
                ? firstViewportText->second.handle
                : nullptr;
            longLineEditor.m_horizontalScrollOffset = 60;
            longLineEditor.render(renderer, {0, 0, 200, 160});
            const bool horizontalSliceUpdated =
                longLineEditor.m_renderedTextSliceOffset == 60
                && longLineEditor.m_renderedTextSlices.size() == 1
                && longLineEditor.m_renderedTextSlices[0].firstVisibleByte > 0
                && longLineEditor.m_renderedTextSlices[0].visibleEndByte
                    > longLineEditor.m_renderedTextSlices[0].firstVisibleByte;
            const size_t scrolledTextureCount = longLineEditor.m_textTextureCache.size();
            auto retainedViewportText = longLineEditor.m_textTextureCache.m_entries.find(
                firstViewportKey);
            check(!firstViewportKey.text.empty() && retainedViewportText
                      != longLineEditor.m_textTextureCache.m_entries.end()
                      && retainedViewportText->second.handle == firstViewportTexture
                      && scrolledTextureCount
                          <= monolith::detail::TextTextureCache::kMaxEntries
                      && longLineEditor.m_textTextureCache.estimatedBytes()
                          <= monolith::detail::TextTextureCache::kMaxEstimatedBytes
                      && horizontalSliceUpdated,
                  "Text Editor retains scrolled textures within the bounded cache");
            longLineEditor.m_horizontalScrollOffset = 0;
            longLineEditor.render(renderer, {0, 0, 200, 160});
            check(longLineEditor.m_textTextureCache.size() == scrolledTextureCount,
                  "Text Editor reuses the prior viewport texture when scrolling back");
            longLineEditor.render(renderer, {0, 0, 220, 160});
            check(longLineEditor.m_renderedTextSliceWidth
                      == 220 - 2 * TestEditor::kPadding - TestEditor::kLineNumWidth,
                  "Text Editor recalculates visible byte bounds when its content width changes");

            scaleEditor.m_lines.assign(20, "line");
            scaleEditor.m_cursorRow = 19;
            scaleEditor.m_cursorCol = 0;
            scaleEditor.m_scrollOffset = 19;
            scaleEditor.render(renderer, {0, 0, 320, 80});
            const int directRenderVisible = std::max(
                1, scaleEditor.getVisibleLineCount({0, 0, 320, 80}));
            check(scaleEditor.m_clientWidth == 320
                      && scaleEditor.m_clientHeight == 80
                      && scaleEditor.m_scrollOffset == 20 - directRenderVisible,
                  "Text Editor direct renders synchronize client geometry and scroll bounds");
            scaleEditor.m_textTextureCache.clear();
            longLineEditor.m_textTextureCache.clear();
            SDL_DestroyRenderer(renderer);
        }
        if (surface) SDL_FreeSurface(surface);
    }
    check(fs.writeFile("/old.txt", "original"), "write original editor file");
    check(fs.writeFile("/retry-open.txt", "retry target"), "write editor retry target");
    check(fs.writeFile("/empty.txt", ""), "write empty editor file");
    check(fs.writeFile("/windows.txt", "first\r\nsecond\r\n"),
          "write CRLF editor file");
    check(fs.writeFile("/classic-mac.txt", "first\rsecond\r"),
          "write lone-CR editor file");
    std::string chunkBoundaryContent(16 * 1024 - 1, 'x');
    chunkBoundaryContent += "\r\nend\r";
    check(fs.writeFile("/chunk-boundary.txt", chunkBoundaryContent),
          "write editor line endings across a read-chunk boundary");
    check(fs.writeFile("/oversized.txt",
                       std::string(TestEditor::kMaxDocumentBytes + 1, 'x')),
          "write editor file above the byte limit");
    check(fs.writeFile("/too-many-lines.txt",
                       std::string(TestEditor::kMaxDocumentLines, '\n')),
          "write editor file above the line limit");
    check(fs.writeFile("/max-lines.txt",
                       std::string(TestEditor::kMaxDocumentLines - 1, '\n')),
          "write editor file at the line limit");
    check(fs.createDirectory("/folder"), "create unwritable save target directory");
    check(fs.createDirectory("/unicode"), "create Unicode completion directory");
    check(fs.createDirectory("/home/monolith/documents"),
          "create virtual-home path prompt directory");
    check(fs.writeFile("/home/monolith/documents/home-note.txt", "home prompt target"),
          "write virtual-home path prompt target");
    const std::string eAcuteName = std::string("\xC3\xA9") + "clair.txt";
    const std::string eCircumflexName = std::string("\xC3\xAA") + "clair.txt";
    check(fs.writeFile("/unicode/" + eAcuteName, "acute"),
          "write first Unicode completion candidate");
    check(fs.writeFile("/unicode/" + eCircumflexName, "circumflex"),
          "write second Unicode completion candidate");

    TestEditor emptyEditor(nullptr, &fs, "/empty.txt");
    check(emptyEditor.m_lines == std::vector<std::string>{""}
              && emptyEditor.m_documentSerializedBytes == 0,
          "empty files open as one editable blank line");

    TestEditor windowsEditor(nullptr, &fs, "/windows.txt");
    check(windowsEditor.m_lines == std::vector<std::string>{"first", "second", ""}
              && windowsEditor.m_documentSerializedBytes == 13,
          "CRLF files open with normalized line endings");

    TestEditor classicMacEditor(nullptr, &fs, "/classic-mac.txt");
    check(classicMacEditor.m_lines == std::vector<std::string>{"first", "second", ""}
              && classicMacEditor.m_documentSerializedBytes == 13,
          "lone-CR files open with normalized line endings");

    TestEditor chunkBoundaryEditor(nullptr, &fs, "/chunk-boundary.txt");
    check(chunkBoundaryEditor.m_lines.size() == 3
              && chunkBoundaryEditor.m_lines[0] == std::string(16 * 1024 - 1, 'x')
              && chunkBoundaryEditor.m_lines[1] == "end"
              && chunkBoundaryEditor.m_lines[2].empty()
              && chunkBoundaryEditor.m_documentSerializedBytes == 16 * 1024 + 4,
          "streamed editor open normalizes CRLF split across chunks and trailing CR");

    TestEditor maxLinesEditor(nullptr, &fs, "/max-lines.txt");
    check(maxLinesEditor.m_lines.size() == TestEditor::kMaxDocumentLines
              && maxLinesEditor.m_lines.front().empty()
              && maxLinesEditor.m_lines.back().empty()
              && maxLinesEditor.m_documentSerializedBytes
                  == TestEditor::kMaxDocumentLines - 1,
          "Text Editor accepts a file exactly at its line limit");

    TestEditor boundedEditor(nullptr, &fs, "/old.txt");
    boundedEditor.m_lines = {"unsaved buffer"};
    boundedEditor.m_savedLines = {"saved baseline"};
    boundedEditor.m_dirty = true;
    const auto linesBeforeLimitedOpen = boundedEditor.m_lines;
    check(!boundedEditor.loadInitialFile("/oversized.txt")
              && boundedEditor.m_lines == linesBeforeLimitedOpen
              && boundedEditor.m_savedLines == std::vector<std::string>{"saved baseline"}
              && boundedEditor.m_filePath == "/old.txt"
              && boundedEditor.m_dirty
              && boundedEditor.m_statusMessage.find("16 MiB") != std::string::npos,
          "oversized streamed open preserves the current dirty document");
    check(!boundedEditor.loadInitialFile("/too-many-lines.txt")
              && boundedEditor.m_lines == linesBeforeLimitedOpen
              && boundedEditor.m_filePath == "/old.txt"
              && boundedEditor.m_dirty
              && boundedEditor.m_statusMessage.find("65,536") != std::string::npos,
          "over-limit line count is rejected without replacing the current document");

    TestEditor retryOpenEditor(nullptr, &fs, "/old.txt");
    prepareOpen(retryOpenEditor, "/oversized.txt");
    retryOpenEditor.m_pathPromptCursorPos = 5;
    retryOpenEditor.m_statusHorizontalScrollPx = 19;
    retryOpenEditor.finishPathPrompt(true);
    check(retryOpenEditor.m_pathPromptMode == TestEditor::PathPromptMode::Open
              && retryOpenEditor.m_pathPromptBuffer == "/oversized.txt"
              && retryOpenEditor.m_pathPromptCursorPos == 5
              && retryOpenEditor.m_statusHorizontalScrollPx == 19
              && retryOpenEditor.m_filePath == "/old.txt"
              && retryOpenEditor.m_lines == std::vector<std::string>{"original"},
          "rejected oversized Open keeps its prompt text, caret, scroll, and document");
    retryOpenEditor.m_pathPromptBuffer = "/retry-open.txt";
    retryOpenEditor.m_pathPromptCursorPos = retryOpenEditor.m_pathPromptBuffer.size();
    retryOpenEditor.finishPathPrompt(true);
    check(retryOpenEditor.m_pathPromptMode == TestEditor::PathPromptMode::None
              && retryOpenEditor.m_filePath == "/retry-open.txt"
              && retryOpenEditor.m_lines == std::vector<std::string>{"retry target"},
          "correcting a rejected Open path retries successfully");

    TestEditor retryLineEditor(nullptr, &fs, "/old.txt");
    retryLineEditor.m_lines = {"first", "second", "third"};
    retryLineEditor.beginPathPrompt(TestEditor::PathPromptMode::GoToLine);
    retryLineEditor.m_pathPromptBuffer = "0";
    retryLineEditor.m_pathPromptCursorPos = 1;
    retryLineEditor.m_statusHorizontalScrollPx = 7;
    retryLineEditor.finishPathPrompt(true);
    check(retryLineEditor.m_pathPromptMode == TestEditor::PathPromptMode::GoToLine
              && retryLineEditor.m_pathPromptBuffer == "0"
              && retryLineEditor.m_pathPromptCursorPos == 1
              && retryLineEditor.m_statusHorizontalScrollPx == 7,
          "invalid Go to Line input remains editable for correction");
    retryLineEditor.m_pathPromptBuffer = "2";
    retryLineEditor.m_pathPromptCursorPos = 1;
    retryLineEditor.finishPathPrompt(true);
    check(retryLineEditor.m_pathPromptMode == TestEditor::PathPromptMode::None
              && retryLineEditor.m_cursorRow == 1,
          "correcting invalid Go to Line input completes the jump");

    TestEditor oversizedSaveEditor(nullptr, &fs, "");
    oversizedSaveEditor.m_filePath = "/oversized-save.txt";
    oversizedSaveEditor.m_lines = {
        std::string(TestEditor::kMaxDocumentBytes + 1, 'x')};
    oversizedSaveEditor.m_dirty = true;
    check(!oversizedSaveEditor.saveCurrentFile()
              && !fs.exists("/oversized-save.txt")
              && oversizedSaveEditor.m_statusMessage.find("16 MiB") != std::string::npos,
          "Text Editor refuses saves beyond its byte limit");
    oversizedSaveEditor.m_lines.assign(TestEditor::kMaxDocumentLines + 1, "");
    check(!oversizedSaveEditor.saveCurrentFile()
              && !fs.exists("/oversized-save.txt")
              && oversizedSaveEditor.m_statusMessage.find("65,536") != std::string::npos,
          "Text Editor refuses saves beyond its line limit");

    TestEditor boundarySaveEditor(nullptr, &fs, "");
    boundarySaveEditor.m_filePath = "/maximum-valid-save.txt";
    boundarySaveEditor.m_lines = {
        std::string(TestEditor::kMaxDocumentBytes, 'z')};
    std::uint64_t maximumSaveBytes = 0;
    size_t streamedMaximumBytes = 0;
    bool streamedMaximumContentsMatch = true;
    check(boundarySaveEditor.saveCurrentFile()
              && fs.fileSize(boundarySaveEditor.m_filePath, maximumSaveBytes)
              && maximumSaveBytes == TestEditor::kMaxDocumentBytes
              && fs.readFileChunks(boundarySaveEditor.m_filePath,
                  [&](std::string_view chunk) {
                      for (const char byte : chunk) {
                          if (byte != 'z') {
                              streamedMaximumContentsMatch = false;
                              return false;
                          }
                          ++streamedMaximumBytes;
                      }
                      return true;
                  })
              && streamedMaximumBytes == TestEditor::kMaxDocumentBytes
              && streamedMaximumContentsMatch,
          "Text Editor streams exact content at its 16 MiB byte limit");

    TestEditor serializationEditor(nullptr, &fs, "");
    serializationEditor.m_filePath = "/serialized-lines.txt";
    serializationEditor.m_lines = {"first", "", "last", ""};
    check(serializationEditor.saveCurrentFile()
              && fs.readFile(serializationEditor.m_filePath)
                  == "first\n\nlast\n",
          "Text Editor preserves blank lines and a trailing newline when saving");

    TestEditor chunkedSaveEditor(nullptr, &fs, "");
    chunkedSaveEditor.m_filePath = "/chunked-save.txt";
    chunkedSaveEditor.m_lines = {
        std::string(16 * 1024 - 1, 'a'), "b", std::string(16 * 1024, 'c'), "", "end"};
    const std::string chunkedSaveExpected =
        std::string(16 * 1024 - 1, 'a') + "\nb\n"
        + std::string(16 * 1024, 'c') + "\n\nend";
    check(chunkedSaveEditor.saveCurrentFile()
              && fs.readFile(chunkedSaveEditor.m_filePath) == chunkedSaveExpected,
          "Text Editor preserves line separators across bounded save chunks");

    TestEditor editor(nullptr, &fs, "/old.txt");
    TestController controller;
    editor.setController(&controller);

    controller.blockedPath = "/blocked.txt";
    prepareSaveAs(editor, "/blocked.txt");
    editor.finishPathPrompt(true);
    check(controller.focusedPath == "/blocked.txt",
          "Save As checks for an existing editor binding");
    check(editor.m_filePath == "/old.txt",
          "rejected Save As preserves the current file path");
    check(!fs.exists("/blocked.txt"), "rejected Save As does not write a new file");

    controller.blockedPath.clear();
    prepareSaveAs(editor, "/folder");
    editor.m_pathPromptCursorPos = 3;
    editor.m_statusHorizontalScrollPx = 23;
    editor.finishPathPrompt(true);
    check(editor.m_filePath == "/old.txt"
              && editor.m_pathPromptMode == TestEditor::PathPromptMode::SaveAs
              && editor.m_pathPromptBuffer == "/folder"
              && editor.m_pathPromptCursorPos == 3
              && editor.m_statusHorizontalScrollPx == 23,
          "failed Save As preserves the binding and editable prompt state");
    check(fs.isDirectory("/folder"), "failed Save As leaves the existing target intact");

    editor.m_pathPromptBuffer = "/new.txt";
    editor.m_pathPromptCursorPos = editor.m_pathPromptBuffer.size();
    editor.finishPathPrompt(true);
    check(editor.m_filePath == "/new.txt"
              && editor.m_pathPromptMode == TestEditor::PathPromptMode::None,
          "correcting a failed Save As path updates the file binding and closes the prompt");
    check(fs.readFile("/new.txt") == "original", "successful Save As writes the document");
    check(controller.boundPath == "/new.txt", "successful Save As updates the shell binding");
    check(controller.lifecycleEvents.size() >= 2
              && controller.lifecycleEvents[0] == "bind:/new.txt"
              && controller.lifecycleEvents[1] == "created:/new.txt",
          "editor claims a new file before broadcasting its creation");

    TestEditor externalEditor(nullptr, &fs, "/new.txt");
    TestController externalController;
    externalEditor.setController(&externalController);
    check(fs.writeFile("/new.txt", "outside change"),
          "overwrite the editor file outside the editor");
    externalEditor.onVirtualPathChanged("/new.txt");
    check(externalEditor.m_lines == std::vector<std::string>{"original"}
              && externalEditor.m_externalChangePending
              && externalEditor.m_statusMessage.find("External file changed") != std::string::npos,
          "external overwrite warns without replacing the editor buffer");
    externalEditor.setStatus("Copied selection.");
    check(externalEditor.m_externalChangePending,
          "routine Text Editor status updates retain the external-change state");
    externalController.blockedPath = "/new.txt";
    prepareOpen(externalEditor, "/new.txt");
    externalEditor.finishPathPrompt(true);
    check(externalEditor.m_filePath == "/new.txt"
              && externalEditor.m_lines == std::vector<std::string>{"outside change"}
              && !externalEditor.m_dirty && !externalEditor.m_externalChangePending,
          "Open reloads the current file instead of focusing its own singleton window");

    externalEditor.m_lines = {"local edit"};
    externalEditor.m_dirty = true;
    check(fs.writeFile("/new.txt", "newer external change"),
          "write a second external editor version");
    externalEditor.onVirtualPathChanged("/new.txt");
    prepareOpen(externalEditor, "/new.txt");
    externalEditor.finishPathPrompt(true);
    check(externalEditor.m_discardKind == TestEditor::DiscardKind::Open
              && externalEditor.m_lines == std::vector<std::string>{"local edit"},
          "reloading the same file still confirms before discarding dirty editor text");
    externalEditor.finishPathPrompt(true);
    check(externalEditor.m_discardKind == TestEditor::DiscardKind::Open
              && externalEditor.m_lines == std::vector<std::string>{"local edit"},
          "repeating Enter does not discard a dirty editor buffer");
    sendKey(externalEditor, SDLK_d, KMOD_CTRL);
    check(externalEditor.m_lines == std::vector<std::string>{"newer external change"}
              && !externalEditor.m_dirty && !externalEditor.m_externalChangePending,
          "Ctrl+D explicitly confirms a same-file reload");

    TestEditor selfSaveEditor(nullptr, &fs, "/new.txt");
    TestController selfSaveController;
    selfSaveEditor.setController(&selfSaveController);
    selfSaveController.editor = &selfSaveEditor;
    selfSaveEditor.onVirtualPathChanged("/new.txt");
    selfSaveEditor.m_lines = {"editor save"};
    selfSaveEditor.m_dirty = true;
    check(!selfSaveEditor.saveCurrentFile()
              && selfSaveEditor.m_overwriteConfirmationPending
              && fs.readFile("/new.txt") == "newer external change",
          "the first save preserves an external version and asks before overwriting");
    check(!selfSaveEditor.saveCurrentFile()
              && selfSaveEditor.m_overwriteConfirmationPending
              && fs.readFile("/new.txt") == "newer external change",
          "repeated direct saves cannot bypass the pending overwrite decision");
    sendKey(selfSaveEditor, SDLK_ESCAPE);
    check(!selfSaveEditor.m_overwriteConfirmationPending
              && selfSaveEditor.m_externalChangePending
              && fs.readFile("/new.txt") == "newer external change",
          "Escape cancels an external-overwrite attempt without changing either version");
    sendKey(selfSaveEditor, SDLK_s, KMOD_CTRL);
    check(selfSaveEditor.m_overwriteConfirmationPending
              && fs.readFile("/new.txt") == "newer external change",
          "a new save attempt requires a fresh overwrite confirmation");
    sendKey(selfSaveEditor, SDLK_d, KMOD_CTRL);
    check(fs.readFile("/new.txt") == "editor save",
          "Ctrl+D explicitly confirms replacing the external file version");
    check(selfSaveEditor.m_statusMessage == "Saved: new.txt"
              && !selfSaveEditor.m_suppressChangedNotification
              && !selfSaveEditor.m_externalChangePending,
          "the editor ignores its own synchronous change notification");

    TestEditor hostChangeEditor(nullptr, &fs, "/new.txt");
    hostChangeEditor.m_lines = {"stale local version"};
    hostChangeEditor.m_dirty = true;
    check(fs.writeFile("/new.txt", "host-side version"),
          "write a host-side version without sending an app notification");
    check(!hostChangeEditor.saveCurrentFile()
              && hostChangeEditor.m_externalChangePending
              && hostChangeEditor.m_overwriteConfirmationPending
              && fs.readFile("/new.txt") == "host-side version",
          "bound save detects an unannounced disk change and preserves it for confirmation");

    check(fs.writeFile("/line-endings.txt", "first\r\nsecond\r"),
          "create a CRLF and CR line-ending fixture");
    TestEditor lineEndingEditor(nullptr, &fs, "/line-endings.txt");
    const monolith::fs::FileStamp originalLineEndingStamp =
        lineEndingEditor.m_savedFileStamp;
    check(fs.writeFile("/line-endings.txt", "first\r\nsecond\r"),
          "replace unchanged line-ending bytes to exercise the exact comparison path");
    monolith::fs::FileStamp replacedLineEndingStamp;
    check(fs.fileStamp("/line-endings.txt", replacedLineEndingStamp)
              && replacedLineEndingStamp != originalLineEndingStamp,
          "atomic replacement changes the bound file stamp");
    lineEndingEditor.m_lines[0] = "edited first";
    lineEndingEditor.m_dirty = true;
    check(lineEndingEditor.saveCurrentFile()
              && !lineEndingEditor.m_externalChangePending
              && fs.readFile("/line-endings.txt") == "edited first\nsecond\n",
          "bound-save comparison normalizes existing CRLF and CR separators");

    check(fs.createDirectory("/blocked.txt"), "create blocked direct-save target");
    editor.m_filePath = "/blocked.txt";
    editor.m_dirty = true;
    check(!editor.allowClose(), "first close arms the dirty editor guard");
    check(editor.m_statusMessage.find("Ctrl+D discard") != std::string::npos,
          "dirty close explains its explicit discard and cancel keys");
    check(!editor.saveCurrentFile(), "direct save failure is reported");
    check(!editor.allowClose(), "failed save clears the stale dirty guard arm");

    TestEditor closeEditor(nullptr, &fs, "/old.txt");
    TestController closeController;
    closeEditor.setController(&closeController);
    closeEditor.m_dirty = true;
    check(!closeEditor.allowClose(), "dirty close first asks for a decision");
    sendKey(closeEditor, SDLK_d, KMOD_CTRL);
    check(closeController.closeRequests == 1 && closeEditor.m_closeDiscardAuthorized,
          "Ctrl+D requests close with one-shot discard authorization");
    check(closeEditor.allowClose() && !closeEditor.m_closeDiscardAuthorized,
          "the window manager's close check consumes the explicit authorization");
    closeEditor.m_dirty = true;
    check(!closeEditor.allowClose(), "a later close asks again after the buffer changes");
    sendKey(closeEditor, SDLK_ESCAPE);
    check(closeEditor.m_dirty
              && closeEditor.m_discardKind == TestEditor::DiscardKind::None,
          "Escape cancels dirty close without changing the buffer");

    check(fs.writeFile("/save-close.txt", "before"), "create save-and-close target");
    TestEditor saveCloseEditor(nullptr, &fs, "/save-close.txt");
    TestController saveCloseController;
    saveCloseEditor.setController(&saveCloseController);
    saveCloseEditor.m_lines = {"after"};
    saveCloseEditor.m_dirty = true;
    check(!saveCloseEditor.allowClose(), "dirty save-and-close asks for a decision");
    sendKey(saveCloseEditor, SDLK_s, KMOD_CTRL);
    check(fs.readFile("/save-close.txt") == "after"
              && !saveCloseEditor.m_dirty
              && saveCloseController.closeRequests == 1,
          "Ctrl+S saves successfully before requesting the pending close");

    check(fs.writeFile("/save-close-conflict.txt", "disk version"),
          "create external-change save-and-close target");
    TestEditor conflictCloseEditor(nullptr, &fs, "/save-close-conflict.txt");
    TestController conflictCloseController;
    conflictCloseEditor.setController(&conflictCloseController);
    conflictCloseEditor.m_lines = {"editor version"};
    conflictCloseEditor.m_dirty = true;
    check(fs.writeFile("/save-close-conflict.txt", "external version"),
          "write external save-and-close version");
    conflictCloseEditor.onVirtualPathChanged("/save-close-conflict.txt");
    check(!conflictCloseEditor.allowClose(),
          "externally changed dirty Editor still asks before close");
    sendKey(conflictCloseEditor, SDLK_s, KMOD_CTRL);
    check(conflictCloseEditor.m_overwriteConfirmationPending
              && conflictCloseController.closeRequests == 0
              && fs.readFile("/save-close-conflict.txt") == "external version",
          "save-and-close pauses before overwriting the external version");
    check(fs.writeFile("/save-close-conflict.txt", "external version 2"),
          "write a second external save-and-close version");
    conflictCloseEditor.onVirtualPathChanged("/save-close-conflict.txt");
    check(conflictCloseEditor.m_overwriteConfirmationPending
              && fs.readFile("/save-close-conflict.txt") == "external version 2",
          "a newer external write keeps the overwrite decision attached to the latest disk version");
    sendKey(conflictCloseEditor, SDLK_o, KMOD_CTRL);
    check(conflictCloseEditor.m_overwriteConfirmationPending
              && conflictCloseEditor.m_pathPromptMode == TestEditor::PathPromptMode::None
              && conflictCloseController.closeRequests == 0,
          "unrelated shortcuts cannot replace a pending overwrite decision");
    sendKey(conflictCloseEditor, SDLK_d, KMOD_CTRL);
    check(!conflictCloseEditor.m_dirty
              && conflictCloseController.closeRequests == 1
              && fs.readFile("/save-close-conflict.txt") == "editor version",
          "explicit overwrite completes the pending Editor close only after save");

    TestEditor untitledCloseEditor(nullptr, &fs, "");
    TestController untitledCloseController;
    untitledCloseEditor.setController(&untitledCloseController);
    untitledCloseEditor.m_lines = {"untitled saved"};
    untitledCloseEditor.m_dirty = true;
    check(!untitledCloseEditor.allowClose(), "untitled dirty close asks for a decision");
    sendKey(untitledCloseEditor, SDLK_s, KMOD_CTRL);
    check(untitledCloseEditor.m_pathPromptMode == TestEditor::PathPromptMode::SaveAs
              && untitledCloseEditor.m_closeAfterSave,
          "saving an untitled document during close starts Save As");
    prepareSaveAs(untitledCloseEditor, "/save-close-as.txt");
    untitledCloseEditor.finishPathPrompt(true);
    check(fs.readFile("/save-close-as.txt") == "untitled saved"
              && !untitledCloseEditor.m_dirty
              && !untitledCloseEditor.m_closeAfterSave
              && untitledCloseController.closeRequests == 1,
          "successful Save As completes an explicitly requested close");

    TestEditor failedSaveCloseEditor(nullptr, &fs, "/blocked.txt");
    TestController failedSaveCloseController;
    failedSaveCloseEditor.setController(&failedSaveCloseController);
    failedSaveCloseEditor.m_filePath = "/blocked.txt";
    failedSaveCloseEditor.m_lines = {"cannot save"};
    failedSaveCloseEditor.m_dirty = true;
    check(!failedSaveCloseEditor.allowClose(), "failed save close asks for a decision");
    sendKey(failedSaveCloseEditor, SDLK_s, KMOD_CTRL);
    check(failedSaveCloseController.closeRequests == 0
              && failedSaveCloseEditor.m_dirty,
          "a failed save never closes or clears the dirty buffer");

    check(fs.writeFile("/other.txt", "other"), "write alternate open target");
    editor.m_dirty = true;
    prepareOpen(editor, "/other.txt");
    editor.finishPathPrompt(true);
    check(editor.m_discardKind == TestEditor::DiscardKind::Open
              && editor.m_pathPromptMode == TestEditor::PathPromptMode::Open,
          "dirty open arms a discard confirmation and keeps the prompt active");
    editor.finishPathPrompt(true);
    check(editor.m_dirty && editor.m_filePath == "/blocked.txt"
              && editor.m_pathPromptMode == TestEditor::PathPromptMode::Open,
          "repeating Enter does not discard the dirty document");
    editor.finishPathPrompt(false);
    check(editor.m_discardKind == TestEditor::DiscardKind::None
              && editor.m_pathPromptMode == TestEditor::PathPromptMode::None,
          "canceling a dirty open clears its discard arm");
    editor.m_dirty = true;
    prepareOpen(editor, "/other.txt");
    editor.finishPathPrompt(true);
    check(editor.m_discardKind == TestEditor::DiscardKind::Open
              && editor.m_pathPromptMode == TestEditor::PathPromptMode::Open
              && editor.m_filePath == "/blocked.txt",
          "a later dirty open requires a fresh confirmation after cancellation");

    check(fs.writeFile("/third.txt", "third"), "write alternate dirty-open target");
    editor.m_dirty = true;
    editor.beginPathPrompt(TestEditor::PathPromptMode::Open);
    editor.m_pathPromptBuffer = "/other.txt";
    editor.m_pathPromptCursorPos = editor.m_pathPromptBuffer.size();
    editor.finishPathPrompt(true);
    editor.m_pathPromptBuffer = "/third.txt";
    editor.m_pathPromptCursorPos = editor.m_pathPromptBuffer.size();
    editor.finishPathPrompt(true);
    check(editor.m_discardKind == TestEditor::DiscardKind::Open
              && editor.m_pathPromptMode == TestEditor::PathPromptMode::Open
              && editor.m_filePath == "/blocked.txt"
              && editor.m_lines == std::vector<std::string>{"original"},
          "changing a dirty open target requires a fresh confirmation");
    sendKey(editor, SDLK_d, KMOD_CTRL);
    check(editor.m_filePath == "/third.txt" && !editor.m_dirty
              && editor.m_lines == std::vector<std::string>{"third"},
          "Ctrl+D explicitly opens the newly confirmed dirty-open target");

    check(fs.writeFile("/fourth.txt", "fourth"), "write directly confirmed dirty-open target");
    editor.m_dirty = true;
    editor.beginPathPrompt(TestEditor::PathPromptMode::Open);
    editor.m_pathPromptBuffer = "/other.txt";
    editor.m_pathPromptCursorPos = editor.m_pathPromptBuffer.size();
    editor.finishPathPrompt(true);
    editor.m_pathPromptBuffer = "/fourth.txt";
    editor.m_pathPromptCursorPos = editor.m_pathPromptBuffer.size();
    sendKey(editor, SDLK_d, KMOD_CTRL);
    check(editor.m_filePath == "/fourth.txt" && !editor.m_dirty
              && editor.m_lines == std::vector<std::string>{"fourth"},
          "Ctrl+D explicitly confirms the currently edited Open target");

    check(fs.writeFile("/open-save.txt", "before save"), "create save-before-open target");
    TestEditor openSaveEditor(nullptr, &fs, "/open-save.txt");
    TestController openSaveController;
    openSaveEditor.setController(&openSaveController);
    openSaveController.editor = &openSaveEditor;
    openSaveEditor.m_lines = {"saved before open"};
    openSaveEditor.m_dirty = true;
    prepareOpen(openSaveEditor, "/other.txt");
    sendKey(openSaveEditor, SDLK_s, KMOD_CTRL);
    check(fs.readFile("/open-save.txt") == "saved before open"
              && openSaveEditor.m_filePath == "/open-save.txt"
              && openSaveEditor.m_pathPromptMode == TestEditor::PathPromptMode::None
              && !openSaveEditor.m_dirty,
          "Ctrl+S in dirty Open saves the current file and cancels that prompt");

    TestEditor undoEditor(nullptr, &fs, "/old.txt");
    undoEditor.m_cursorRow = 0;
    undoEditor.m_cursorCol = 0;
    undoEditor.insertText("X");
    check(undoEditor.m_dirty && undoEditor.m_lines == std::vector<std::string>{"Xoriginal"},
          "editing a loaded document marks it dirty");
    undoEditor.undo();
    check(!undoEditor.m_dirty && undoEditor.m_lines == std::vector<std::string>{"original"},
          "undoing back to the loaded document clears the dirty state");
    undoEditor.redo();
    check(undoEditor.m_dirty && undoEditor.m_lines == std::vector<std::string>{"Xoriginal"},
          "redoing an undone edit restores the dirty state");
    check(undoEditor.saveCurrentFile(), "save the editor dirty-state baseline");
    undoEditor.insertText("!");
    undoEditor.undo();
    check(!undoEditor.m_dirty && undoEditor.m_lines == std::vector<std::string>{"Xoriginal"},
          "undoing after save compares against the new saved content");

    TestEditor findPasteEditor(nullptr, &fs, "");
    findPasteEditor.m_lines = {"a\xC3\xA9", "!b"};
    findPasteEditor.m_searchMode = TestEditor::SearchMode::Find;
    findPasteEditor.m_findQuery = "ab";
    findPasteEditor.m_findCursorPos = 1;
    SDL_Event searchPasteEvent{};
    searchPasteEvent.type = SDL_KEYDOWN;
    searchPasteEvent.key.keysym.mod = KMOD_CTRL;
    searchPasteEvent.key.keysym.sym = SDLK_v;
    const bool setFindClipboard = SDL_SetClipboardText("\xC3\xA9\n!") == 0;
    if (setFindClipboard) findPasteEditor.handleEvent(searchPasteEvent);
    check(setFindClipboard
              && findPasteEditor.m_findQuery == "a\xC3\xA9\n!b"
              && findPasteEditor.m_findCursorPos == 5
              && findPasteEditor.m_findMatchCount == 1,
          "Ctrl+V preserves normalized line breaks and finds multiline clipboard text");

    findPasteEditor.m_lines = {"ax", "yb"};
    findPasteEditor.m_findQuery = "ab";
    findPasteEditor.m_findCursorPos = 1;
    SDL_Event searchTextEvent{};
    searchTextEvent.type = SDL_TEXTINPUT;
    searchTextEvent.text.text[0] = 'x';
    searchTextEvent.text.text[1] = '\n';
    searchTextEvent.text.text[2] = 'y';
    findPasteEditor.handleEvent(searchTextEvent);
    check(findPasteEditor.m_findQuery == "ax\nyb"
              && findPasteEditor.m_findCursorPos == 4
              && findPasteEditor.m_findMatchCount == 1,
          "typed Find input retains line breaks and refreshes multiline matches");

    TestEditor boundedSearchEditor(nullptr, &fs, "");
    boundedSearchEditor.m_searchMode = TestEditor::SearchMode::Find;
    boundedSearchEditor.m_findQuery.assign(TestEditor::kMaxDocumentBytes - 2, 'q');
    boundedSearchEditor.m_findCursorPos = boundedSearchEditor.m_findQuery.size();
    SDL_Event utf8SearchTextEvent{};
    utf8SearchTextEvent.type = SDL_TEXTINPUT;
    utf8SearchTextEvent.text.text[0] = static_cast<char>(0xC3);
    utf8SearchTextEvent.text.text[1] = static_cast<char>(0xA9);
    boundedSearchEditor.handleEvent(utf8SearchTextEvent);
    check(boundedSearchEditor.m_findQuery.size() == TestEditor::kMaxDocumentBytes
              && boundedSearchEditor.m_findQuery.ends_with("\xC3\xA9")
              && boundedSearchEditor.m_findCursorPos == TestEditor::kMaxDocumentBytes,
          "Find accepts a complete UTF-8 character that exactly fills its byte limit");
    SDL_Event excessFindTextEvent{};
    excessFindTextEvent.type = SDL_TEXTINPUT;
    excessFindTextEvent.text.text[0] = 'x';
    boundedSearchEditor.handleEvent(excessFindTextEvent);
    check(boundedSearchEditor.m_findQuery.size() == TestEditor::kMaxDocumentBytes
              && boundedSearchEditor.m_statusMessage
                  == "Find limit reached (16 MiB / 65,536 lines)",
          "typing beyond the Find query limit is rejected with status feedback");

    boundedSearchEditor.m_findQuery.assign(TestEditor::kMaxDocumentBytes - 1, 'q');
    boundedSearchEditor.m_findCursorPos = boundedSearchEditor.m_findQuery.size();
    const bool setPartialFindClipboard = SDL_SetClipboardText("\xC3\xA9") == 0;
    check(setPartialFindClipboard,
          "set UTF-8 clipboard text for a partial-capacity Find query");
    if (setPartialFindClipboard) boundedSearchEditor.handleEvent(searchPasteEvent);
    check(boundedSearchEditor.m_findQuery.size() == TestEditor::kMaxDocumentBytes - 1
              && boundedSearchEditor.m_findQuery.back() == 'q'
              && boundedSearchEditor.m_statusMessage
                  == "Find limit reached (16 MiB / 65,536 lines)",
          "pasting across the Find query limit drops the whole UTF-8 character");

    TestEditor boundedSearchLinesEditor(nullptr, &fs, "");
    boundedSearchLinesEditor.m_searchMode = TestEditor::SearchMode::Find;
    boundedSearchLinesEditor.m_findQuery.assign(TestEditor::kMaxDocumentLines - 1, '\n');
    boundedSearchLinesEditor.m_findQueryLineBreaks =
        TestEditor::kMaxDocumentLines - 1;
    boundedSearchLinesEditor.m_findCursorPos = boundedSearchLinesEditor.m_findQuery.size();
    sendKey(boundedSearchLinesEditor, SDLK_RETURN, KMOD_CTRL);
    check(boundedSearchLinesEditor.m_findQuery.size()
                  == TestEditor::kMaxDocumentLines - 1
              && boundedSearchLinesEditor.m_statusMessage
                  == "Find limit reached (16 MiB / 65,536 lines)",
          "search fields reject a line break beyond their 65,536-line limit");

    boundedSearchEditor.m_searchMode = TestEditor::SearchMode::Replace;
    boundedSearchEditor.m_searchField = TestEditor::SearchField::Replacement;
    boundedSearchEditor.m_replaceText.assign(TestEditor::kMaxDocumentBytes - 1, 'r');
    boundedSearchEditor.m_replaceCursorPos = boundedSearchEditor.m_replaceText.size();
    const bool setPartialReplaceClipboard = SDL_SetClipboardText("\xC3\xA9") == 0;
    check(setPartialReplaceClipboard,
          "set UTF-8 clipboard text for a partial-capacity replacement field");
    if (setPartialReplaceClipboard) boundedSearchEditor.handleEvent(searchPasteEvent);
    check(boundedSearchEditor.m_replaceText.size() == TestEditor::kMaxDocumentBytes - 1
              && boundedSearchEditor.m_replaceText.back() == 'r'
              && boundedSearchEditor.m_statusMessage
                  == "Replacement limit reached (16 MiB / 65,536 lines)",
          "pasting across the replacement limit drops the whole UTF-8 character");
    SDL_Event exactReplaceTextEvent{};
    exactReplaceTextEvent.type = SDL_TEXTINPUT;
    exactReplaceTextEvent.text.text[0] = 'z';
    boundedSearchEditor.handleEvent(exactReplaceTextEvent);
    const bool replacementAcceptsExactFit =
        boundedSearchEditor.m_replaceText.size() == TestEditor::kMaxDocumentBytes
        && boundedSearchEditor.m_replaceCursorPos == TestEditor::kMaxDocumentBytes;
    boundedSearchEditor.handleEvent(exactReplaceTextEvent);
    check(replacementAcceptsExactFit
              && boundedSearchEditor.m_replaceText.size() == TestEditor::kMaxDocumentBytes
              && boundedSearchEditor.m_statusMessage
                  == "Replacement limit reached (16 MiB / 65,536 lines)",
          "typed replacement input reaches but cannot exceed its byte limit");

    SDL_Event ctrlFindEvent{};
    ctrlFindEvent.type = SDL_KEYDOWN;
    ctrlFindEvent.key.keysym.mod = KMOD_CTRL;
    ctrlFindEvent.key.keysym.sym = SDLK_f;
    TestEditor selectedFindEditor(nullptr, &fs, "");
    selectedFindEditor.m_lines = {"before needle after"};
    selectedFindEditor.m_cursorRow = 0;
    selectedFindEditor.m_cursorCol = 13;
    selectedFindEditor.m_selAnchorRow = 0;
    selectedFindEditor.m_selAnchorCol = 7;
    selectedFindEditor.m_hasSelection = true;
    selectedFindEditor.handleEvent(ctrlFindEvent);
    check(selectedFindEditor.m_findQuery == "needle"
              && selectedFindEditor.m_findCursorPos == 6
              && selectedFindEditor.m_currentFindPosition == std::pair<int, int>{0, 7}
              && selectedFindEditor.m_hasSelection,
          "Ctrl+F searches a selected single-line term and keeps its first match selected");

    SDL_Event ctrlReplaceEvent = ctrlFindEvent;
    ctrlReplaceEvent.key.keysym.sym = SDLK_h;
    TestEditor selectedReplaceEditor(nullptr, &fs, "");
    selectedReplaceEditor.m_lines = {"needle needle"};
    selectedReplaceEditor.m_cursorRow = 0;
    selectedReplaceEditor.m_cursorCol = 7;
    selectedReplaceEditor.m_selAnchorRow = 0;
    selectedReplaceEditor.m_selAnchorCol = 13;
    selectedReplaceEditor.m_hasSelection = true;
    selectedReplaceEditor.handleEvent(ctrlReplaceEvent);
    check(selectedReplaceEditor.m_findQuery == "needle"
              && selectedReplaceEditor.m_findCursorPos == 6
              && selectedReplaceEditor.m_currentFindMatch == 1
              && selectedReplaceEditor.m_currentFindPosition == std::pair<int, int>{0, 7}
              && selectedReplaceEditor.m_replaceText.empty(),
          "Ctrl+H pre-fills a reversed single-line selection and selects that occurrence");

    TestEditor multilineFindEditor(nullptr, &fs, "");
    multilineFindEditor.m_lines = {"before", "after"};
    multilineFindEditor.m_cursorRow = 1;
    multilineFindEditor.m_cursorCol = 3;
    multilineFindEditor.m_selAnchorRow = 0;
    multilineFindEditor.m_selAnchorCol = 3;
    multilineFindEditor.m_hasSelection = true;
    multilineFindEditor.handleEvent(ctrlFindEvent);
    check(multilineFindEditor.m_findQuery == "ore\naft"
              && multilineFindEditor.m_findMatchCount == 1
              && multilineFindEditor.m_currentFindPosition == std::pair<int, int>{0, 3}
              && multilineFindEditor.m_currentFindEndPosition == std::pair<int, int>{1, 3}
              && multilineFindEditor.hasSelection(),
          "Ctrl+F seeds and selects the occurrence of a multi-line selection");

    TestEditor controlFindEditor(nullptr, &fs, "");
    controlFindEditor.m_lines = {"before\t after"};
    controlFindEditor.m_cursorRow = 0;
    controlFindEditor.m_cursorCol = 13;
    controlFindEditor.m_selAnchorRow = 0;
    controlFindEditor.m_selAnchorCol = 6;
    controlFindEditor.m_hasSelection = true;
    controlFindEditor.handleEvent(ctrlFindEvent);
    check(controlFindEditor.m_findQuery.empty()
              && controlFindEditor.m_findMatchCount == 0,
          "Ctrl+F does not seed a control-bearing selection into the inline field");

    TestEditor replacementPasteEditor(nullptr, &fs, "");
    replacementPasteEditor.m_lines = {"needle"};
    replacementPasteEditor.m_searchMode = TestEditor::SearchMode::Replace;
    replacementPasteEditor.m_searchField = TestEditor::SearchField::Replacement;
    replacementPasteEditor.m_findQuery = "needle";
    replacementPasteEditor.m_replaceText = "xy";
    replacementPasteEditor.m_replaceCursorPos = 1;
    replacementPasteEditor.updateFindMatches();
    const bool setReplacementClipboard = SDL_SetClipboardText("a\r\n\t\xC3\xA9") == 0;
    if (setReplacementClipboard) replacementPasteEditor.handleEvent(searchPasteEvent);
    check(setReplacementClipboard
              && replacementPasteEditor.m_replaceText == "xa\n\xC3\xA9y"
              && replacementPasteEditor.m_replaceCursorPos == 5
              && replacementPasteEditor.m_findQuery == "needle"
              && replacementPasteEditor.m_findMatchCount == 1,
          "Ctrl+V normalizes line breaks in the active replacement field");

    TestEditor ctrlEnterFindEditor(nullptr, &fs, "");
    ctrlEnterFindEditor.m_lines = {"a", "b"};
    ctrlEnterFindEditor.m_searchMode = TestEditor::SearchMode::Find;
    ctrlEnterFindEditor.m_findQuery = "ab";
    ctrlEnterFindEditor.m_findCursorPos = 1;
    ctrlEnterFindEditor.updateFindMatches();
    sendKey(ctrlEnterFindEditor, SDLK_RETURN, KMOD_CTRL);
    check(ctrlEnterFindEditor.m_findQuery == "a\nb"
              && ctrlEnterFindEditor.m_findCursorPos == 2
              && ctrlEnterFindEditor.m_findMatchCount == 1,
          "Ctrl+Enter inserts a line break in the active search field");

    TestEditor searchLineCounterEditor(nullptr, &fs, "");
    searchLineCounterEditor.m_searchMode = TestEditor::SearchMode::Find;
    searchLineCounterEditor.m_findQuery = "ab\ncd";
    searchLineCounterEditor.m_findQueryLineBreaks = 1;
    searchLineCounterEditor.m_findCursorPos = 3;
    sendKey(searchLineCounterEditor, SDLK_BACKSPACE);
    const bool searchBackspaceUpdatesLineCount =
        searchLineCounterEditor.m_findQuery == "abcd"
        && searchLineCounterEditor.m_findQueryLineBreaks == 0;
    sendKey(searchLineCounterEditor, SDLK_RETURN, KMOD_CTRL);
    searchLineCounterEditor.m_findCursorPos = 2;
    sendKey(searchLineCounterEditor, SDLK_DELETE);
    check(searchBackspaceUpdatesLineCount
              && searchLineCounterEditor.m_findQuery == "abcd"
              && searchLineCounterEditor.m_findQueryLineBreaks == 0,
          "Find newline counts stay correct across Ctrl+Enter, Backspace, and Delete");

    TestEditor replacementLineCounterEditor(nullptr, &fs, "");
    replacementLineCounterEditor.m_searchMode = TestEditor::SearchMode::Replace;
    replacementLineCounterEditor.m_searchField = TestEditor::SearchField::Replacement;
    replacementLineCounterEditor.m_replaceText = "r\ns";
    replacementLineCounterEditor.m_replaceTextLineBreaks = 1;
    replacementLineCounterEditor.m_replaceCursorPos = 1;
    sendKey(replacementLineCounterEditor, SDLK_DELETE);
    check(replacementLineCounterEditor.m_replaceText == "rs"
              && replacementLineCounterEditor.m_replaceTextLineBreaks == 0,
          "replacement newline counts stay correct after deletion");

    TestEditor coalescedEditor(nullptr, &fs, "");
    setEditorLines(coalescedEditor, {"base"});
    coalescedEditor.m_cursorCol = 4;
    coalescedEditor.insertText("a");
    coalescedEditor.insertText("b");
    check(coalescedEditor.m_lines == std::vector<std::string>{"baseab"}
              && coalescedEditor.m_undoStack.size() == 1
              && coalescedEditor.m_documentSerializedBytes == 6,
          "typing bursts still coalesce into one bounded undo state");
    coalescedEditor.undo();
    coalescedEditor.redo();
    check(coalescedEditor.m_lines == std::vector<std::string>{"baseab"}
              && coalescedEditor.m_documentSerializedBytes == 6,
          "coalesced typing preserves document size through move-based undo and redo");

    TestEditor noOpReplaceEditor(nullptr, &fs, "/old.txt");
    noOpReplaceEditor.m_searchMode = TestEditor::SearchMode::Replace;
    noOpReplaceEditor.m_findQuery = "Xoriginal";
    noOpReplaceEditor.m_replaceText = "Xoriginal";
    noOpReplaceEditor.updateFindMatches();
    const size_t noOpUndoCount = noOpReplaceEditor.m_undoStack.size();
    noOpReplaceEditor.replaceCurrentMatch();
    check(!noOpReplaceEditor.m_dirty, "replacing a match with itself stays clean");
    check(noOpReplaceEditor.m_lines == std::vector<std::string>{"Xoriginal"},
          "replacing a match with itself preserves document content");
    check(noOpReplaceEditor.m_undoStack.size() == noOpUndoCount,
          "replacing a match with itself preserves undo history");
    check(noOpReplaceEditor.m_statusMessage == "Replace: text is unchanged",
          "replacing a match with itself reports a no-op");
    noOpReplaceEditor.replaceAllMatches();
    check(!noOpReplaceEditor.m_dirty, "replace all with identical text stays clean");
    check(noOpReplaceEditor.m_lines == std::vector<std::string>{"Xoriginal"},
          "replace all with identical text preserves document content");
    check(noOpReplaceEditor.m_undoStack.size() == noOpUndoCount,
          "replace all with identical text preserves undo history");
    check(noOpReplaceEditor.m_statusMessage == "Replace all: text is unchanged",
          "replace all with identical text reports a no-op");

    TestEditor multilineReplaceEditor(nullptr, &fs, "");
    setEditorLines(multilineReplaceEditor, {"left needle", "across", "tail end"});
    multilineReplaceEditor.m_searchMode = TestEditor::SearchMode::Replace;
    multilineReplaceEditor.m_findQuery = "needle\nacross\ntail";
    multilineReplaceEditor.m_replaceText = "NEW\nline";
    multilineReplaceEditor.updateFindMatches();
    multilineReplaceEditor.replaceCurrentMatch();
    const std::vector<std::string> replacedMultilineLines = {"left NEW", "line end"};
    check(multilineReplaceEditor.m_lines == replacedMultilineLines
              && multilineReplaceEditor.m_documentSerializedBytes == 17
              && multilineReplaceEditor.m_cursorRow == 1
              && multilineReplaceEditor.m_cursorCol == 4
              && multilineReplaceEditor.m_findMatchCount == 0
              && multilineReplaceEditor.m_undoStack.size() == 1,
          "Replace Current replaces across rows with multiline text and updates document size");
    multilineReplaceEditor.undo();
    check(multilineReplaceEditor.m_lines
                  == std::vector<std::string>{"left needle", "across", "tail end"}
              && multilineReplaceEditor.m_documentSerializedBytes == 27,
          "undo restores the full source range after a multiline current replacement");
    multilineReplaceEditor.redo();
    check(multilineReplaceEditor.m_lines == replacedMultilineLines
              && multilineReplaceEditor.m_documentSerializedBytes == 17,
          "redo restores a multiline current replacement");

    TestEditor multilineReplaceAllEditor(nullptr, &fs, "");
    setEditorLines(multilineReplaceAllEditor, {"A", "B", "xA", "Bz"});
    multilineReplaceAllEditor.m_searchMode = TestEditor::SearchMode::Replace;
    multilineReplaceAllEditor.m_findQuery = "A\nB";
    multilineReplaceAllEditor.m_replaceText = "C";
    multilineReplaceAllEditor.replaceAllMatches();
    check(multilineReplaceAllEditor.m_lines == std::vector<std::string>{"C", "xCz"}
              && multilineReplaceAllEditor.m_documentSerializedBytes == 5
              && multilineReplaceAllEditor.m_undoStack.size() == 1,
          "Replace All replaces every non-overlapping multiline match in source order");
    multilineReplaceAllEditor.undo();
    check(multilineReplaceAllEditor.m_lines
                  == std::vector<std::string>{"A", "B", "xA", "Bz"}
              && multilineReplaceAllEditor.m_documentSerializedBytes == 9,
          "one undo step restores all multiline Replace All results");

    TestEditor newlineReplaceAllEditor(nullptr, &fs, "");
    setEditorLines(newlineReplaceAllEditor, {"left", "right"});
    newlineReplaceAllEditor.m_searchMode = TestEditor::SearchMode::Replace;
    newlineReplaceAllEditor.m_findQuery = "\n";
    newlineReplaceAllEditor.m_replaceText = "\nmid\n";
    newlineReplaceAllEditor.replaceAllMatches();
    check(newlineReplaceAllEditor.m_lines
                  == std::vector<std::string>{"left", "mid", "right"}
              && newlineReplaceAllEditor.m_documentSerializedBytes == 14,
          "Replace All handles newline-only matches and leading or trailing replacement breaks");

    check(fs.writeFile("/same.txt", "same"), "write identical selection editor fixture");
    TestEditor noOpSelectionEditor(nullptr, &fs, "/same.txt");
    noOpSelectionEditor.m_selAnchorRow = 0;
    noOpSelectionEditor.m_selAnchorCol = 0;
    noOpSelectionEditor.m_cursorRow = 0;
    noOpSelectionEditor.m_cursorCol = 4;
    noOpSelectionEditor.m_hasSelection = true;
    noOpSelectionEditor.insertText("same");
    check(!noOpSelectionEditor.m_dirty
              && !noOpSelectionEditor.hasSelection()
              && noOpSelectionEditor.m_cursorCol == 4
              && noOpSelectionEditor.m_undoStack.empty(),
          "typing the selected text preserves clean state and undo history");
    check(SDL_SetClipboardText("same") == 0, "set identical selection clipboard fixture");
    noOpSelectionEditor.m_selAnchorRow = 0;
    noOpSelectionEditor.m_selAnchorCol = 0;
    noOpSelectionEditor.m_cursorRow = 0;
    noOpSelectionEditor.m_cursorCol = 4;
    noOpSelectionEditor.m_hasSelection = true;
    noOpSelectionEditor.pasteClipboard();
    check(!noOpSelectionEditor.m_dirty
              && !noOpSelectionEditor.hasSelection()
              && noOpSelectionEditor.m_cursorCol == 4
              && noOpSelectionEditor.m_undoStack.empty(),
          "pasting the selected text preserves clean state and undo history");

    TestEditor batchPasteEditor(nullptr, &fs, "");
    setEditorLines(batchPasteEditor, {"abMIDCD", "tail"});
    batchPasteEditor.m_selAnchorRow = 0;
    batchPasteEditor.m_selAnchorCol = 2;
    batchPasteEditor.m_cursorRow = 0;
    batchPasteEditor.m_cursorCol = 5;
    batchPasteEditor.m_hasSelection = true;
    check(SDL_SetClipboardText("X\r\nY\rZ") == 0,
          "set mixed-line-ending multiline paste fixture");
    batchPasteEditor.pasteClipboard();
    const std::vector<std::string> batchPasteLines = {"abX", "Y", "ZCD", "tail"};
    check(batchPasteEditor.m_lines == batchPasteLines
              && batchPasteEditor.m_cursorRow == 2
              && batchPasteEditor.m_cursorCol == 1,
          "multiline paste replaces selection, normalizes line endings, and keeps suffix and caret");
    batchPasteEditor.undo();
    check(batchPasteEditor.m_lines == std::vector<std::string>{"abMIDCD", "tail"}
              && batchPasteEditor.m_cursorRow == 0
              && batchPasteEditor.m_cursorCol == 5
              && batchPasteEditor.m_documentSerializedBytes == 12,
          "undo restores the document and cursor from before multiline paste");
    batchPasteEditor.redo();
    check(batchPasteEditor.m_lines == batchPasteLines
              && batchPasteEditor.m_cursorRow == 2
              && batchPasteEditor.m_cursorCol == 1
              && batchPasteEditor.m_documentSerializedBytes == 14,
          "redo restores the complete multiline paste");

    TestEditor trailingPasteEditor(nullptr, &fs, "");
    setEditorLines(trailingPasteEditor, {"prepost"});
    trailingPasteEditor.m_cursorCol = 3;
    check(SDL_SetClipboardText("A\nB\n") == 0,
          "set trailing-newline multiline paste fixture");
    trailingPasteEditor.pasteClipboard();
    check(trailingPasteEditor.m_lines == std::vector<std::string>{"preA", "B", "post"}
              && trailingPasteEditor.m_cursorRow == 2
              && trailingPasteEditor.m_cursorCol == 0,
          "multiline paste preserves a trailing newline before the original line suffix");

    TestEditor byteLimitEditor(nullptr, &fs, "");
    setEditorLines(byteLimitEditor,
                   {std::string(TestEditor::kMaxDocumentBytes - 1, 'x')});
    byteLimitEditor.m_cursorCol = static_cast<int>(TestEditor::kMaxDocumentBytes - 1);
    byteLimitEditor.insertText("x");
    const size_t exactLimitUndoCount = byteLimitEditor.m_undoStack.size();
    byteLimitEditor.insertText("y");
    check(byteLimitEditor.m_lines[0].size() == TestEditor::kMaxDocumentBytes
              && byteLimitEditor.m_lines[0].back() == 'x'
              && byteLimitEditor.m_documentSerializedBytes == TestEditor::kMaxDocumentBytes
              && byteLimitEditor.m_undoStack.size() == exactLimitUndoCount
              && byteLimitEditor.m_statusMessage
                  == "Edit rejected: exceeds 16 MiB or 65,536 lines",
          "typing reaches the exact byte limit and rejects further growth without undo state");

    TestEditor selectedLimitPasteEditor(nullptr, &fs, "");
    setEditorLines(selectedLimitPasteEditor,
                   {std::string(TestEditor::kMaxDocumentBytes, 'x')});
    selectedLimitPasteEditor.m_selAnchorCol = 0;
    selectedLimitPasteEditor.m_cursorCol = static_cast<int>(TestEditor::kMaxDocumentBytes);
    selectedLimitPasteEditor.m_hasSelection = true;
    check(SDL_SetClipboardText("ok") == 0,
          "set small replacement clipboard for a maximum-size document");
    selectedLimitPasteEditor.pasteClipboard();
    check(selectedLimitPasteEditor.m_lines == std::vector<std::string>{"ok"}
              && selectedLimitPasteEditor.m_documentSerializedBytes == 2,
          "paste accounts for selected bytes before enforcing the document limit");

    TestEditor lineLimitEditor(nullptr, &fs, "");
    setEditorLines(lineLimitEditor,
                   std::vector<std::string>(TestEditor::kMaxDocumentLines - 1));
    check(SDL_SetClipboardText("\n") == 0,
          "set newline clipboard for the exact line limit");
    lineLimitEditor.pasteClipboard();
    const size_t exactLimitLineCount = lineLimitEditor.m_lines.size();
    const size_t exactLimitLineUndoCount = lineLimitEditor.m_undoStack.size();
    lineLimitEditor.insertNewline();
    check(exactLimitLineCount == TestEditor::kMaxDocumentLines
              && lineLimitEditor.m_lines.size() == exactLimitLineCount
              && lineLimitEditor.m_undoStack.size() == exactLimitLineUndoCount
              && lineLimitEditor.m_statusMessage
                  == "Edit rejected: exceeds 16 MiB or 65,536 lines",
          "multiline paste reaches the exact line limit and Enter rejects further growth");

    TestEditor replaceLimitEditor(nullptr, &fs, "");
    setEditorLines(replaceLimitEditor,
                   {std::string(8 * 1024 * 1024 + 1, 'x')});
    replaceLimitEditor.m_searchMode = TestEditor::SearchMode::Replace;
    replaceLimitEditor.m_findQuery = "x";
    replaceLimitEditor.m_replaceText = "xx";
    replaceLimitEditor.updateFindMatches();
    replaceLimitEditor.replaceAllMatches();
    check(replaceLimitEditor.m_lines[0].size() == 8 * 1024 * 1024 + 1
              && replaceLimitEditor.m_documentSerializedBytes == 8 * 1024 * 1024 + 1
              && replaceLimitEditor.m_undoStack.empty()
              && replaceLimitEditor.m_statusMessage
                  == "Replace all rejected: exceeds 16 MiB or 65,536 lines",
          "Replace All rejects a result above the byte limit without changing text or history");

    setEditorLines(editor, {"aa"});
    editor.m_cursorRow = 0;
    editor.m_cursorCol = 0;
    editor.m_searchMode = TestEditor::SearchMode::Replace;
    editor.m_findQuery = "a";
    editor.m_replaceText = "aa";
    editor.replaceAllMatches();
    check(editor.m_lines == std::vector<std::string>{"aaaa"},
          "replace all does not reprocess replacement text");

    setEditorLines(editor, {"aaa"});
    editor.m_cursorRow = 0;
    editor.m_cursorCol = 0;
    editor.m_searchMode = TestEditor::SearchMode::Replace;
    editor.m_findQuery = "aa";
    editor.m_replaceText = "X";
    editor.replaceAllMatches();
    check(editor.m_lines == std::vector<std::string>{"Xa"},
          "replace all follows find order for overlapping candidates");

    editor.m_lines = {"aaaa"};
    editor.m_cursorRow = 0;
    editor.m_cursorCol = 0;
    editor.m_searchMode = TestEditor::SearchMode::Find;
    editor.m_findQuery = "aa";
    editor.updateFindMatches();
    check(editor.m_findMatchCount == 2
              && editor.findMatchAtIndex(0) == std::pair<int, int>{0, 0}
              && editor.findMatchAtIndex(1) == std::pair<int, int>{0, 2},
          "find uses non-overlapping matches like replace all");

    TestEditor denseFindEditor(nullptr, &fs, "");
    constexpr std::size_t denseFindCount = 1'048'576;
    denseFindEditor.m_lines = {std::string(denseFindCount, 'x')};
    denseFindEditor.m_findQuery = "x";
    denseFindEditor.updateFindMatches();
    const std::size_t checkpointBytes = denseFindEditor.m_findCheckpoints.size()
        * sizeof(TestEditor::FindMatchCheckpoint);
    denseFindEditor.m_currentFindMatch = 255;
    denseFindEditor.m_hasCurrentFindMatch = true;
    denseFindEditor.m_currentFindPosition = denseFindEditor.findMatchAtIndex(255);
    denseFindEditor.moveFindMatch(1);
    const bool crossesCheckpointForward = denseFindEditor.m_currentFindMatch == 256
        && denseFindEditor.m_currentFindPosition == std::pair<int, int>{0, 256};
    denseFindEditor.moveFindMatch(-1);
    const bool crossesCheckpointBackward = denseFindEditor.m_currentFindMatch == 255
        && denseFindEditor.m_currentFindPosition == std::pair<int, int>{0, 255};
    denseFindEditor.m_currentFindMatch = 0;
    denseFindEditor.m_hasCurrentFindMatch = true;
    denseFindEditor.m_currentFindPosition = denseFindEditor.findMatchAtIndex(0);
    denseFindEditor.moveFindMatch(-1);
    check(denseFindEditor.m_findMatchCount == denseFindCount
              && denseFindEditor.m_findCheckpoints.size()
                  == denseFindCount / TestEditor::kFindCheckpointStride
              && checkpointBytes * 100
                  < denseFindCount * sizeof(std::pair<int, int>)
              && denseFindEditor.findMatchAtIndex(255) == std::pair<int, int>{0, 255}
              && denseFindEditor.findMatchAtIndex(256) == std::pair<int, int>{0, 256}
              && denseFindEditor.findMatchAtIndex(denseFindCount - 1)
                  == std::pair<int, int>{0, static_cast<int>(denseFindCount - 1)}
              && crossesCheckpointForward && crossesCheckpointBackward
              && denseFindEditor.m_currentFindMatch == denseFindCount - 1,
          "Find uses sparse checkpoints for million-hit navigation and bounded memory");

    setEditorLines(editor, {"ab", "\xF0\x9F\x98\x80"});
    editor.m_cursorRow = 0;
    editor.m_cursorCol = 2;
    editor.moveDown(false);
    check(editor.m_cursorRow == 1 && editor.m_cursorCol == 0,
          "vertical movement keeps the cursor on a UTF-8 boundary");
    editor.insertText("X");
    check(editor.m_lines[1] == "X\xF0\x9F\x98\x80",
          "editing after vertical movement preserves the full UTF-8 character");

    editor.m_syntaxMode = TestEditor::SyntaxMode::Code;
    const auto signedNumberSpans = editor.tokenizeLine("-42 +7", {});
    check(signedNumberSpans.size() == 3
              && signedNumberSpans[0].start == 0
              && signedNumberSpans[0].length == 3
              && signedNumberSpans[1].start == 3
              && signedNumberSpans[1].length == 1
              && signedNumberSpans[2].start == 4
              && signedNumberSpans[2].length == 2,
          "syntax highlighting keeps signs attached to numeric tokens");

    TestEditor::SyntaxState openCommentState;
    const auto openCommentSpans = editor.tokenizeLine(
        "int x; /* open", {}, &openCommentState);
    const bool openedBlockComment = openCommentState.inBlockComment;
    const auto continuedCommentSpans = editor.tokenizeLine(
        "inside */ return", openCommentState, &openCommentState);
    check(openedBlockComment
              && openCommentSpans.back().color.g == 145
              && !openCommentState.inBlockComment
              && continuedCommentSpans.size() == 3
              && continuedCommentSpans[0].length == 9
              && continuedCommentSpans[0].color.g == 145
              && continuedCommentSpans[2].start == 10
              && continuedCommentSpans[2].color.b == 225,
          "syntax highlighting carries block comments across lines and resumes tokens after closing");

    setEditorLines(editor, {"/* open", "inside", "*/ return"});
    editor.m_syntaxLineStates.clear();
    editor.ensureSyntaxStateThrough(2);
    check(editor.m_syntaxLineStates.size() == 3
              && editor.m_syntaxLineStates[0].inBlockComment
              && editor.m_syntaxLineStates[1].inBlockComment
              && !editor.m_syntaxLineStates[2].inBlockComment,
          "editor caches block-comment state through scrolled document lines");
    editor.m_selAnchorRow = 0;
    editor.m_selAnchorCol = 0;
    editor.m_cursorRow = 0;
    editor.m_cursorCol = 2;
    editor.m_hasSelection = true;
    editor.insertText("  ");
    editor.ensureSyntaxStateThrough(2);
    check(editor.m_syntaxLineStates.size() == 3
              && !editor.m_syntaxLineStates[0].inBlockComment
              && !editor.m_syntaxLineStates[1].inBlockComment
              && !editor.m_syntaxLineStates[2].inBlockComment,
          "editing a block-comment opener invalidates following cached line states");

    editor.clearUndoHistory();
    editor.clearRedoHistory();
    for (int i = 0; i < 60; ++i) {
        editor.m_cursorCol = i;
        editor.pushUndoState();
    }
    check(editor.m_undoStack.size() == TestEditor::kMaxUndoStates,
          "undo history stays within its 50-state cap");
    for (int i = 0; i < 20; ++i) editor.undo();
    check(editor.m_undoStack.size() + editor.m_redoStack.size() == TestEditor::kMaxUndoStates,
          "undo and redo share the 50-state cap");

    TestEditor snapshotEditor(nullptr, &fs, "");
    snapshotEditor.m_lines = {"alpha", "", "omega"};
    snapshotEditor.m_cursorRow = 2;
    snapshotEditor.m_cursorCol = 3;
    const auto snapshot = snapshotEditor.captureEditorState();
    check(snapshot.lines == snapshotEditor.m_lines
              && snapshot.cursorRow == 2 && snapshot.cursorCol == 3
              && snapshot.memoryBytes
                  == snapshotEditor.measureEditorStateBytes(snapshot.lines),
          "editor snapshots account cloned storage while preserving document and cursor state");

    {
        TestEditor memoryEditor(nullptr, &fs, "");
        constexpr size_t largeLineBytes = 24 * 1024 * 1024;
        memoryEditor.m_lines = {std::string(largeLineBytes, 'a')};
        for (char next = 'b'; next <= 'd'; ++next) {
            memoryEditor.pushUndoState();
            memoryEditor.m_lines[0][0] = next;
        }
        check(memoryEditor.m_undoStack.size() == 2
                  && memoryEditor.m_undoStack[0].lines[0][0] == 'b'
                  && memoryEditor.m_undoStack[1].lines[0][0] == 'c'
                  && memoryEditor.m_undoBytes <= TestEditor::kMaxUndoBytes,
              "editor evicts oldest snapshots to stay within its 64 MiB budget");
        memoryEditor.undo();
        check(memoryEditor.m_lines[0][0] == 'c'
                  && memoryEditor.m_undoBytes + memoryEditor.m_redoBytes
                      <= TestEditor::kMaxUndoBytes,
              "large-document undo moves its buffer without exceeding the history budget");
        memoryEditor.redo();
        check(memoryEditor.m_lines[0][0] == 'd'
                  && memoryEditor.m_undoBytes + memoryEditor.m_redoBytes
                      <= TestEditor::kMaxUndoBytes,
              "large-document redo restores the moved buffer within the history budget");
    }

    TestEditor overBudgetEditor(nullptr, &fs, "");
    overBudgetEditor.m_lines = {"small"};
    overBudgetEditor.pushUndoState();
    overBudgetEditor.m_lines.clear();
    overBudgetEditor.m_lines.emplace_back(TestEditor::kMaxUndoBytes + 1, 'x');
    overBudgetEditor.pushUndoState();
    check(overBudgetEditor.m_undoStack.empty() && overBudgetEditor.m_undoBytes == 0,
          "an oversized document clears history without storing another full copy");
    overBudgetEditor.undo();
    check(overBudgetEditor.m_statusMessage == "Nothing to undo (history memory limit).",
          "the editor explains when an oversized document has no undo snapshot");

    TestEditor promptEditor(nullptr, &fs, "/new.txt");
    prepareOpen(promptEditor, "/does-not-exist");
    promptEditor.completePathPrompt();
    check(promptEditor.m_statusMessage == "No path matches.",
          "path completion reports when the editor has no matches");

    prepareOpen(promptEditor, "/unicode/");
    promptEditor.completePathPrompt();
    check(promptEditor.m_pathPromptBuffer == "/unicode/"
              && promptEditor.m_pathPromptCursorPos == promptEditor.m_pathPromptBuffer.size(),
          "ambiguous Unicode completion never inserts a partial codepoint");
    prepareOpen(promptEditor, "/unicode/" + eAcuteName.substr(0, 2));
    promptEditor.completePathPrompt();
    check(promptEditor.m_pathPromptBuffer == "/unicode/" + eAcuteName,
          "Unicode path completion expands an exact codepoint prefix");

    TestEditor homePathEditor(nullptr, &fs, "");
    prepareOpen(homePathEditor, "~/documents/home-");
    homePathEditor.completePathPrompt();
    check(homePathEditor.m_pathPromptBuffer == "~/documents/home-note.txt",
          "Text Editor completion searches virtual home and keeps the shorthand");
    homePathEditor.finishPathPrompt(true);
    check(homePathEditor.m_filePath == "/home/monolith/documents/home-note.txt"
              && homePathEditor.m_lines == std::vector<std::string>{"home prompt target"},
          "Text Editor Open expands a home-relative prompt path");
    prepareSaveAs(homePathEditor, "~/documents/home-saved.txt");
    homePathEditor.finishPathPrompt(true);
    check(homePathEditor.m_filePath == "/home/monolith/documents/home-saved.txt"
              && fs.readFile("/home/monolith/documents/home-saved.txt")
                  == "home prompt target",
          "Text Editor Save As expands a home-relative prompt path");

    prepareOpen(homePathEditor, "~/documents/home-note.txt");
    homePathEditor.m_pathPromptCursorPos = std::string("~/documents/").size();
    homePathEditor.onVirtualPathMoved(
        "/home/monolith/documents", "/archive");
    check(homePathEditor.m_pathPromptBuffer == "/archive/home-note.txt"
              && homePathEditor.m_pathPromptCursorPos == std::string("/archive/").size(),
          "Text Editor move notifications preserve the caret after home expansion");

    prepareSaveAs(promptEditor, "/new.txt/child.txt");
    promptEditor.m_pathPromptCursorPos = std::string("/new.txt/").size();
    promptEditor.onBoundFileMoved("/new.txt", "/docs/../renamed.txt");
    check(promptEditor.m_pathPromptBuffer == "/renamed.txt/child.txt"
              && promptEditor.m_pathPromptCursorPos == std::string("/renamed.txt/").size(),
          "Save As prompt canonicalizes a moved bound editor file and preserves its caret");
    prepareOpen(promptEditor, "/docs/nested/child.txt");
    promptEditor.m_pathPromptCursorPos = std::string("/docs/nested/").size();
    promptEditor.onVirtualPathMoved("/docs", "/archive/../archive");
    check(promptEditor.m_pathPromptBuffer == "/archive/nested/child.txt"
              && promptEditor.m_pathPromptCursorPos == std::string("/archive/nested/").size(),
          "Open prompt follows a moved editor directory and preserves its caret");
    prepareSaveAs(promptEditor, "/renamed.txt");
    promptEditor.onBoundFileRemoved("/renamed.txt");
    check(promptEditor.m_pathPromptBuffer == "/",
          "Save As prompt returns to a valid parent after deletion");

    if (scaleFont) TTF_CloseFont(scaleFont);
    if (ttfReady) TTF_Quit();
    if (videoReady) SDL_Quit();
    if (failures == 0) {
        std::cout << "ALL TEXT EDITOR STATE TESTS PASSED\n";
        return 0;
    }
    std::cerr << failures << " test(s) failed\n";
    return 1;
}
