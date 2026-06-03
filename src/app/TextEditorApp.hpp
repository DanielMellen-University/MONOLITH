#pragma once

#include "App.hpp"
#include "../fs/Filesystem.hpp"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <string>
#include <vector>

namespace monolith::app {

/**
 * A simple native text editor app.
 * Supports basic editing, cursor movement, scrolling, and optional load/save via the Monolith filesystem.
 */
class TextEditorApp : public App {
public:
    TextEditorApp(TTF_Font* font, monolith::fs::Filesystem* fs = nullptr, const std::string& initialPath = "");
    ~TextEditorApp() override = default;

    void render(SDL_Renderer* renderer, const SDL_Rect& contentRect) override;
    void handleEvent(const SDL_Event& event) override;
    void onResize(int clientWidth, int clientHeight) override;

    // Optional: allow external trigger to save (future use)
    bool saveCurrentFile();

private:
    // === Editing helpers ===
    void insertChar(char c);
    void insertNewline();
    void deleteChar();        // Backspace behavior
    void deleteForward();     // Delete key behavior
    void moveLeft();
    void moveRight();
    void moveUp();
    void moveDown();
    void moveHome();
    void moveEnd();
    void clampCursor();
    void ensureCursorVisible();

    // === File I/O ===
    void loadInitialFile(const std::string& virtualPath);
    std::string getDisplayName() const;

    // === Undo ===
    void pushUndoState();
    void undo();

    // === Find ===
    void enterFindMode();
    void exitFindMode();
    void updateFindMatches();
    void moveFindMatch(int direction);
    void applyCurrentFindMatch();
    int findMatchIndexAt(int row, int col) const;

    // === Rendering helpers ===
    int getLineHeight() const;
    int getVisibleLineCount(const SDL_Rect& contentRect) const;

    TTF_Font* m_font = nullptr;
    monolith::fs::Filesystem* m_fs = nullptr;

    std::vector<std::string> m_lines;
    int m_cursorRow = 0;
    int m_cursorCol = 0;
    int m_scrollOffset = 0;   // index of the first visible line

    std::string m_filePath;   // virtual path in Monolith FS (if set)
    bool m_dirty = false;

    // Very basic undo stack (saves full state before major edits)
    struct EditorState {
        std::vector<std::string> lines;
        int cursorRow;
        int cursorCol;
    };
    std::vector<EditorState> m_undoStack;

    // Find state (basic)
    bool m_findMode = false;
    std::string m_findQuery;
    std::vector<std::pair<int, int>> m_findMatches;  // row, col starts
    int m_currentFindMatch = -1;

    // Cached for layout
    int m_clientWidth = 0;
    int m_clientHeight = 0;
};

} // namespace monolith::app
