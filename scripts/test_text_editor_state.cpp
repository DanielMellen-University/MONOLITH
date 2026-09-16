// Headless regression test for Text Editor Save As identity handling.

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include <filesystem>
#include <iostream>
#include <string>
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
    check(editor.m_findMatches == std::vector<std::pair<int, int>>{{0, 0}, {0, 2}},
          "find uses non-overlapping matches like replace all");

    editor.m_lines = {"ab", "\xF0\x9F\x98\x80"};
    editor.m_cursorRow = 0;
    editor.m_cursorCol = 2;
    editor.moveDown(false);
    check(editor.m_cursorRow == 1 && editor.m_cursorCol == 0,
          "vertical movement keeps the cursor on a UTF-8 boundary");
    editor.insertText("X");
    check(editor.m_lines[1] == "X\xF0\x9F\x98\x80",
          "editing after vertical movement preserves the full UTF-8 character");

    const auto signedNumberSpans = editor.tokenizeLine("-42 +7");
    check(signedNumberSpans.size() == 3
              && signedNumberSpans[0].start == 0
              && signedNumberSpans[0].length == 3
              && signedNumberSpans[1].start == 3
              && signedNumberSpans[1].length == 1
              && signedNumberSpans[2].start == 4
              && signedNumberSpans[2].length == 2,
          "syntax highlighting keeps signs attached to numeric tokens");

    editor.m_undoStack.clear();
    for (int i = 0; i < 60; ++i) {
        editor.m_cursorCol = i;
        editor.pushUndoState();
    }
    check(editor.m_undoStack.size() == TestEditor::kMaxUndoStates,
          "undo history stays within its 50-state cap");

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
