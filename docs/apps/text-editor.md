# Text Editor App

The Text Editor is a native multi-line editor for plain text files in Monolith's [internal filesystem](../filesystem.md). It supports basic editing, undo, find-in-buffer, and save/load.

## Launching

Open **Text Editor** from the Start menu, or double-click a file in the Filesystem Browser.

### Window Titles

- Bare editor (no file): `Editor`, `Editor 2`, …
- File-backed editor: `Editor - <filename>` (e.g. `Editor - welcome.txt`)

File-backed editors are singletons per path — opening the same file again focuses the existing window instead of creating a duplicate.

If an initial path is missing or unreadable, the failed window falls back to the normal bare `Editor` title, remains an untitled editor, and does not reserve that path. Creating or repairing the file and opening it again retries the load normally.

Saving a bare editor with Save As turns it into a file-backed editor and releases its old bare `Editor` instance number; other bare editors compact their titles immediately.

If a bound file's parent directory is deleted, the editor keeps the document buffer but releases the file binding. An active Open or Save As prompt returns to the nearest surviving parent directory.

## Editing

- Type to insert characters at the cursor (UTF-8 text input; cursor movement and backspace/delete stay on complete codepoint boundaries).
- **Enter** inserts a new line.
- **Arrow keys**, **Home**, and **End** move the cursor; hold **Shift** to extend the selection.
- Click to place the cursor; drag to select. A captured drag clamps to the nearest visible document edge when the pointer leaves the text viewport, so selections can end at the top or bottom without stopping early. **Esc** clears the selection.
- Losing focus ends an in-progress mouse selection without discarding the selected text, so opening the Start menu cannot leave later pointer motion attached to the editor.
- **Backspace** / **Delete** remove the selection when one exists, otherwise one codepoint.
- Typing or paste replaces the current selection.
- Typing or pasting exactly the selected text only collapses the selection; it does not dirty the buffer or add an undo step.
- **Mouse wheel** scrolls the buffer.
- **Shift + mouse wheel** scrolls horizontally through long lines; the cursor also auto-scrolls into view while you type or move with the keyboard.
- Line numbers appear in the left margin.
- Empty files open as one editable blank line. A file that ends with a newline keeps its final blank line.
- Files opened with CRLF or lone-CR line endings are normalized to LF in the editor. Saving writes the document with LF separators.
- File opens and saves stream in bounded 16 KiB chunks. Documents above 16 MiB or 65,536 lines are rejected on open and save; a rejected open leaves the current document untouched. Typing, Enter, paste, Replace, and Replace All also reject edits that would exceed either limit without changing the document or undo history. Clipboard input is bounded before it is normalized.
- Syntax highlighting colors comments, strings, signed and unsigned numbers, and (for code files) keywords.
- A `*` in the status bar indicates that the buffer differs from the last loaded or saved content; undoing back to that content clears the marker.
- Open/save results and errors appear in the status bar (e.g. `Saved: note.txt`, `Open failed: …`).
- The status bar keeps the keyboard discovery hints visible after those feedback messages, including `Ctrl+F find` and `Ctrl+H replace`.
- The status bar height follows the shared interface font with a stable minimum, so find, replace, and path prompts remain vertically contained after Settings text scaling.
- Find, Replace, Go to Line, Open, and Save As prompts reuse the caret-prefix width until the caret text or shared font changes.
- Failed reads are reported as open errors instead of being treated as empty documents.
- Recoverable Go to Line, Open, and Save As validation or I/O failures keep the prompt active with its text, caret, and horizontal position, so the input can be corrected and retried. Escape cancels it.
- Closing the window or opening another file while dirty shows explicit status-bar choices: **Ctrl+D** discards, **Ctrl+S** saves, and **Esc** cancels. Save on a Close warning closes the window after a successful save; repeating Close or Enter never discards by itself.
- For a dirty Open, Enter validates the target and shows the decision; Ctrl+D opens it and discards the current buffer. Changing the target requires a fresh Ctrl+D decision. Ctrl+S saves the current document and cancels the Open prompt so it can be reopened afterward.
- A global Shut Down request also stays blocked while a dirty Editor remains open. Save or explicitly discard and close each dirty Editor, then retry Shut Down.
- A failed save clears any pending discard confirmation, so closing or opening again always asks before discarding the still-dirty buffer.
- Canceling a dirty Open prompt also clears its pending confirmation; a later Open requires a fresh confirmation before discarding the buffer.
- If another app overwrites the bound file, the editor keeps its in-memory buffer unchanged and reports the external change in the status bar. Before each save to the bound path, it compares the current file stamp with the last loaded/saved version; a changed stamp triggers an exact stream comparison, detecting host edits and other Monolith processes even without a change notification. Unchanged stamps avoid the extra read. Content comparison normalizes CRLF and CR line endings to the editor's LF document model. An `[external change]` marker remains visible through other status updates until the file is reloaded, successfully saved, or unbound. A normal Ctrl+S pauses before replacing a different or missing disk version; press Ctrl+D to overwrite it or Esc to keep the external version. The conditional atomic save checks the target version again immediately before replacement. If another write arrives while the save is being generated, the newer file is preserved and Ctrl+D must be pressed again to retry; a writer can still race in the final stamp-check-to-rename interval. Opening the currently bound path reloads the external version. Ctrl+D still discards dirty text only after a Close or Open warning.

## Syntax Highlighting

The editor applies lightweight syntax highlighting:

| Element | Color role |
|---------|------------|
| Comments (`//`, `#`, `/* ... */`) | Muted green |
| Strings (`"..."`, `'...'`) | Gold |
| Numbers | Purple |
| Keywords | Blue (code files only) |
| Everything else | Default text |

**Light mode** (`.txt` and other plain extensions): line comments (`//`, `#`), double-quoted strings, and numbers only — keywords and block comments are not highlighted, and apostrophes in prose (`Monolith's`) are not treated as strings.

**Code mode** (`.cpp`, `.py`, `.js`, `.rs`, `.md`, and similar): adds keyword highlighting for common programming tokens and carries C-style `/* ... */` block comments across lines. The editor caches lexical state through the visible rows and reuses syntax spans for the current viewport; edits invalidate downstream lexical state and visible spans.

## Keyboard Shortcuts

| Shortcut | Action |
|----------|--------|
| Ctrl+S | Save (prompts for path if untitled) |
| Ctrl+Shift+S | Save as (path prompt) |
| Ctrl+O | Open file by virtual path |
| Ctrl+D | Confirm an external-file overwrite, or discard dirty text after a Close or Open warning |
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

When a single line of text is selected, Ctrl+F starts with that text as the query and selects its current occurrence. Multi-line or control-bearing selections start with an empty query.

| Key | Action |
|-----|--------|
| Type | Build search query (matches update live) |
| Enter | Jump to next match |
| Shift+Enter | Jump to previous match |
| Tab | Switch to replace mode (focus replacement field) |
| Left / Right / Home / End | Move the active query caret |
| Backspace / Delete | Remove the previous or next complete UTF-8 character |
| Ctrl+V | Paste at the query caret; line breaks and other control characters are removed |
| Esc | Exit find mode |

The status bar shows match count (e.g. `2/5`). The current match is selected in the buffer. Search stores one location checkpoint per 256 non-overlapping matches, resolving navigation from the nearest checkpoint instead of retaining every hit; highlight geometry is built only for matches intersecting the visible text viewport.

### Find & Replace (Ctrl+H)

When started from the editor with a single-line selection, Ctrl+H uses that text as the query and selects the matching occurrence. Multi-line or control-bearing selections start with an empty query.

| Key | Action |
|-----|--------|
| Type | Edit the active field (find or replacement) |
| Tab | Toggle between find and replacement fields |
| Left / Right / Home / End | Move the active field caret |
| Backspace / Delete | Remove the previous or next complete UTF-8 character |
| Ctrl+V | Paste at the active field caret; line breaks and other control characters are removed |
| Enter / Shift+Enter | Next / previous match |
| Ctrl+R | Replace current match, then jump forward |
| Ctrl+Shift+R | Replace all matches (one undo step) |
| Esc | Exit |

Replacement is case-sensitive, non-overlapping substring match (same as find). Each original match is replaced once, even when the replacement text contains the search text. Multi-line find is not supported.
Replacing with the same text is a no-op: it leaves the buffer clean and does not add an undo step.
Both search fields insert text at the caret, and long prompts scroll horizontally to keep the active caret visible.
Each field is bounded to 16 MiB; input that would exceed the limit is rejected, and a UTF-8 character is never split at the boundary.
Long Find/Replace values keep their full search text, while the status bar renders a bounded UTF-8-safe excerpt around each field caret. Match highlights measure only the portion intersecting the visible text viewport.

## Saving

Ctrl+S saves to the bound path when one exists. If the buffer is untitled, Ctrl+S opens a save-as path prompt (Tab completion, Enter to confirm). Ctrl+Shift+S always opens save-as. Ctrl+O opens a path prompt starting in `/home/monolith/` (or the current file's directory). Save As refuses a path already open in another editor and keeps the current file binding when the destination cannot be written. A newly saved path is claimed before its creation notification is broadcast, so another app opening it synchronously focuses this editor. Tab reports `No path matches.` when the current prefix has no eligible entries.

A leading `~` or `~/` in Open and Save As expands to `/home/monolith`; completion searches that directory while keeping the shorthand in the prompt until accepted. `~name` is not expanded.

Path prompts support Left/Right/Home/End, UTF-8-safe Backspace/Delete, and insertion at the caret. Long prompts scroll horizontally to keep the caret visible at native text size. Tab completes the final path component before the caret; when the caret is inside a directory component, Tab waits until the caret is in the final component so the untouched suffix is not rewritten. Ambiguous completions extend only through complete UTF-8 codepoints, so filenames that share leading bytes cannot leave an invalid half-character in the prompt. If a file or directory in the active prompt moves, the prompt follows the move and keeps the caret at the same suffix position on a UTF-8 boundary. Starting a prompt ends an in-progress mouse selection so the prompt's input cannot leave a stale drag active. When Go to Line validation or an Open/Save As operation fails, its prompt remains active with the attempted input and caret for correction; successful operations, singleton focus redirects, and Escape close it.

## Current Limitations

- Open/save-as use inline path prompts, not graphical file-picker dialogs (not a multi-button dialog).
- Dirty close/open decisions use status-bar keyboard choices rather than a modal dialog; Close/Open can still be canceled with Esc, and changing the Open target requires a fresh explicit discard decision.
- Undo/redo share a limit of 50 full-buffer states and an estimated 64 MiB of snapshot memory. Oldest undo states are evicted first; snapshots larger than the budget are not retained, and the status bar explains when history is unavailable for that reason. Consecutive typing or in-line backspace within ~1s remains one undo step; Enter, paste, and other edits start a new step.
- Multiline string literals and language-specific syntax edge cases are not parsed; highlighting is a lightweight token scan, not a full language parser.
- No multiple buffers/tabs.
- Open and save are limited to 16 MiB and 65,536 lines so large or newline-heavy files cannot cause unbounded editor allocations.
- Long lines remain editable without wrapping; horizontal scrolling moves the text viewport in pixel increments while preserving document columns.
- Long-line rendering rasterizes only UTF-8 syntax-span slices near the visible text viewport. A renderer-aware LRU reuses those textures between frames and across edits, scrolling, and resizing; it is capped at 256 entries and an estimated 16 MiB, and clears on renderer or UI-scale changes.
- Horizontal scrolling is clamped to the current line after wheel input and window resizing, so widening the editor cannot leave the text viewport stranded past the line end.
- Resizing clamps vertical scrollback to the lines that fit in the new editor area. If the client is too short to fit a document row above the status bar, the editor leaves the document area empty instead of claiming rows that cannot be rendered.
- Direct render-size changes use the same resize path, so client dimensions and scroll bounds stay synchronized even before the next Window Manager resize callback.
- Mouse selection starts only on complete rendered rows; the unused gap above the status bar is not treated as document content.
- A captured mouse selection extends to the nearest visible document edge while the pointer is outside the text viewport, matching the shell's pointer-capture behavior for other drag-based apps.
- Syntax-highlighted spans at the viewport edge are clipped without scaling, so text measurements and cursor geometry stay consistent.
- Document rows and the status prompt intersect their internal clips with the caller clip and restore it after rendering, so editor content stays inside the shell's visible client intersection.
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

Shell integration: open via `openInTextEditor` / `openPath` (default for non-`.modr` files). Dirty buffers use `allowClose` and explicit status-bar Ctrl+D decisions for close/open.

When the bound file is renamed in Filesystem Browser, moved with Filesystem Browser cut/paste, or moved with Terminal `mv`, the open editor follows the normalized virtual path. Its title, Save target, session record, singleton focus binding, and active Save/Open prompt update with it. Moving a directory also remaps open files below that directory.

If the bound file or one of its parent directories is deleted, the editor stays open with its current in-memory buffer, releases the deleted file singleton, and becomes a tracked untitled `Editor` window. Any active Save/Open prompt returns to the nearest valid parent. Use Save or Save As to choose a new path; dirty content is not discarded automatically.

The editor caches lexical spans for its current rendered rows so steady-state frames do not retokenize unchanged visible lines. The cache is bounded to the viewport and is rebuilt when its row range, document text, or syntax mode changes. Visible syntax-span slices, line numbers, and status text are separately cached as renderer-owned textures in a 256-entry LRU with an estimated 16 MiB budget. Text, color, and font form the texture-cache key, so unchanged content remains reusable across edits, status changes, prompts, scrolling, and resizing; renderer switches and shared interface text-scale changes clear it. Long-line UTF-8 byte bounds and pixel offsets are reused for unchanged viewport rows, then recalculated when the visible row range, content width, horizontal scroll, document text, or font changes. Only syntax spans intersecting the horizontal viewport are rasterized. Mouse hit testing measures the clicked prefix with one `TTF_MeasureUTF8` pass and maps measured characters back to byte columns, avoiding repeated prefix allocations and measurements on long lines. Cursor and selection width checks use a retained 4 KiB prefix scratch buffer, with temporary storage only for longer prefixes so retained memory stays bounded. Find retains one location checkpoint per 256 non-overlapping results and resolves navigation from the nearest checkpoint; viewport scans begin from the nearest preceding checkpoint, preserving non-overlapping behavior without rescanning dense prefixes. It caches highlight geometry and measures match widths only for visible slices. Scrolling, query updates, and shared-font changes invalidate that geometry. Prompt caret-prefix width is cached by exact text and invalidated after shared-font scaling.

Undo and redo move document line buffers between the active editor and their history stacks. Snapshot accounting includes the line vector and its string storage, measured as each snapshot line is cloned after the active document passes the budget precheck. It is an estimate used to keep retained history bounded, not a limit on the active document itself.
