// Headless regression test for Text Editor Save As identity handling.

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <iostream>
#include <string>
#include <utility>
#include <unistd.h>
#include <vector>

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

    void close() override {}
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
        / ("monolith-text-editor-state-" + std::to_string(getpid()));
    std::error_code ec;
    std::filesystem::remove_all(hostRoot, ec);

    monolith::fs::Filesystem fs(hostRoot.string());
    check(fs.initialize(), "editor state filesystem initialize");
    const bool videoReady = SDL_Init(SDL_INIT_VIDEO) == 0;
    check(videoReady, "editor state SDL initialize");
    const bool ttfReady = TTF_Init() == 0;
    check(ttfReady, "editor state SDL_ttf initialize");
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
            const auto firstTextTexture = scaleEditor.m_textTextureCache.get(
                renderer, scaleFont, "first", {200, 205, 210, 255});
            const size_t cachedTextureCount = scaleEditor.m_textTextureCache.size();
            scaleEditor.render(renderer, {0, 0, 200, 160});
            const auto repeatedTextTexture = scaleEditor.m_textTextureCache.get(
                renderer, scaleFont, "first", {200, 205, 210, 255});
            check(firstTextTexture && repeatedTextTexture.handle == firstTextTexture.handle
                      && scaleEditor.m_textTextureCache.size() == cachedTextureCount,
                  "Text Editor reuses renderer textures between unchanged frames");

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
                expectedCursorWidth = 0;
                const bool openWidthMeasured = TTF_SizeUTF8(
                    promptFont, openCursorText.c_str(), &expectedCursorWidth,
                    &expectedCursorHeight) == 0;
                const bool openPromptCached = openWidthMeasured
                    && promptMetricEditor.m_statusCursorMeasureText == openCursorText
                    && promptMetricEditor.m_statusCursorPixelWidth == expectedCursorWidth;

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
                check(findPromptRetained && findCursorMovementRefreshes
                          && replacePromptCached && goToLinePromptCached
                          && openPromptCached && saveAsPromptCached
                          && fontResized && promptMetricsInvalidated && scaledWidthMeasured
                          && promptMetricEditor.m_statusCursorMeasureText == saveAsCursorText
                          && promptMetricEditor.m_statusCursorPixelWidth == expectedCursorWidth
                          && expectedCursorWidth != oldPromptWidth,
                      "Text Editor reuses prompt caret widths across Find, Replace, path prompts, and UI scaling");
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
                    == findViewportEditor.m_renderedFindPrefixWidths.size();
            const auto* findGeometryStorage =
                findViewportEditor.m_renderedFindPrefixWidths.data();
            findViewportEditor.render(renderer, {0, 0, 240, 200});
            const bool retainedFindGeometry = cachedFindGeometry
                && findGeometryStorage
                    == findViewportEditor.m_renderedFindPrefixWidths.data();
            int matchPrefixWidth = 0;
            int matchPrefixHeight = 0;
            TTF_SizeUTF8(scaleFont, "target ", &matchPrefixWidth, &matchPrefixHeight);
            int expectedFindQueryWidth = 0;
            int expectedFindQueryHeight = 0;
            const bool measuredFindQueryWidth = TTF_SizeUTF8(
                scaleFont, "target", &expectedFindQueryWidth, &expectedFindQueryHeight) == 0;
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
                      && retainedFindGeometry
                      && measuredFindQueryWidth
                      && findViewportEditor.m_findQueryPixelWidthValid
                      && findViewportEditor.m_findQueryPixelWidth == expectedFindQueryWidth,
                  "Find reuses viewport highlight geometry and preserves active styling after scrolling");
            findViewportEditor.moveFindMatch(1);
            findViewportEditor.render(renderer, {0, 0, 240, 200});
            const bool navigationReusesGeometry =
                findViewportEditor.m_currentFindMatch == 258
                && findViewportEditor.m_renderedFindStartRow == 128
                && findGeometryStorage
                    == findViewportEditor.m_renderedFindPrefixWidths.data();
            findViewportEditor.m_scrollOffset = 129;
            findViewportEditor.render(renderer, {0, 0, 240, 200});
            const bool viewportGeometryRebuilt =
                findViewportEditor.m_renderedFindStartRow == 129
                && findViewportEditor.m_renderedFindLineCount == visibleFindRows
                && !findViewportEditor.m_renderedFindPrefixWidths.empty();
            findViewportEditor.m_findQuery = "target ";
            findViewportEditor.updateFindMatches();
            const bool queryWidthInvalidated = !findViewportEditor.m_findQueryPixelWidthValid;
            const bool queryGeometryInvalidated =
                findViewportEditor.m_renderedFindStartRow == -1
                && findViewportEditor.m_renderedFindPrefixWidths.empty();
            findViewportEditor.render(renderer, {0, 0, 240, 200});
            const bool fontMetricWasCached = findViewportEditor.m_findQueryPixelWidthValid;
            const bool queryGeometryRebuilt =
                findViewportEditor.m_renderedFindStartRow == findViewportEditor.m_scrollOffset
                && !findViewportEditor.m_renderedFindPrefixWidths.empty();
            findViewportEditor.onUiScaleChanged();
            check(navigationReusesGeometry && viewportGeometryRebuilt
                      && queryWidthInvalidated && fontMetricWasCached
                      && queryGeometryInvalidated && queryGeometryRebuilt
                      && !findViewportEditor.m_findQueryPixelWidthValid
                      && findViewportEditor.m_renderedFindStartRow == -1
                      && findViewportEditor.m_renderedFindPrefixWidths.empty(),
                  "Find query and font changes invalidate cached highlight geometry and width");

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

            TestEditor syntaxCacheEditor(scaleFont, &fs, "");
            syntaxCacheEditor.m_syntaxMode = TestEditor::SyntaxMode::Code;
            syntaxCacheEditor.m_lines = {"int x; /* open", "return 4; */"};
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
                    return entry.first.size() >= sizeof(TTF_Font*) + 9
                        && entry.first.compare(sizeof(TTF_Font*), 9, longLine, 0, 9) == 0;
                });
            check(firstViewportText != longLineEditor.m_textTextureCache.m_entries.end()
                      && firstViewportText->first.size()
                          < sizeof(TTF_Font*) + longLine.size() / 20 + 5
                      && firstViewportText->second.width
                          < longLineViewportWidth + 2 * TTF_FontHeight(scaleFont),
                  "Text Editor caches only viewport-sized textures for very long lines");
            check(retainedTextSlice,
                  "Text Editor reuses UTF-8 viewport measurements across unchanged frames");

            const std::string firstViewportKey = firstViewportText
                != longLineEditor.m_textTextureCache.m_entries.end()
                ? firstViewportText->first
                : std::string{};
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
            check(!firstViewportKey.empty() && retainedViewportText
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
    const std::string eAcuteName = std::string("\xC3\xA9") + "clair.txt";
    const std::string eCircumflexName = std::string("\xC3\xAA") + "clair.txt";
    check(fs.writeFile("/unicode/" + eAcuteName, "acute"),
          "write first Unicode completion candidate");
    check(fs.writeFile("/unicode/" + eCircumflexName, "circumflex"),
          "write second Unicode completion candidate");

    TestEditor emptyEditor(nullptr, &fs, "/empty.txt");
    check(emptyEditor.m_lines == std::vector<std::string>{""},
          "empty files open as one editable blank line");

    TestEditor windowsEditor(nullptr, &fs, "/windows.txt");
    check(windowsEditor.m_lines == std::vector<std::string>{"first", "second", ""},
          "CRLF files open with normalized line endings");

    TestEditor classicMacEditor(nullptr, &fs, "/classic-mac.txt");
    check(classicMacEditor.m_lines == std::vector<std::string>{"first", "second", ""},
          "lone-CR files open with normalized line endings");

    TestEditor chunkBoundaryEditor(nullptr, &fs, "/chunk-boundary.txt");
    check(chunkBoundaryEditor.m_lines.size() == 3
              && chunkBoundaryEditor.m_lines[0] == std::string(16 * 1024 - 1, 'x')
              && chunkBoundaryEditor.m_lines[1] == "end"
              && chunkBoundaryEditor.m_lines[2].empty(),
          "streamed editor open normalizes CRLF split across chunks and trailing CR");

    TestEditor maxLinesEditor(nullptr, &fs, "/max-lines.txt");
    check(maxLinesEditor.m_lines.size() == TestEditor::kMaxDocumentLines
              && maxLinesEditor.m_lines.front().empty()
              && maxLinesEditor.m_lines.back().empty(),
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
    editor.finishPathPrompt(true);
    check(editor.m_filePath == "/old.txt",
          "failed Save As preserves the current file path");
    check(fs.isDirectory("/folder"), "failed Save As leaves the existing target intact");

    prepareSaveAs(editor, "/new.txt");
    editor.finishPathPrompt(true);
    check(editor.m_filePath == "/new.txt", "successful Save As updates the file path");
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
              && externalEditor.m_statusMessage.find("changed externally") != std::string::npos,
          "external overwrite warns without replacing the editor buffer");
    externalController.blockedPath = "/new.txt";
    prepareOpen(externalEditor, "/new.txt");
    externalEditor.finishPathPrompt(true);
    check(externalEditor.m_filePath == "/new.txt"
              && externalEditor.m_lines == std::vector<std::string>{"outside change"}
              && !externalEditor.m_dirty,
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
    check(externalEditor.m_lines == std::vector<std::string>{"newer external change"}
              && !externalEditor.m_dirty,
          "confirmed same-file reload loads the external editor version");

    TestEditor selfSaveEditor(nullptr, &fs, "/new.txt");
    TestController selfSaveController;
    selfSaveEditor.setController(&selfSaveController);
    selfSaveController.editor = &selfSaveEditor;
    selfSaveEditor.m_lines = {"editor save"};
    selfSaveEditor.m_dirty = true;
    check(selfSaveEditor.saveCurrentFile(), "editor save succeeds after an external overwrite");
    check(selfSaveEditor.m_statusMessage == "Saved: new.txt"
              && !selfSaveEditor.m_suppressChangedNotification,
          "the editor ignores its own synchronous change notification");
    check(fs.readFile("/new.txt") == "editor save",
          "editor save deliberately replaces the external file content");

    check(fs.createDirectory("/blocked.txt"), "create blocked direct-save target");
    editor.m_filePath = "/blocked.txt";
    editor.m_dirty = true;
    check(!editor.allowClose(), "first close arms the dirty editor guard");
    check(!editor.saveCurrentFile(), "direct save failure is reported");
    check(!editor.allowClose(), "failed save clears the stale dirty guard arm");

    check(fs.writeFile("/other.txt", "other"), "write alternate open target");
    editor.m_dirty = true;
    prepareOpen(editor, "/other.txt");
    editor.finishPathPrompt(true);
    check(editor.m_discardKind == TestEditor::DiscardKind::Open
              && editor.m_pathPromptMode == TestEditor::PathPromptMode::Open,
          "dirty open arms a discard confirmation and keeps the prompt active");
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
    editor.finishPathPrompt(true);
    check(editor.m_filePath == "/third.txt" && !editor.m_dirty
              && editor.m_lines == std::vector<std::string>{"third"},
          "confirming the changed dirty open target loads it");

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

    TestEditor coalescedEditor(nullptr, &fs, "");
    coalescedEditor.m_lines = {"base"};
    coalescedEditor.m_cursorCol = 4;
    coalescedEditor.insertText("a");
    coalescedEditor.insertText("b");
    check(coalescedEditor.m_lines == std::vector<std::string>{"baseab"}
              && coalescedEditor.m_undoStack.size() == 1,
          "typing bursts still coalesce into one bounded undo state");
    coalescedEditor.undo();
    coalescedEditor.redo();
    check(coalescedEditor.m_lines == std::vector<std::string>{"baseab"},
          "coalesced typing survives move-based undo and redo");

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

    editor.m_lines = {"aa"};
    editor.m_cursorRow = 0;
    editor.m_cursorCol = 0;
    editor.m_searchMode = TestEditor::SearchMode::Replace;
    editor.m_findQuery = "a";
    editor.m_replaceText = "aa";
    editor.replaceAllMatches();
    check(editor.m_lines == std::vector<std::string>{"aaaa"},
          "replace all does not reprocess replacement text");

    editor.m_lines = {"aaa"};
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

    editor.m_lines = {"ab", "\xF0\x9F\x98\x80"};
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

    editor.m_lines = {"/* open", "inside", "*/ return"};
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

    std::filesystem::remove_all(hostRoot, ec);
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
