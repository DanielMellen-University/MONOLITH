# Terminal App

The Terminal is Monolith's command-line interface. It provides scrollback output, an editable input line, command history, tab completion, and a set of built-in filesystem commands against the [internal filesystem](../filesystem.md).

## Launching

Open **Terminal** from the Start menu. Multiple instances are supported with automatic numbering:

- `Terminal`
- `Terminal 2`
- `Terminal 3`

Closing a lower-numbered window renumbers survivors (e.g. closing `Terminal` promotes `Terminal 2` to `Terminal`).

## Input Line

The prompt shows an abbreviated working directory, for example:

```text
~>
```

When you are in a subdirectory of home, the path is shown relative to home:

```text
~/documents>
```

(`~` stands for `/home/monolith`.)

Type commands at the prompt and press **Enter** to run them. Output appears above the input line.

## Keyboard Shortcuts

| Key | Action |
|-----|--------|
| Enter | Run the current command |
| Up / Down | Navigate command history |
| Left / Right | Move cursor within the input line |
| Home / End | Jump to start / end of input |
| Backspace / Delete | Delete the previous or next complete UTF-8 character |
| Shift + Left / Right | Extend the selection by complete UTF-8 characters |
| Shift + Home / End | Extend the selection to the start / end of input |
| Ctrl+A | Select the full input line |
| Ctrl+C / Ctrl+X | Copy / cut the selected input text |
| Ctrl+V | Paste clipboard text at the caret or over the selection |
| Mouse drag in the input bar | Select input text |
| Tab | Complete command name or filesystem path |
| Ctrl+R | Enter reverse history search |
| Ctrl+R (in search) | Find older matching command |
| Enter (in search) | Accept matched command |
| Esc (in search) | Cancel search, restore input |
| Page Up / Page Down | Scroll output history |
| Mouse wheel | Scroll output history |
| Shift+Page Up / Shift+Page Down | Pan long output rows right / left |

While reverse search is active, each additional `Ctrl+R` moves to the next older matching command. When no older match remains, the current match stays selected.

Once a history entry is recalled, typing, Backspace, Delete, or a completion
that changes the command returns to normal input editing. Pressing Down after
that keeps the edited command instead of restoring the pre-navigation buffer.
When Down returns to an untouched draft, it also restores the draft's original
caret position.

## Built-in Commands

Run `help` for the full list. Current commands:

| Command | Description |
|---------|-------------|
| `echo <text>` | Print text |
| `clear` | Clear the screen |
| `date` | Show current date and time |
| `whoami` | Print current user (`monolith`) |
| `version` / `ver` | Show Monolith version |
| `ls [path]` | List directory contents (`▶` = directory, `•` = file); a file path prints that file and a missing path reports an error |
| `pwd` | Print working directory |
| `cd [dir]` | Change directory (no arg → `/home/monolith`) |
| `mkdir <dir>` | Create directory |
| `touch <file>` | Create an empty file or update an existing file's modification time |
| `cat <file>` | Show file contents |
| `edit <file>` | Open a text file in the Text Editor |
| `open <path>` | Open via shell routing (case-insensitive `.modr` → Drawing, else Text Editor) |
| `cp [-r] <src> <dst>` | Copy file or directory tree (`Filesystem::copyRecursive`; verifies file reads and refuses copy into self) |
| `mv <src> <dst>` | Move or rename (destination directory supported; open Editor and Drawing bindings follow the move) |
| `rm [-r] <path>` | Remove file or directory tree (`Filesystem::removeRecursive` with `-r`; cannot remove `/`) |
| `history` | Show command history |
| `help` | Show command list |
| `exit` / `quit` | Close this terminal window |

Paths may be absolute or relative to the current working directory. Quoted paths preserve their exact whitespace, including repeated spaces. Tab completion works for both command names and paths, including a blank command or path slot after whitespace, the virtual root (`/`), paths inside double or single quotes, filenames containing apostrophes, and paths with backslash-escaped spaces. The input cursor, selection, and deletion move through complete UTF-8 characters, so accented characters and emoji are not split into invalid byte fragments. Shift + Left/Right and Shift + Home/End extend a keyboard selection; dragging in the input bar selects with the mouse. Ctrl+C and Ctrl+X do nothing when there is no selection. Paste filters control characters and maps tabs and line breaks to spaces, keeping the clipboard in the single-line prompt without silently dropping later lines or running them as separate commands. Long commands scroll horizontally to keep the caret visible.

Reverse history search has its own editable query. Left/Right/Home/End move through the query, typed text is inserted at the caret, Delete removes the next complete UTF-8 character, and Backspace removes the previous one. Long search queries scroll horizontally to keep the caret visible. Up/Down cancel search and return to normal history navigation. Canceling restores both the original input and its caret position. Accepting or canceling a search also clears any older Up/Down navigation state, so the accepted or restored input is not overwritten by a stale history slot.

Editing the search query reselects the newest history entry that matches the full query, including when the query is shortened. `Ctrl+R` remains the explicit action for walking to older matches.

Completion replaces only the token text before the cursor. Opening quotes remain in place; a single file completion at the end of an open double- or single-quoted path adds the matching closing quote automatically. Apostrophes inside single-quoted completions use a close, escaped apostrophe, and reopen sequence so the completed command stays one argument. Directory completions keep the quote open and add a trailing slash so another Tab can continue into that directory. Unquoted completions escape spaces, backslashes, and quote characters so the completed command keeps the same meaning when it runs. Ambiguous completions stop at a complete UTF-8 codepoint, so filenames that share only leading bytes cannot insert an invalid partial character.

When the cursor is immediately after a closed quoted token, Tab does nothing. The existing closing quote is already part of a complete command, so completion does not rewrite it or reopen the quote.

`cat` reads in bounded 16 KiB chunks instead of copying the entire file before displaying it. CRLF and lone-CR separators are normalized as they are read, including when CRLF crosses a chunk boundary. It stops at 5,000 output lines, keeps at most 64 KiB of an in-progress row, and reports read failure separately from an empty file.

After a successful `mv`, any open Text Editor or Drawing window bound to the source path follows the normalized destination path. Moving a directory also updates bindings for open files beneath it, and any Terminal or Filesystem Browser currently inside that directory follows the new location.

`touch` creates a missing empty file or updates the last-write time of an existing regular file without changing its contents. Existing-file touches do not send a content-change notification; timestamps are not displayed by Monolith.

After a successful `mkdir`, a newly created `touch` target, or `cp`, open Filesystem Browser windows refresh when the new or changed entry belongs directly to the folder they are viewing. If the operation also creates missing parent directories, ancestor Browser windows refresh as well. Recursive `cp -r` into an existing directory reports the destination tree's changed paths in directory-first depth-first order, so open Text Editor and Drawing windows below that tree receive external-change notifications too. The walk uses the entry types already returned by each directory listing and an explicit stack. Creating or overwriting a file by saving a Text Editor document or Drawing sketch uses the same notification path. If the changed path is the active wallpaper image, the desktop reloads it on the next render.

The shared Filesystem Browser clipboard also follows a moved source, so a pending Copy or Cut can still be pasted after another window renames or moves that source.

After a successful `rm`, open Editor and Drawing windows keep their in-memory work but release deleted paths and become untitled tracked windows. A Terminal whose current directory was inside the removed tree moves to the nearest existing parent. Deleted paths are also removed from the shared cut clipboard.

## Command History

Command history persists across sessions in:

```text
/home/monolith/.terminal_history
```

History is saved after each submitted command through atomic replacement in bounded 16 KiB writes. Command history retains at most 500 entries and 2 MiB total, with a 64 KiB limit per command; older entries drop first. A command longer than 64 KiB still runs, but Terminal reports that it was omitted from history. Startup seeks to a bounded tail of the history file and parses it in chunks, preserving the newest entries that fit those limits; oversized legacy files are rewritten in the bounded format. On-screen scrollback retains at most 2,000 rows and 8 MiB; an individual row is capped at 64 KiB and marked `[truncated]`. Oldest rows and their viewport measurements are evicted from the front without shifting the retained scrollback on each new output row.

If writing command history fails, the command still runs and Terminal reports that the history was not saved. Each later command that fits the history-entry limit retries persistence; the first successful retry reports that history saving recovered.

History loading accepts both Unix and Windows line endings, so recalled commands do not carry a hidden carriage return into command parsing.

Output scrolling is bounded to the history rows that fit above the input strip. Page Up, Page Down, and the mouse wheel cannot scroll beyond the oldest fully visible output, and resizing or changing the interface text scale clamps the saved scroll position to the new history area. Use Shift+Page Up/Down to pan long rows horizontally; the pan snaps to complete UTF-8 codepoints and applies to the rows currently visible. New output returns to the left edge. If the client is too short to expose a history row, the Terminal leaves the history area empty instead of painting through the input strip.

Direct render-size changes update the cached client geometry before scroll bounds are calculated.

The input strip remains inside the client rectangle even when a window is resized below its normal text height.
The history viewport also clamps both width and height to zero for clients smaller than its padding, so narrow windows do not create invalid clip rectangles.
Terminal intersects its input and history clips with the caller's renderer clip and restores that clip after each region.

Terminal measures each scrollback row's visible UTF-8 range and rasterizes only that viewport-sized segment, so off-screen text never creates a full-width texture. Row widths are measured lazily for horizontal panning; visible byte ranges are cached by row, viewport width, and horizontal offset. Resize and shared-font changes clamp or reset the pixel-based pan state. Scrollback rows, command-input fragments, and reverse-search text share a renderer-aware 256-entry LRU with an estimated 16 MiB budget. Text, font, and color are part of each key, so unchanged rows and prompt fragments survive output changes, scrolling, and resizing; renderer switches and shared interface text-scale changes clear the cache. Cached input-prefix texture widths also drive cursor placement, avoiding separate font measurement on steady-state renders.

## Argument Quoting

Whitespace splits arguments unless you quote them:

| Form | Behavior |
|------|----------|
| `"path with spaces"` | Keeps spaces. Inside, `\\` and `\"` are escapes. |
| `'path with spaces'` | Keeps spaces. Contents are literal (no escapes). |
| `one\ two` | Outside quotes, a backslash escapes the next character. |

Examples:

```text
cat "/home/monolith/my file.txt"
cp -r "src dir" "dst dir"
echo 'hello world'
```

Unterminated quotes print `parse error: ...` and do not run the command.

## Current Limitations

- No pipes, redirection, or job control.
- No script execution or custom language integration yet.
- File modification times can be updated by Terminal `touch`, but Monolith does not display timestamps in its apps.
- The prompt is a single line; clipboard line breaks become spaces rather than separate command lines.
- Scrollback rows remain unwrapped; the horizontal pan position is shared across the rows currently visible.
- Esc clears the current input and resets the insertion point, so typing can continue immediately.

## Developer Notes

Main implementation files:

- `src/app/TerminalApp.hpp`
- `src/app/TerminalApp.cpp`
- `src/app/TerminalLexer.*` - command-line quoting, completion context, and argv split (headless-testable)
- `src/fs/Filesystem.*` - shared path + recursive copy/remove used by `cp` / `rm`
- Shell `open` / `edit` go through `IWindowController::openPath` / `openInTextEditor`

Launched via `WindowManager::launchTerminal()`.

Scrollback caps: 2,000 rows, 8 MiB total, and 64 KiB per row. Command history caps: 500 entries, 2 MiB total, and 64 KiB per entry. `cat` emits at most 5,000 file lines before the scrollback caps apply.
