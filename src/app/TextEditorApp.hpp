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
    bool allowClose() override;

    // Optional: allow external trigger to save (future use)
    bool saveCurrentFile();

private:
    enum class DiscardKind { None, Close, Open };

    // Returns true if the destructive action may proceed (clean buffer, or second confirm).
    bool requestDiscard(DiscardKind kind, const char* statusMessage);
    void clearDiscardArm();
    struct EditorState {
        std::vector<std::string> lines;
        int cursorRow;
        int cursorCol;
    };

    // === Editing helpers ===
    void insertText(const char* text);  // full UTF-8 sequence from SDL_TEXTINPUT
    void insertNewline();
    void deleteChar();        // Backspace: one UTF-8 codepoint (or join lines)
    void deleteForward();     // Delete: one UTF-8 codepoint (or join lines)
    void moveLeft();
    void moveRight();
    void moveUp();
    void moveDown();
    void moveHome();
    void moveEnd();
    void clampCursor();
    void ensureCursorVisible();
    void setStatus(const std::string& message);

    // === File I/O ===
    bool loadInitialFile(const std::string& virtualPath);
    std::string getDisplayName() const;
    void updateTitleForPath();

    enum class PathPromptMode { None, Open, SaveAs };
    void beginPathPrompt(PathPromptMode mode);
    void finishPathPrompt(bool commit);
    void completePathPrompt();
    void handlePathPromptKey(const SDL_Keysym& keysym);
    void handlePathPromptText(const char* text);

    // === Undo / Redo ===
    void pushUndoState();
    void undo();
    void redo();
    void applyEditorState(const EditorState& state);

    // === Find ===
    void enterFindMode();
    void exitFindMode();
    void updateFindMatches();
    void moveFindMatch(int direction);
    void applyCurrentFindMatch();
    int findMatchIndexAt(int row, int col) const;

    // === Syntax highlighting ===
    enum class SyntaxMode { Light, Code };

    struct ColoredSpan {
        size_t start = 0;
        size_t length = 0;
        SDL_Color color{};
    };

    SyntaxMode syntaxModeForPath(const std::string& path) const;
    void refreshSyntaxMode();
    std::vector<ColoredSpan> tokenizeLine(const std::string& line) const;
    void drawColoredLine(SDL_Renderer* renderer, const std::string& line, int x, int y,
                         int maxWidth) const;

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
    std::string m_statusMessage;  // transient status-bar feedback (save/open errors, etc.)
    DiscardKind m_discardKind = DiscardKind::None;

    std::vector<EditorState> m_undoStack;
    std::vector<EditorState> m_redoStack;

    PathPromptMode m_pathPromptMode = PathPromptMode::None;
    std::string m_pathPromptBuffer;

    // Find state (basic)
    bool m_findMode = false;
    std::string m_findQuery;
    std::vector<std::pair<int, int>> m_findMatches;  // row, col starts
    int m_currentFindMatch = -1;

    SyntaxMode m_syntaxMode = SyntaxMode::Light;

    static constexpr int kStatusBarHeight = 22;

    // Cached for layout
    int m_clientWidth = 0;
    int m_clientHeight = 0;
};

} // namespace monolith::app
