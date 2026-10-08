# Terminal

A command line for the [internal filesystem](../filesystem.md), with scrollback, history, reverse search and Tab completion.

## Using it

Open **Terminal** from the Start menu, the desktop icon or the taskbar. You can open several; they are titled `Terminal`, `Terminal 2` and so on.

The prompt shows the current folder relative to home (`/home/monolith`):

```text
~>
~/documents>
```

Type a command and press Enter. Run `help` to list commands.

## Commands

| Command | What it does |
|---------|--------------|
| `echo <text>` | Print text |
| `clear` | Clear the screen |
| `help` | List commands |
| `date` | Show the date and time |
| `whoami` | Print `monolith` |
| `version`, `ver` | Print `Monolith Terminal v1.0` |
| `ls [path]` | List a folder (`▶` folder, `•` file, `(empty)` when empty). A file path prints that file name. |
| `pwd` | Print the current folder |
| `cd [dir]` | Change folder. With no argument, go to `/home/monolith`. |
| `mkdir <dir>` | Create a folder |
| `touch <file>` | Create an empty file, or update an existing file's modification time |
| `cat <file>` | Print a file |
| `edit <file>` | Open a file in Text Editor |
| `open <path>` | Open `.modr` files in Drawing and anything else in Text Editor |
| `cp [-r] <src> <dst>` | Copy a file, or a folder with `-r` |
| `mv <src> <dst>` | Move or rename. Never overwrites an existing entry. |
| `rm [-r] <path>` | Remove a file, or a folder with `-r`. `/` cannot be removed. |
| `history` | Show command history |
| `exit`, `quit` | Close this window |

`cp` and `rm` also accept `-rf`, which behaves the same as `-r`. Commands reject extra operands before doing anything, so `mv a b c` prints an error and changes nothing.

## Paths and quoting

- Paths can be absolute or relative to the current folder.
- `~` and `~/` mean `/home/monolith`, also inside quotes. `~name` is not expanded.
- Unquoted spaces separate arguments. To keep spaces:

| Form | Rule |
|------|------|
| `"my file.txt"` | Inside double quotes, `\\` and `\"` are escapes; other backslashes are literal |
| `'my file.txt'` | Inside single quotes, everything is literal |
| `my\ file.txt` | Outside quotes, a backslash escapes the next character |

An unterminated quote prints `parse error: ...` and nothing runs.

## Keyboard shortcuts

| Key | Action |
|-----|--------|
| Enter | Run the command |
| Esc | Clear the input line |
| Up / Down | Previous / next command in history |
| Left / Right, Home / End | Move the cursor |
| Backspace / Delete | Delete the previous / next character |
| Shift + Left / Right, Shift + Home / End | Extend the selection |
| Ctrl+A | Select the whole input line |
| Ctrl+C / Ctrl+X / Ctrl+V | Copy / cut / paste |
| Mouse drag in the input bar | Select text |
| Tab | Complete a command name or path |
| Ctrl+R | Start reverse history search; press again for older matches |
| Page Up / Page Down, mouse wheel | Scroll output (3 rows per key press) |
| Shift+Page Up / Shift+Page Down | Pan long output rows right / left |

In reverse search, typing edits the search query, Enter accepts the match, Esc cancels and restores the original input, and Up or Down cancels the search and returns to normal history browsing. Editing the query jumps to the newest match for the full query.

## Details

- **Completion** works for commands and paths, including `/`, `~/`, paths in double or single quotes, names with apostrophes and names with escaped spaces. A unique file inside an open quote gets its closing quote. A folder gets a trailing `/` so you can press Tab again. Unquoted completions escape spaces, backslashes and quotes.
- **Paste** turns tabs and line breaks into spaces and drops control characters, so a multi-line paste becomes one command instead of several.
- **History** is saved after every command to `/home/monolith/.terminal_history`. If saving fails, the command still runs and the Terminal says so; it retries on the next command.
- **Other windows follow changes.** After `mv`, open Text Editor and Drawing windows on the moved file switch to the new path, and a Terminal or Filesystem window inside a moved folder follows it. After `rm`, editors keep their unsaved content but become untitled, and a Terminal inside the removed folder moves to the nearest parent. `mkdir`, `touch` and `cp` refresh open Filesystem windows.
- **Symlinks.** `rm` and `mv` act on a symlink itself, never its target. See [filesystem.md](../filesystem.md#symlinks).

## Limits

- No pipes, redirection, job control or scripts.
- One input line; long commands scroll horizontally.
- Output rows do not wrap. Horizontal panning applies to all visible rows together.
- Scrollback keeps 2,000 rows and 8 MiB. History keeps 500 commands and 2 MiB. `cat` prints at most 5,000 lines. Full list in [limits.md](../internals/limits.md#terminal).
- MONOLITH shows no timestamps, although `touch` updates them.

## Developer notes

- `src/app/TerminalApp.{hpp,cpp}`: commands, input, scrollback, history.
- `src/app/TerminalLexer.{hpp,cpp}`: quoting, completion context and argument splitting. Tested headless by `test_terminal_lexer`.
- `cp -r` and `rm -r` call `Filesystem::copyRecursive` and `removeRecursive`.
- `open` and `edit` call `IWindowController::openPath` and `openInTextEditor`.
- `cat` and history loading read in 16 KiB chunks and accept CRLF and lone CR line endings, including a CRLF split across chunks.
- Only the visible part of each scrollback row is turned into a texture. See [rendering.md](../internals/rendering.md#terminal).
- Tests: `test_terminal_lexer` (quoting and completion) and `test_terminal_filesystem_state` (commands, history, scrolling, editing). See [testing.md](../development/testing.md).
