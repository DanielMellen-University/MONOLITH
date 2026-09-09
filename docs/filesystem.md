# Internal Filesystem

Monolith has its own hierarchical filesystem that lives inside the application. Apps use **virtual paths** (e.g. `/home/monolith/notes.txt`); the runtime maps these to real directories on the host machine for persistence.

## Virtual Paths

All filesystem operations use paths starting with `/`. Common locations:

| Virtual path | Purpose |
|--------------|---------|
| `/` | Root |
| `/home/monolith` | Default user home (Terminal cwd, browser start path) |
| `/home/monolith/documents/` | Seeded documents folder (created on first run) |
| `/home/monolith/drawings/` | Default location for Drawing `.modr` files |
| `/home/monolith/welcome.txt` | Sample text file created on first run |
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

Related host files (not inside the virtual tree):

| Host path | Purpose |
|-----------|---------|
| `~/.monolith/desktop_settings.txt` | Desktop background color + wallpaper path |
| `~/.monolith/session.txt` | Open windows for session restore |
| `~/.monolith/snake_highscore.txt` | Snake high score (games host file) |
| `~/.monolith/minesweeper_best.txt` | Minesweeper best times (game host file) |

## API Overview

The `monolith::fs::Filesystem` class provides:

- `exists`, `isFile`, `isDirectory`
- `createDirectory`, `remove`, `removeRecursive`, `rename`, `renameEntry`
- `readFile`, `writeFile`, `fileSize`
- `copyRecursive` (file or directory tree; blocks copy into self/descendant)
- `copyItemsInto` (multi-source paste into a directory, via `copyRecursive`)
- `moveItemsInto` (multi-source cut/paste into a directory, via non-overwriting rename)
- `list`, `listEntries` (typed entries for the graphical browser)
- `filterEntries` / `entryNameMatches` (case-insensitive name search)
- `isValidEntryName` (rejects empty, `.`, `..`, and names containing `/`)
- Path helpers: `normalize`, `join`, `baseName`, `isSameOrDescendant`, `toHostPath`, `hostRoot`

Implementation: `src/fs/Filesystem.hpp`, `src/fs/Filesystem.cpp`.

### Recursive operations

| Method | Behavior |
|--------|----------|
| `removeRecursive(path)` | Deletes a file or whole directory tree (children first). Refuses virtual root `/`. |
| `copyRecursive(src, dst)` | Copies a file or tree; creates destination directories as needed. Fails if `dst` is the same as or under `src`. |
| `copyItemsInto(srcs, destDir)` | Copies each source into `destDir` under its basename (uses `copyRecursive`). Skips existing names, self-copy, and invalid names. Returns the count copied. |
| `moveItemsInto(srcs, destDir)` | Moves each source into `destDir` under its basename. Uses non-overwriting rename, so existing destination names leave their original sources untouched. Returns the count moved. |
| `renameEntry(dir, old, new)` | Renames one entry in `dir`. Rejects names that fail `isValidEntryName` (including `/`). |
| `filterEntries(entries, query)` | Case-insensitive substring filter on entry names. Empty query returns all. |
| `isSameOrDescendant(a, p)` | True when `p` is `a` or a path under `a` (after normalize). |

Terminal (`cp -r` / `rm -r`) and the Filesystem Browser (delete, cut/paste) both call these shared methods — apps should not reimplement recursive walk logic.

## Apps That Use the Filesystem

| App | Usage |
|-----|--------|
| Terminal | Full CLI access: `ls`, `cd`, `cat`, `mkdir`, `touch`, `cp`, `mv`, `rm`, tab completion |
| Text Editor | Load and save text files by virtual path |
| Filesystem Browser | Graphical navigation and file management (including recursive delete/copy) |
| Drawing | Save/load `.modr` raster sketches |
| Settings | Displays host root path |

## Path Rules (Shared Behavior)

- **Absolute paths** start with `/`.
- **Relative paths** resolve against the Terminal's current working directory (other apps use explicit paths or current directory context).
- **Directory destinations** for `mv` and `cp` place the source basename inside the target directory (same semantics as common Unix tools).
- **Recursive operations** use `Filesystem::copyRecursive` / `removeRecursive` from both Terminal and Filesystem Browser.

## Current Limitations

- No permissions, ownership, symlinks, or metadata.
- Symlinks under the host root are not specially jailed beyond virtual-path normalization.
- No quotas or versioning; `readFile` has no size cap (apps should refuse huge files if needed).
- No cross-app file locking (two editors can theoretically race on the same file).
- Empty files are valid and read as an empty string; callers that need to distinguish an empty file from an I/O failure should check `exists` or `fileSize` first.
- Filenames with spaces are awkward in the Terminal (no quoting).
- `readFile` returns an empty string for both empty files and some I/O failures.

## Developer Notes

When adding a new app that reads or writes files:

1. Accept virtual paths, not host paths.
2. Use `Filesystem::normalize()` (or `join`) before operations.
3. Prefer `copyRecursive` / `removeRecursive` over hand-rolled walks.
4. Document default paths and formats in `docs/apps/<app>.md`.
5. Update this file only if the shared API or path conventions change.
