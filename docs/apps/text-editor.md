# Text Editor App

The Text Editor is a native multi-line editor for plain text files in Monolith's [internal filesystem](../filesystem.md). It supports basic editing, undo, find-in-buffer, and save/load.

## Launching

Open **Text Editor** from the Start menu, or double-click a file in the Filesystem Browser.

### Window Titles

- Bare editor (no file): `Editor`, `Editor 2`, …
- File-backed editor: `Editor - <filename>` (e.g. `Editor - welcome.txt`)

File-backed editors are singletons per path — opening the same file again focuses the existing window instead of creating a duplicate.

## Editing

- Type to insert characters at the cursor (UTF-8 text input; left/right and backspace/delete move by codepoint).
- **Enter** inserts a new line.
- **Arrow keys**, **Home**, and **End** move the cursor; hold **Shift** to extend the selection.
- Click to place the cursor; drag to select. **Esc** clears the selection.
- **Backspace** / **Delete** remove the selection when one exists, otherwise one codepoint.
- Typing or paste replaces the current selection.
- **Mouse wheel** scrolls the buffer.
- Line numbers appear in the left margin.
- Syntax highlighting colors comments, strings, numbers, and (for code files) keywords.
- A `*` in the status bar indicates unsaved changes.
- Open/save results and errors appear in the status bar (e.g. `Saved: note.txt`, `Open failed: …`).
- Closing the window or opening another file while dirty asks once via the status bar; confirm the same action again to discard, or save first (Ctrl+S).

## Syntax Highlighting

The editor applies lightweight per-line highlighting:

| Element | Color role |
|---------|------------|
| Comments (`//`, `#`) | Muted green |
| Strings (`"..."`, `'...'`) | Gold |
| Numbers | Purple |
| Keywords | Blue (code files only) |
| Everything else | Default text |

**Light mode** (`.txt` and other plain extensions): comments, double-quoted strings, and numbers only — keywords are not highlighted and apostrophes in prose (`Monolith's`) are not treated as strings.

**Code mode** (`.cpp`, `.py`, `.js`, `.rs`, `.md`, and similar): adds keyword highlighting for common programming tokens.

## Keyboard Shortcuts

| Shortcut | Action |
|----------|--------|
| Ctrl+S | Save (prompts for path if untitled) |
| Ctrl+Shift+S | Save as (path prompt) |
| Ctrl+O | Open file by virtual path |
| Ctrl+A | Select all |
| Ctrl+C | Copy selection (system clipboard) |
| Ctrl+X | Cut selection |
| Ctrl+V | Paste from clipboard (multi-line OK) |
| Ctrl+Z | Undo last edit |
| Ctrl+Y / Ctrl+Shift+Z | Redo |
| Ctrl+F | Enter find mode |
| Ctrl+H | Enter find & replace mode |
| Shift+Arrows / Home / End | Extend selection |

### Find Mode (Ctrl+F)

| Key | Action |
|-----|--------|
| Type | Build search query (matches update live) |
| Enter | Jump to next match |
| Shift+Enter | Jump to previous match |
| Tab | Switch to replace mode (focus replacement field) |
| Backspace | Delete last character of query |
| Delete | Clear query |
| Esc | Exit find mode |

The status bar shows match count (e.g. `2/5`). The current match is selected in the buffer.

### Find & Replace (Ctrl+H)

| Key | Action |
|-----|--------|
| Type | Edit the active field (find or replacement) |
| Tab | Toggle between find and replacement fields |
| Enter / Shift+Enter | Next / previous match |
| Ctrl+R | Replace current match, then jump forward |
| Ctrl+Shift+R | Replace all matches (one undo step) |
| Esc | Exit |

Replacement is case-sensitive substring match (same as find). Multi-line find is not supported.

## Saving

Ctrl+S saves to the bound path when one exists. If the buffer is untitled, Ctrl+S opens a save-as path prompt (Tab completion, Enter to confirm). Ctrl+Shift+S always opens save-as. Ctrl+O opens a path prompt starting in `/home/monolith/` (or the current file's directory).

## Current Limitations

- Open/save-as use inline path prompts, not graphical file-picker dialogs (not a multi-button dialog).
- Dirty close/open uses a second press of the same action to discard — there is no separate “Save / Discard / Cancel” modal.
- Undo/redo store full buffer snapshots (capped stack); typing still pushes per keystroke.
- Highlighting is per-line only (no multiline strings or block comments).
- No multiple buffers/tabs.
- No horizontal scroll; long lines clip (selection still works by column).
- Combining characters / complex scripts are treated as separate codepoints for cursor motion.
- Clipboard uses the host OS clipboard (SDL), not a Monolith-only buffer.
- Find/replace is case-sensitive and single-line only (no regex).
- No integration with the custom language runtime yet.

## Developer Notes

Main implementation files:

- `src/app/TextEditorApp.hpp`
- `src/app/TextEditorApp.cpp`
- `src/window/WindowManager.cpp` — `launchTextEditor()`, file singleton tracking, session restore

Shell integration: open via `openInTextEditor` / `openPath` (default for non-`.modr` files). Dirty buffers use `allowClose` and status-bar double-confirm for close/open.