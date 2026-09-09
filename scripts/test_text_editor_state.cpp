// Headless regression test for Text Editor Save As identity handling.

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>

#include <filesystem>
#include <iostream>
#include <string>
#include <unistd.h>

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

    void close() override {}
    void setTitle(const std::string&) override {}

    bool focusEditorForFile(const std::string& path) override {
        focusedPath = path;
        return path == blockedPath;
    }

    void bindEditorFile(const std::string& path) override {
        boundPath = path;
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
    check(fs.writeFile("/old.txt", "original"), "write original editor file");
    check(fs.writeFile("/empty.txt", ""), "write empty editor file");
    check(fs.writeFile("/windows.txt", "first\r\nsecond\r\n"),
          "write CRLF editor file");
    check(fs.writeFile("/classic-mac.txt", "first\rsecond\r"),
          "write lone-CR editor file");
    check(fs.createDirectory("/folder"), "create unwritable save target directory");

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

    editor.m_lines = {"aa"};
    editor.m_cursorRow = 0;
    editor.m_cursorCol = 0;
    editor.m_searchMode = TestEditor::SearchMode::Replace;
    editor.m_findQuery = "a";
    editor.m_replaceText = "aa";
    editor.replaceAllMatches();
    check(editor.m_lines == std::vector<std::string>{"aaaa"},
          "replace all does not reprocess replacement text");

    editor.m_undoStack.clear();
    for (int i = 0; i < 60; ++i) {
        editor.m_cursorCol = i;
        editor.pushUndoState();
    }
    check(editor.m_undoStack.size() == TestEditor::kMaxUndoStates,
          "undo history stays within its 50-state cap");

    std::filesystem::remove_all(hostRoot, ec);
    if (failures == 0) {
        std::cout << "ALL TEXT EDITOR STATE TESTS PASSED\n";
        return 0;
    }
    std::cerr << failures << " test(s) failed\n";
    return 1;
}
