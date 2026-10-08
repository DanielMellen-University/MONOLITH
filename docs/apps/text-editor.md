# Text Editor

A plain-text editor for files in the [internal filesystem](../filesystem.md), with undo, find and replace, go to line and light syntax highlighting.

## Using it

Open **Text Editor** from the Start menu or desktop, double-click a file in Filesystem, or run `edit <file>` in Terminal.

- An editor without a file is titled `Editor`, `Editor 2` and so on. An editor with a file is titled `Editor - <name>`.
- Each file opens in at most one editor. Opening it again focuses the existing window.
- If the file cannot be opened, you get an untitled `Editor` and the status bar says why.
- Line numbers are shown on the left. The status bar shows the file, a `*` when there are unsaved changes, messages, and key hints such as `Ctrl+F find`.

## Keyboard shortcuts

| Key | Action |
|-----|--------|
| Ctrl+S | Save (asks for a path if the document is untitled) |
| Ctrl+Shift+S | Save As |
| Ctrl+O | Open a file by path |
| Ctrl+D | Confirm: discard changes after a Close or Open warning, or overwrite a file changed on disk |
| Ctrl+Z | Undo |
| Ctrl+Y, Ctrl+Shift+Z | Redo |
| Ctrl+A | Select all |
| Ctrl+C / Ctrl+X / Ctrl+V | Copy / cut / paste (system clipboard, multi-line) |
| Ctrl+F | Find |
| Ctrl+H | Find and replace |
| Ctrl+G | Go to line |
| Arrows, Home, End | Move the cursor |
| Shift + movement keys | Extend the selection |
| Backspace / Delete | Delete the selection, or one character |
| Esc | Clear the selection, or close the active prompt |
| Mouse click / drag | Place the cursor / select |
| Mouse wheel / Shift + wheel | Scroll vertically / horizontally |

There is no Page Up or Page Down handling.

### In Find (Ctrl+F) and Replace (Ctrl+H)

If text is selected when you press Ctrl+F or Ctrl+H, it becomes the search query.

| Key | Action |
|-----|--------|
| Type | Edit the active field; matches update as you type |
| Enter / Shift+Enter | Next / previous match |
| Tab | Switch between the find and replace fields (from Find, opens Replace) |
| Ctrl+Enter | Insert a line break in the field |
| Ctrl+V | Paste into the field |
| Left / Right / Home / End, Backspace / Delete | Edit the field |
| Ctrl+R | Replace the current match and move to the next (Replace only) |
| Ctrl+Shift+R | Replace all matches as one undo step (Replace only) |
| Esc | Leave Find or Replace |

The status bar shows the match position, for example `2/5`. Search is case-sensitive, plain text (no regular expressions) and non-overlapping. A query with line breaks (shown as `\n`) matches across consecutive lines. Replacing a match with identical text changes nothing and adds no undo step.

## Opening and saving

- Open and Save As use a path prompt in the status bar. It starts in the current file's folder, or `/home/monolith/`.
- Tab completes the last path component. `~` means `/home/monolith`. If nothing matches, the status bar says `No path matches.`
- If an Open, Save As or Go to Line attempt fails, the prompt stays open with your text so you can fix it. Esc cancels.
- Save As refuses a path that is already open in another editor.
- Files are read and written in 16 KiB chunks. Windows (CRLF) and old Mac (CR) line endings are converted to LF on open, and files are saved with LF.

## Unsaved changes

When you close the window or open another file with unsaved changes, the status bar asks what to do:

| Key | Result |
|-----|--------|
| Ctrl+S | Save. After a Close, the window then closes. |
| Ctrl+D | Discard the changes |
| Esc | Cancel |

Pressing Close or Enter again never discards on its own. Shut Down is blocked while any editor has unsaved changes.

## Files changed outside the editor

- If another app or host program changes the open file, the editor keeps your text and shows `[external change]` in the status bar until you reload, save or close the file.
- Before every save, the editor checks whether the file on disk still matches what it last loaded or saved. If it differs, or is missing, Ctrl+S pauses: Ctrl+D overwrites, Esc keeps the disk version.
- Opening the same path again reloads the version on disk.
- If the file is renamed or moved (in Filesystem or with `mv`), the editor follows it: title, save target and session entry update.
- If the file or a parent folder is deleted, the editor keeps your text and becomes untitled. Use Save As to choose a new path.

How the check and the save interact with other writers is described in [atomic-writes.md](../internals/atomic-writes.md#conditional-writes).

## Syntax highlighting

| Element | Color |
|---------|-------|
| Comments | Muted green |
| Strings | Gold |
| Numbers | Purple |
| Keywords (code files only) | Blue |

- **Plain files** (`.txt` and other extensions): `//` and `#` line comments, double-quoted strings and numbers. Apostrophes are not treated as strings.
- **Code files**: adds keywords, single-quoted strings and `/* ... */` block comments across lines. Code extensions: `c cc cpp cxx h hh hpp hxx py js ts rs go java cs lua sh bash zsh json yaml yml toml xml html css md`.

Highlighting is a simple token scan, not a language parser. Multi-line string literals are not recognized.

## Limits

- Documents: 16 MiB and 65,536 lines. A larger file is refused on open and the current document stays as it was. Edits, pastes and replacements that would go over either limit are refused.
- Undo and redo: 50 states and about 64 MiB in total. Typing or backspacing within about a second is one undo step. If a single snapshot is too large to keep, the status bar says history is unavailable.
- One document per window; no tabs.
- No line wrapping. Long lines scroll horizontally.
- Combining characters are separate cursor positions.
- No file picker dialog; Open and Save As use the path prompt.

## Developer notes

- `src/app/TextEditorApp.{hpp,cpp}`: the editor.
- `src/window/detail/wm_body_07.inc`: `launchTextEditor()` and window creation.
- Open routing, file singletons and session restore are in the window manager; see [window-manager.md](../internals/window-manager.md).
- Closing with unsaved changes goes through the app's `allowClose` callback.
- Rendering and caching details: [rendering.md](../internals/rendering.md#text-editor).
- Test: `test_text_editor_state`.
