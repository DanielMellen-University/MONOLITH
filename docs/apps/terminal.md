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
| Tab | Complete command name or filesystem path |
| Ctrl+R | Enter reverse history search |
| Ctrl+R (in search) | Find older matching command |
| Enter (in search) | Accept matched command |
| Esc (in search) | Cancel search, restore input |
| Page Up / Page Down | Scroll output history |
| Mouse wheel | Scroll output history |

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
| `touch <file>` | Create empty file |
| `cat <file>` | Show file contents |
| `edit <file>` | Open a text file in the Text Editor |
| `open <path>` | Open via shell routing (case-insensitive `.modr` → Drawing, else Text Editor) |
| `cp [-r] <src> <dst>` | Copy file or directory tree (`Filesystem::copyRecursive`; verifies file reads and refuses copy into self) |
| `mv <src> <dst>` | Move or rename (destination directory supported; open Editor and Drawing bindings follow the move) |
| `rm [-r] <path>` | Remove file or directory tree (`Filesystem::removeRecursive` with `-r`; cannot remove `/`) |
| `history` | Show command history |
| `help` | Show command list |
| `exit` / `quit` | Close this terminal window |

Paths may be absolute or relative to the current working directory. Quoted paths preserve their exact whitespace, including repeated spaces. Tab completion works for both command names and paths, including the virtual root (`/`), paths inside double or single quotes, and paths with backslash-escaped spaces. The input cursor and Backspace move through complete UTF-8 characters, so accented characters and emoji are not split into invalid byte fragments. Long commands scroll horizontally to keep the cursor visible.

Reverse history search has its own editable query. Left/Right/Home/End move through the query, typed text is inserted at the caret, Delete removes the next complete UTF-8 character, and Backspace removes the previous one. Long search queries scroll horizontally to keep the caret visible. Up/Down cancel search and return to normal history navigation. Canceling restores both the original input and its caret position. Accepting or canceling a search also clears any older Up/Down navigation state, so the accepted or restored input is not overwritten by a stale history slot.

Completion replaces only the token text before the cursor. Opening quotes remain in place, and unquoted completions escape spaces, backslashes, and quote characters so the completed command keeps the same meaning when it runs.

`cat` prints one scrollback line per file line (truncated after many lines so huge files cannot flood the terminal). CRLF and lone-CR separators are normalized to LF before output. It reports a read failure separately from a valid empty file.

After a successful `mv`, any open Text Editor or Drawing window bound to the source path follows the normalized destination path. Moving a directory also updates bindings for open files beneath it, and any Terminal or Filesystem Browser currently inside that directory follows the new location.

The shared Filesystem Browser clipboard also follows a moved source, so a pending Copy or Cut can still be pasted after another window renames or moves that source.

After a successful `rm`, open Editor and Drawing windows keep their in-memory work but release deleted paths and become untitled tracked windows. A Terminal whose current directory was inside the removed tree moves to the nearest existing parent. Deleted paths are also removed from the shared cut clipboard.

## Command History

Command history persists across sessions in:

```text
/home/monolith/.terminal_history
```

History is saved after each submitted command. Command history is capped (oldest entries drop); on-screen scrollback is also capped so long sessions stay responsive.

History loading accepts both Unix and Windows line endings, so recalled commands do not carry a hidden carriage return into command parsing.

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
- `touch` creates an empty file if missing; existing files are left unchanged (no mtime update yet).
- The prompt is a single line and does not provide Text Editor-style selection or clipboard editing.
- Tab completion does not add a closing quote automatically when completing inside an open quoted path.
- Scrollback lines stay at native text size and clip at the viewport edge instead of being horizontally scaled.
- Esc clears the current input and resets the insertion point, so typing can continue immediately.

## Developer Notes

Main implementation files:

- `src/app/TerminalApp.hpp`
- `src/app/TerminalApp.cpp`
- `src/app/TerminalLexer.*` - command-line quoting, completion context, and argv split (headless-testable)
- `src/fs/Filesystem.*` - shared path + recursive copy/remove used by `cp` / `rm`
- Shell `open` / `edit` go through `IWindowController::openPath` / `openInTextEditor`

Launched via `WindowManager::launchTerminal()`.

Scrollback cap: 2000 lines. Command history cap: 500 entries. `cat` truncates after 5000 lines.
