# Text Editor App

The Text Editor is a native multi-line editor for plain text files in Monolith's [internal filesystem](../filesystem.md). It supports basic editing, undo, find-in-buffer, and save/load.

## Launching

Open **Text Editor** from the Start menu, or double-click a file in the Filesystem Browser.

### Window Titles

- Bare editor (no file): `Editor`, `Editor 2`, …
- File-backed editor: `Editor - <filename>` (e.g. `Editor - welcome.txt`)

File-backed editors are singletons per path — opening the same file again focuses the existing window instead of creating a duplicate.

## Editing

- Type to insert characters at the cursor.
- **Enter** inserts a new line.
- **Arrow keys**, **Home**, and **End** move the cursor.
- **Backspace** deletes before the cursor; **Delete** deletes after.
- Line numbers appear in the left margin.
- A `*` in the status bar indicates unsaved changes.

## Keyboard Shortcuts

| Shortcut | Action |
|----------|--------|
| Ctrl+S | Save to the bound file path |
| Ctrl+Z | Undo last edit |
| Ctrl+F | Enter find mode |

### Find Mode (Ctrl+F)

| Key | Action |
|-----|--------|
| Type | Build search query (matches update live) |
| Enter | Jump to next match |
| Shift+Enter | Jump to previous match |
| Backspace | Delete last character of query |
| Delete | Clear query |
| Esc | Exit find mode |

The status bar shows match count (e.g. `2/5`) while find mode is active.

## Saving

Ctrl+S saves only when the editor has a bound virtual file path. A bare editor launched from the Start menu without opening a file cannot save until it is associated with a path (typically by opening a file from the Filesystem Browser or launching with an initial path).

## Current Limitations

- No open/save-as dialog — files must be opened via path (Filesystem Browser double-click or launcher with path).
- Undo stores full buffer snapshots; no redo yet.
- No syntax highlighting or multiple buffers/tabs.
- UTF-8 handling is basic (single-byte insertion from SDL text input).
- No integration with the custom language runtime yet.

## Developer Notes

Main implementation files:

- `src/app/TextEditorApp.hpp`
- `src/app/TextEditorApp.cpp`
- `src/window/WindowManager.cpp` — `launchTextEditor()`, file singleton tracking via `m_fileEditors`

Shell integration: Filesystem Browser calls `IWindowController::openInTextEditor(virtualPath)` to open files.