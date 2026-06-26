# Terminal App

The Terminal is Monolith's command-line interface. It provides scrollback output, an editable input line, command history, tab completion, and a set of built-in filesystem commands against the [internal filesystem](../filesystem.md).

## Launching

Open **Terminal** from the Start menu. Multiple instances are supported with automatic numbering:

- `Terminal`
- `Terminal 2`
- `Terminal 3`

Closing a lower-numbered window renumbers survivors (e.g. closing `Terminal` promotes `Terminal 2` to `Terminal`).

## Input Line

The prompt shows the current working directory, for example:

```text
/home/monolith>
```

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
| `cp [-r] <src> <dst>` | Copy file or directory tree |
| `mv <src> <dst>` | Move or rename (destination directory supported) |
| `rm [-r] <path>` | Remove file or directory |
| `history` | Show command history |
| `help` | Show command list |
| `exit` / `quit` | Close this terminal window |

Paths may be absolute or relative to the current working directory. Tab completion works for both command names and paths.

## Command History

Command history persists across sessions in:

```text
/home/monolith/.terminal_history
```

History is saved after each submitted command.

## Current Limitations

- No quoting support — filenames with spaces cannot be passed as single arguments.
- No pipes, redirection, or job control.
- No script execution or custom language integration yet.
- `cp`/`rm -r` recursive operations are Terminal-only (not exposed in the graphical browser).
- UTF-8 input is not fully handled in the input line.

## Developer Notes

Main implementation files:

- `src/app/TerminalApp.hpp`
- `src/app/TerminalApp.cpp`

Launched via `WindowManager::launchTerminal()`.