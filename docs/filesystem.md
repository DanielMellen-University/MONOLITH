# Internal Filesystem

Monolith has its own hierarchical filesystem that lives inside the application. Apps use **virtual paths** (e.g. `/home/monolith/notes.txt`); the runtime maps these to real directories on the host machine for persistence.

## Virtual Paths

All filesystem operations use paths starting with `/`. Common locations:

| Virtual path | Purpose |
|--------------|---------|
| `/` | Root |
| `/home/monolith` | Default user home (Terminal cwd, browser start path) |
| `/home/monolith/drawings/` | Default location for Drawing `.modr` files |
| `/home/monolith/.terminal_history` | Persistent Terminal command history |

Paths are normalized by `Filesystem::normalize()` — `..`, `.`, duplicate slashes, and relative segments are resolved consistently across Terminal, Filesystem Browser, and Drawing.

## Host Persistence

Virtual paths map under a host directory, typically:

```text
~/.monolith/fs/
```

For example, `/home/monolith/welcome.txt` is stored at:

```text
~/.monolith/fs/home/monolith/welcome.txt
```

The host root is created on startup if it does not exist. The Settings app displays the actual host path.

## API Overview

The `monolith::fs::Filesystem` class provides:

- `exists`, `isFile`, `isDirectory`
- `createDirectory`, `remove`, `rename`
- `readFile`, `writeFile`
- `list`, `listEntries` (typed entries for the graphical browser)

Implementation: `src/fs/Filesystem.hpp`, `src/fs/Filesystem.cpp`.

## Apps That Use the Filesystem

| App | Usage |
|-----|--------|
| Terminal | Full CLI access: `ls`, `cd`, `cat`, `mkdir`, `touch`, `cp`, `mv`, `rm`, tab completion |
| Text Editor | Load and save text files by virtual path |
| Filesystem Browser | Graphical navigation and file management |
| Drawing | Save/load `.modr` raster sketches |
| Settings | Displays host root path |

## Path Rules (Shared Behavior)

- **Absolute paths** start with `/`.
- **Relative paths** resolve against the Terminal's current working directory (other apps use explicit paths or current directory context).
- **Directory destinations** for `mv` and `cp` place the source basename inside the target directory (same semantics as common Unix tools).
- **Recursive operations** (`cp -r`, `rm -r`) are available in the Terminal only.

## Current Limitations

- No permissions, ownership, symlinks, or metadata.
- No quotas or versioning.
- No cross-app file locking (two editors can theoretically race on the same file).
- Filenames with spaces are awkward in the Terminal (no quoting).

## Developer Notes

When adding a new app that reads or writes files:

1. Accept virtual paths, not host paths.
2. Use `Filesystem::normalize()` before operations.
3. Document default paths and formats in `docs/apps/<app>.md`.
4. Update this file only if the shared API or path conventions change.