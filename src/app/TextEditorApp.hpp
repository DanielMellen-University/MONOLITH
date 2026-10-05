#pragma once

#include "App.hpp"
#include "../detail/TextTextureCache.hpp"
#include "../fs/Filesystem.hpp"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace monolith::app {

/**
 * A simple native text editor app.
 * Supports basic editing, selection + clipboard, cursor movement, scrolling,
 * and optional load/save via the Monolith filesystem.
 */
class TextEditorApp : public App {
public:
    TextEditorApp(TTF_Font* font, monolith::fs::Filesystem* fs = nullptr, const std::string& initialPath = "");
    ~TextEditorApp() override = default;

    void render(SDL_Renderer* renderer, const SDL_Rect& contentRect) override;
    void handleEvent(const SDL_Event& event) override;
    void onFocusLost() override;
    void onResize(int clientWidth, int clientHeight) override;
    void onUiScaleChanged() override;
    void onBoundFileMoved(const std::string& oldPath,
                          const std::string& newPath) override;
    void onVirtualPathMoved(const std::string& oldPath,
                            const std::string& newPath) override;
    void onVirtualPathChanged(const std::string& changedPath) override;
    void onBoundFileRemoved(const std::string& removedPath) override;
    bool allowClose() override;

    // True only after an initial or prompted open has loaded a real file.
    bool hasFilePath() const { return !m_filePath.empty(); }

    // Optional: allow external trigger to save (future use)
    bool saveCurrentFile();

private:
    enum class DiscardKind { None, Close, Open };

    // Returns true if the destructive action may proceed (clean buffer or explicit confirmation).
    bool requestDiscard(DiscardKind kind, const char* statusMessage,
                        bool explicitlyConfirmed = false);
    void clearDiscardArm();
    struct EditorState {
        std::vector<std::string> lines;
        int cursorRow = 0;
        int cursorCol = 0;
        size_t memoryBytes = 0;
    };

    // === Editing helpers ===
    void insertText(const char* text);  // full UTF-8 sequence from SDL_TEXTINPUT
    void insertNewline();
    void deleteChar();        // Backspace: one UTF-8 codepoint (or join lines)
    void deleteForward();     // Delete: one UTF-8 codepoint (or join lines)
    void moveLeft(bool extendSelection);
    void moveRight(bool extendSelection);
    void moveUp(bool extendSelection);
    void moveDown(bool extendSelection);
    void moveHome(bool extendSelection);
    void moveEnd(bool extendSelection);
    void clampCursor();
    void clampHorizontalScroll();
    void ensureCursorVisible();
    void setStatus(const std::string& message);

    // === Selection / clipboard ===
    bool hasSelection() const;
    void clearSelection();
    void prepareMove(bool extendSelection);
    void getOrderedSelection(int& r0, int& c0, int& r1, int& c1) const;
    std::string selectedFindText(int& row, int& column) const;
    std::string selectedText() const;
    bool selectedSerializedSize(size_t& bytes, size_t& lineBreaks) const;
    bool editFitsFileLimits(size_t insertedBytes, size_t insertedLineBreaks,
                            size_t removedBytes, size_t removedLineBreaks) const;
    bool selectionReplacementFits(size_t insertedBytes,
                                  size_t insertedLineBreaks) const;
    void deleteSelectionRange();  // no undo push; caller pushes if needed
    void selectAll();
    void copySelection();
    void cutSelection();
    void pasteClipboard();
    int measureTextPrefixWidth(const std::string& line, int col);
    int measureStatusCursorWidth(const std::string& text);
    bool clientToDocument(int clientX, int clientY, int& outRow, int& outCol,
                          bool clampToViewport = false) const;

    // === File I/O ===
    static constexpr size_t kMaxDocumentBytes = 16 * 1024 * 1024;
    static constexpr size_t kMaxDocumentLines = 65'536;
    static bool documentFitsFileLimits(const std::vector<std::string>& lines,
                                       size_t* serializedBytes = nullptr);
    bool savedDocumentMatchesBoundFile(bool& matches);
    bool loadInitialFile(const std::string& virtualPath);
    bool saveCurrentFile(bool confirmedExternalOverwrite,
                         bool explicitTarget = false);
    std::string getDisplayName() const;
    void updateTitleForPath();

    enum class PathPromptMode { None, Open, SaveAs, GoToLine };
    void beginPathPrompt(PathPromptMode mode);
    void remapPathPrompt(const std::string& oldPath,
                         const std::string& newPath);
    void finishPathPrompt(bool commit, bool confirmDiscard = false);
    void completePathPrompt();
    void handlePathPromptKey(const SDL_Keysym& keysym);
    void handlePathPromptText(const char* text);
    void goToLine(int lineNumber1Based);

    // === Undo / Redo ===
    enum class UndoCoalesce { None, Insert, Backspace };
    void pushUndoState(UndoCoalesce kind = UndoCoalesce::None);
    EditorState captureEditorState() const;
    static size_t measureEditorStateBytes(const std::vector<std::string>& lines);
    void clearUndoHistory();
    void clearRedoHistory();
    void trimEditorHistory();
    void undo();
    void redo();
    void applyEditorState(EditorState&& state);
    void refreshDirtyState();

    // === Find / Replace ===
    enum class SearchMode { None, Find, Replace };
    enum class SearchField { Query, Replacement };

    struct FindMatchCheckpoint {
        std::size_t index = 0;
        int row = 0;
        int col = 0;
    };
    struct FindMatchRange {
        std::pair<int, int> start{-1, -1};
        std::pair<int, int> end{-1, -1};
    };
    struct RenderedFindFragment {
        int row = 0;
        int col = 0;
        int matchStartRow = 0;
        int matchStartCol = 0;
    };
    static constexpr std::size_t kFindCheckpointStride = 256;

    void enterFindMode();
    void enterReplaceMode();
    void exitFindMode();
    void invalidateFindHighlightCache();
    void updateFindMatches();
    bool findMatchAt(int row, std::size_t col, FindMatchRange& outRange) const;
    bool findNextMatch(int& row, std::size_t& col, FindMatchRange& outRange,
                       int maxStartRow) const;
    FindMatchRange findMatchRangeAtIndex(std::size_t index) const;
    std::pair<int, int> findMatchAtIndex(std::size_t index) const;
    void moveFindMatch(int direction);
    void applyCurrentFindMatch();
    void insertSearchFieldText(const char* text);
    void pasteSearchField();
    void replaceCurrentMatch();
    void replaceAllMatches();
    void selectCurrentMatch();

    // === Syntax highlighting ===
    enum class SyntaxMode { Light, Code };

    struct ColoredSpan {
        size_t start = 0;
        size_t length = 0;
        SDL_Color color{};
    };

    struct SyntaxState {
        bool inBlockComment = false;
    };

    struct TextViewportSlice {
        std::size_t firstVisibleByte = 0;
        std::size_t visibleEndByte = 0;
        int hiddenPixelWidth = 0;
        bool measured = false;
        bool valid = false;
    };

    SyntaxMode syntaxModeForPath(const std::string& path) const;
    void refreshSyntaxMode();
    std::vector<ColoredSpan> tokenizeLine(const std::string& line,
                                          SyntaxState incoming,
                                          SyntaxState* outgoing = nullptr) const;
    void ensureSyntaxStateThrough(int lineIndex);
    void invalidateSyntaxFrom(int lineIndex);
    TextViewportSlice measureTextViewportSlice(const std::string& line,
                                                int maxWidth) const;
    void invalidateRenderedTextSlices();
    void drawColoredLine(SDL_Renderer* renderer, const std::string& line, int x, int y,
                         int maxWidth, const TextViewportSlice& slice,
                         const std::vector<ColoredSpan>& spans) const;

    // === Rendering helpers ===
    int getLineHeight() const;
    int getStatusBarHeight() const;
    int getVisibleLineCount(const SDL_Rect& contentRect) const;

    TTF_Font* m_font = nullptr;
    monolith::fs::Filesystem* m_fs = nullptr;
    mutable monolith::detail::TextTextureCache m_textTextureCache;

    std::vector<std::string> m_lines;
    std::vector<std::string> m_savedLines;
    size_t m_documentSerializedBytes = 0;
    int m_cursorRow = 0;
    int m_cursorCol = 0;
    int m_scrollOffset = 0;   // index of the first visible line
    int m_horizontalScrollOffset = 0; // text pixels hidden to the left

    // Selection: active end is always the cursor; anchor is the other end.
    bool m_hasSelection = false;
    int m_selAnchorRow = 0;
    int m_selAnchorCol = 0;
    bool m_selectingWithMouse = false;

    std::string m_filePath;   // virtual path in Monolith FS (if set)
    bool m_hasSavedFileBaseline = false;
    monolith::fs::FileStamp m_savedFileStamp;
    bool m_hasSavedFileStamp = false;
    bool m_dirty = false;
    bool m_externalChangePending = false;
    bool m_overwriteConfirmationPending = false;
    bool m_suppressChangedNotification = false;
    std::string m_statusMessage;  // transient status-bar feedback (save/open errors, etc.)
    std::string m_renderStatusText;
    std::string m_renderCursorText;
    static constexpr std::size_t kMaxRetainedTextPrefixBytes = 4096;
    std::string m_textPrefixMeasureScratch;
    DiscardKind m_discardKind = DiscardKind::None;
    std::string m_discardPath;
    bool m_closeDiscardAuthorized = false;
    bool m_closeAfterSave = false;

    std::vector<EditorState> m_undoStack;
    std::vector<EditorState> m_redoStack;
    UndoCoalesce m_undoCoalesce = UndoCoalesce::None;
    std::uint32_t m_lastCoalesceMs = 0;
    static constexpr size_t kMaxUndoStates = 50;
    static constexpr size_t kMaxUndoBytes = 64 * 1024 * 1024;
    size_t m_undoBytes = 0;
    size_t m_redoBytes = 0;
    bool m_historyBudgetExceeded = false;
    static constexpr std::uint32_t kUndoCoalesceMs = 1000;

    PathPromptMode m_pathPromptMode = PathPromptMode::None;
    std::string m_pathPromptBuffer;
    std::size_t m_pathPromptCursorPos = 0;

    // Find / replace state
    SearchMode m_searchMode = SearchMode::None;
    SearchField m_searchField = SearchField::Query;
    std::string m_findQuery;
    std::string m_replaceText;
    std::size_t m_findQueryLineBreaks = 0;
    std::size_t m_replaceTextLineBreaks = 0;
    std::vector<std::string_view> m_findSegments;
    std::vector<std::size_t> m_findMiddleFailure;
    std::size_t m_findCursorPos = 0;
    std::size_t m_replaceCursorPos = 0;
    int m_statusHorizontalScrollPx = 0;
    std::string m_statusCursorMeasureText;
    int m_statusCursorPixelWidth = 0;
    bool m_statusCursorMeasureValid = false;
    std::vector<FindMatchCheckpoint> m_findCheckpoints;
    std::size_t m_findMatchCount = 0;
    std::size_t m_currentFindMatch = 0;
    bool m_hasCurrentFindMatch = false;
    std::pair<int, int> m_currentFindPosition{-1, -1};
    std::pair<int, int> m_currentFindEndPosition{-1, -1};
    int m_renderedFindStartRow = -1;
    int m_renderedFindLineCount = -1;
    int m_renderedFindHorizontalOffset = -1;
    int m_renderedFindTextWidth = -1;
    std::vector<RenderedFindFragment> m_renderedFindVisibleMatches;
    std::vector<int> m_renderedFindPrefixWidths;
    std::vector<int> m_renderedFindVisibleWidths;

    SyntaxMode m_syntaxMode = SyntaxMode::Light;
    std::vector<SyntaxState> m_syntaxLineStates;
    int m_renderedSyntaxStartRow = -1;
    std::vector<std::vector<ColoredSpan>> m_renderedSyntaxSpans;
    int m_renderedTextSliceStartRow = -1;
    int m_renderedTextSliceWidth = -1;
    int m_renderedTextSliceOffset = -1;
    std::vector<TextViewportSlice> m_renderedTextSlices;

    static constexpr int kStatusBarHeight = 22;
    static constexpr int kPadding = 8;
    static constexpr int kLineNumWidth = 40;

    // Cached for layout
    int m_clientWidth = 0;
    int m_clientHeight = 0;
};

} // namespace monolith::app
