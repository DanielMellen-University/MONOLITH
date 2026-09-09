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
    check(fs.createDirectory("/folder"), "create unwritable save target directory");

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

    editor.m_lines = {"aa"};
    editor.m_cursorRow = 0;
    editor.m_cursorCol = 0;
    editor.m_searchMode = TestEditor::SearchMode::Replace;
    editor.m_findQuery = "a";
    editor.m_replaceText = "aa";
    editor.replaceAllMatches();
    check(editor.m_lines == std::vector<std::string>{"aaaa"},
          "replace all does not reprocess replacement text");

    std::filesystem::remove_all(hostRoot, ec);
    if (failures == 0) {
        std::cout << "ALL TEXT EDITOR STATE TESTS PASSED\n";
        return 0;
    }
    std::cerr << failures << " test(s) failed\n";
    return 1;
}
