# Text Editor App

The Text Editor is a native multi-line editor for plain text files in Monolith's [internal filesystem](../filesystem.md). It supports basic editing, undo, find-in-buffer, and save/load.

## Launching

Open **Text Editor** from the Start menu, or double-click a file in the Filesystem Browser.

### Window Titles

- Bare editor (no file): `Editor`, `Editor 2`, …
- File-backed editor: `Editor - <filename>` (e.g. `Editor - welcome.txt`)

File-backed editors are singletons per path — opening the same file again focuses the existing window instead of creating a duplicate.

If an initial path is missing or unreadable, the failed window falls back to the normal bare `Editor` title, remains an untitled editor, and does not reserve that path. Creating or repairing the file and opening it again retries the load normally.

## Editing

- Type to insert characters at the cursor (UTF-8 text input; cursor movement and backspace/delete stay on complete codepoint boundaries).
- **Enter** inserts a new line.
- **Arrow keys**, **Home**, and **End** move the cursor; hold **Shift** to extend the selection.
- Click to place the cursor; drag to select. **Esc** clears the selection.
- **Backspace** / **Delete** remove the selection when one exists, otherwise one codepoint.
- Typing or paste replaces the current selection.
- **Mouse wheel** scrolls the buffer.
- **Shift + mouse wheel** scrolls horizontally through long lines; the cursor also auto-scrolls into view while you type or move with the keyboard.
- Line numbers appear in the left margin.
- Empty files open as one editable blank line. A file that ends with a newline keeps its final blank line.
- Files opened with CRLF or lone-CR line endings are normalized to LF in the editor. Saving writes the document with LF separators.
- Syntax highlighting colors comments, strings, signed and unsigned numbers, and (for code files) keywords.
- A `*` in the status bar indicates unsaved changes.
- Open/save results and errors appear in the status bar (e.g. `Saved: note.txt`, `Open failed: …`).
- The status bar keeps the keyboard discovery hints visible after those feedback messages, including `Ctrl+F find` and `Ctrl+H replace`.
- The status bar height follows the shared interface font with a stable minimum, so find, replace, and path prompts remain vertically contained after Settings text scaling.
- Failed reads are reported as open errors instead of being treated as empty documents.
- Closing the window or opening another file while dirty asks once via the status bar; confirm the same action again to discard, or save first (Ctrl+S).
- A failed save clears any pending discard confirmation, so closing or opening again always asks before discarding the still-dirty buffer.
- Canceling a dirty Open prompt also clears its pending confirmation; a later Open requires a fresh confirmation before discarding the buffer.
- If another app overwrites the bound file, the editor keeps its in-memory buffer unchanged and reports the external change in the status bar. Saving afterward deliberately overwrites the file; Open can be used to load the external version instead.

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
| Ctrl+G | Go to line (status-bar number prompt) |
| Shift+Arrows / Home / End | Extend selection |

### Find Mode (Ctrl+F)

| Key | Action |
|-----|--------|
| Type | Build search query (matches update live) |
| Enter | Jump to next match |
| Shift+Enter | Jump to previous match |
| Tab | Switch to replace mode (focus replacement field) |
| Left / Right / Home / End | Move the active query caret |
| Backspace / Delete | Remove the previous or next complete UTF-8 character |
| Esc | Exit find mode |

The status bar shows match count (e.g. `2/5`). The current match is selected in the buffer.

### Find & Replace (Ctrl+H)

| Key | Action |
|-----|--------|
| Type | Edit the active field (find or replacement) |
| Tab | Toggle between find and replacement fields |
| Left / Right / Home / End | Move the active field caret |
| Backspace / Delete | Remove the previous or next complete UTF-8 character |
| Enter / Shift+Enter | Next / previous match |
| Ctrl+R | Replace current match, then jump forward |
| Ctrl+Shift+R | Replace all matches (one undo step) |
| Esc | Exit |

Replacement is case-sensitive, non-overlapping substring match (same as find). Each original match is replaced once, even when the replacement text contains the search text. Multi-line find is not supported.
Both search fields insert text at the caret, and long prompts scroll horizontally to keep the active caret visible.

## Saving

Ctrl+S saves to the bound path when one exists. If the buffer is untitled, Ctrl+S opens a save-as path prompt (Tab completion, Enter to confirm). Ctrl+Shift+S always opens save-as. Ctrl+O opens a path prompt starting in `/home/monolith/` (or the current file's directory). Save As refuses a path already open in another editor and keeps the current file binding when the destination cannot be written.

Path prompts support Left/Right/Home/End, UTF-8-safe Backspace/Delete, and insertion at the caret. Long prompts scroll horizontally to keep the caret visible at native text size. Tab completes the final path component before the caret; when the caret is inside a directory component, Tab waits until the caret is in the final component so the untouched suffix is not rewritten. If a bound path moves while a prompt is active, the prompt follows the move and keeps the caret at the same suffix position on a UTF-8 boundary.

## Current Limitations

- Open/save-as use inline path prompts, not graphical file-picker dialogs (not a multi-button dialog).
- Dirty close/open uses a second press of the same action to discard; there is no separate "Save / Discard / Cancel" modal. Changing the Open path after the warning requires a fresh confirmation.
- Undo/redo store up to 50 full buffer snapshots. Consecutive typing or in-line backspace within ~1s is one undo step; Enter, paste, and other edits start a new step.
- Highlighting is per-line only (no multiline strings or block comments).
- No multiple buffers/tabs.
- Long lines remain editable without wrapping; horizontal scrolling moves the text viewport in pixel increments while preserving document columns.
- Syntax-highlighted spans at the viewport edge are clipped without scaling, so text measurements and cursor geometry stay consistent.
- Combining characters / complex scripts are treated as separate codepoints for cursor motion.
- Clipboard uses the host OS clipboard (SDL), not a Monolith-only buffer.
- Find/replace is case-sensitive and single-line only (no regex).
- No integration with the custom language runtime yet.

## Developer Notes

Main implementation files:

- `src/app/TextEditorApp.hpp`
- `src/app/TextEditorApp.cpp`
- `src/window/detail/wm_body_07.inc` — `launchTextEditor()` and editor window creation
- `src/window/detail/wm_body_08.inc` / `wm_body_09.inc` — open routing, session restore, and file singleton bindings

Shell integration: open via `openInTextEditor` / `openPath` (default for non-`.modr` files). Dirty buffers use `allowClose` and status-bar double-confirm for close/open.

When the bound file is renamed in Filesystem Browser, moved with Filesystem Browser cut/paste, or moved with Terminal `mv`, the open editor follows the normalized virtual path. Its title, Save target, session record, singleton focus binding, and active Save/Open prompt update with it. Moving a directory also remaps open files below that directory.

If the bound file or one of its parent directories is deleted, the editor stays open with its current in-memory buffer, releases the deleted file singleton, and becomes a tracked untitled `Editor` window. Any active Save/Open prompt returns to the nearest valid parent. Use Save or Save As to choose a new path; dirty content is not discarded automatically.
