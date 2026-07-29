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
| Backspace | Delete character before cursor |
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
| `ls [path]` | List directory contents (`▶` = directory, `•` = file) |
| `pwd` | Print working directory |
| `cd [dir]` | Change directory (no arg → `/home/monolith`) |
| `mkdir <dir>` | Create directory |
| `touch <file>` | Create empty file |
| `cat <file>` | Show file contents |
| `edit <file>` | Open a text file in the Text Editor |
| `open <path>` | Open a file (`.modr` → Drawing, else Text Editor) |
| `cp [-r] <src> <dst>` | Copy file or directory tree (`Filesystem::copyRecursive`; refuses copy into self) |
| `mv <src> <dst>` | Move or rename (destination directory supported) |
| `rm [-r] <path>` | Remove file or directory tree (`Filesystem::removeRecursive` with `-r`; cannot remove `/`) |
| `history` | Show command history |
| `help` | Show command list |
| `exit` / `quit` | Close this terminal window |

Paths may be absolute or relative to the current working directory. Tab completion works for both command names and paths.

`cat` prints one scrollback line per file line (truncated after many lines so huge files cannot flood the terminal).

## Command History

Command history persists across sessions in:

```text
/home/monolith/.terminal_history
```

History is saved after each submitted command. Command history is capped (oldest entries drop); on-screen scrollback is also capped so long sessions stay responsive.

## Current Limitations

- No quoting support — filenames with spaces cannot be passed as single arguments.
- No pipes, redirection, or job control.
- No script execution or custom language integration yet.
- `touch` overwrites existing files with empty content (not Unix “update mtime only” semantics).
- UTF-8 input in the prompt line is still limited compared to the Text Editor.

## Developer Notes

Main implementation files:

- `src/app/TerminalApp.hpp`
- `src/app/TerminalApp.cpp`
- `src/fs/Filesystem.*` — shared path + recursive copy/remove used by `cp` / `rm`

Launched via `WindowManager::launchTerminal()`.

Scrollback cap: 2000 lines. Command history cap: 500 entries. `cat` truncates after 5000 lines.