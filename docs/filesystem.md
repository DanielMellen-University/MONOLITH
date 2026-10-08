# Filesystem

MONOLITH has its own directory tree. Apps work with **virtual paths** such as `/home/monolith/notes.txt`, and the `Filesystem` class stores them as ordinary files on the host.

Code: `src/fs/Filesystem.hpp`, `src/fs/Filesystem.cpp`.

## Where files live

Virtual paths map under `~/.monolith/fs/`. For example:

```text
/home/monolith/welcome.txt  ->  ~/.monolith/fs/home/monolith/welcome.txt
```

The root is created on startup if it is missing. Startup fails to open the filesystem if that path exists but is not a directory. The Settings app shows the actual host path.

## Standard locations

| Virtual path | Purpose |
|--------------|---------|
| `/home/monolith` | Home. Terminal starts here, and so does the Filesystem app. |
| `/home/monolith/documents/` | Created on first run |
| `/home/monolith/drawings/` | Default folder for Drawing files |
| `/home/monolith/welcome.txt` | Created on first run |
| `/home/monolith/.terminal_history` | Terminal command history |
| `/Wallpapers/` | Sample wallpaper images (see [wallpaper.md](internals/wallpaper.md#sample-wallpapers)) |

## Path rules

- Paths start with `/`. `Filesystem::normalize()` resolves `.`, `..`, repeated slashes and relative segments the same way for every app.
- Relative paths resolve against the Terminal's current directory. Other apps use full paths.
- In Terminal, Text Editor Open and Save As, Drawing Open and Save, and the Settings wallpaper field, a leading `~` or `~/` means `/home/monolith`. Completion keeps the `~` visible; the app passes the full path to the filesystem. `~name` is not expanded, and `normalize()` itself does not expand `~`.
- `cp` and `mv` with a directory as the destination put the source inside it under its own name, as Unix tools do.
- File names cannot be empty, `.`, `..`, or contain `/`.

## How writes work

Every write is atomic: the data goes to a hidden temporary workspace next to the target and is renamed into place only after it is complete and synced. A failed write leaves the old file untouched. The same writer is used for host files such as `session.txt`. Details, including cleanup of workspaces left by a crash, are in [atomic-writes.md](internals/atomic-writes.md).

## Symlinks

The host tree can contain symlinks. The rules:

- A symlink that resolves outside `~/.monolith/fs/` is hidden from listings, and paths through it are rejected (`toHostPath()` returns an empty string).
- A symlink that stays inside the root is listed. A dangling in-root symlink is listed too, and `exists()` reports it, so it can be removed and it blocks a rename onto its name.
- `remove`, `removeRecursive`, `rm` and `mv` act on a final symlink itself, never its target, even when the target is outside the root. The parent path is validated first, so a path that passes through an outside-pointing directory link is still rejected.
- Recursive copy refuses in-root symlink sources instead of following them, skips links that point outside, and refuses a destination that resolves into the source tree.
- Recursive remove deletes a symlink entry and never walks through it.

## API

`monolith::fs::Filesystem` provides:

| Group | Methods |
|-------|---------|
| Queries | `exists`, `isFile`, `isDirectory`, `fileSize`, `fileStamp` |
| Listing | `list`, `listEntries` (directories first, case-insensitive name order), `filterEntries`, `entryNameMatches` |
| Reading | `readFile`, `readFileChunks`, `readFileTailChunks` |
| Writing | `writeFile`, `writeFileWithProducer`, `writeFileWithProducerIfStampMatches`, `updateModifiedTime` |
| Changing the tree | `createDirectory`, `remove`, `removeRecursive`, `rename`, `renameEntry`, `copyRecursive`, `copyItemsInto`, `moveItemsInto` |
| Paths | `normalize`, `join`, `baseName`, `isSameOrDescendant`, `isValidEntryName`, `toHostPath`, `hostRoot` |

Notes on reading:

- `readFile(path)` loads the whole file and returns an empty string for both an empty file and some errors. Use `readFile(path, out)` when you need to tell them apart; it returns success separately.
- `readFile` has no size cap. Apps that may meet large files use the chunked readers and enforce their own limits.
- `readFileChunks` passes at most 16 KiB at a time as a `string_view` that is valid only during the callback. Returning `false` from the callback stops early and still counts as success.
- `readFileTailChunks` seeks to the last N bytes first and reports whether it skipped a prefix. The first chunk may start mid-record. Terminal uses it to load history.

Notes on writing:

- `writeFileWithProducer` streams generated content; the producer returns `false` to abort and keep the old file.
- `writeFileWithProducerIfStampMatches` only replaces the file if it still matches an expected stamp (device, inode, size, change time). It returns `Written`, `Conflict` or `Failed`.
- `updateModifiedTime` changes only the last-write time. Terminal `touch` uses it.

### Tree operations

| Method | Behavior |
|--------|----------|
| `remove(path)` | Removes one file or empty directory. Refuses `/`. |
| `removeRecursive(path)` | Removes a file or a whole tree. Refuses `/`. |
| `copyRecursive(src, dst)` | Copies a file or tree, creating missing parent folders. Streams files in 16 KiB chunks into atomic writes. Fails if the destination is the source or inside it. If a later file fails, everything the copy changed is rolled back (see below). |
| `copyItemsInto(srcs, dir)` | Copies each source into `dir` under its own name. Skips existing names, self-copies and invalid names. Returns the number copied. |
| `moveItemsInto(srcs, dir)` | Moves each source into `dir` without overwriting. A name that already exists leaves that source where it was. Returns the number moved. |
| `rename(old, new)` | Moves one entry without overwriting, even if another process creates the destination at the same moment (atomic no-replace rename). Refuses `/` and moving a folder into itself. |
| `renameEntry(dir, old, new)` | Renames inside one folder; rejects invalid names. |
| `isSameOrDescendant(a, p)` | True when `p` is `a` or inside it, after normalizing. |

Rollback for `copyRecursive`: before overwriting a file, the copy keeps the original as a hard link on the same volume (or a bounded copy if links are not supported). On failure it restores overwritten files, removes new entries and resets directory modification times. A rollback error is printed to stderr and the backup file is kept for recovery.

Terminal (`cp -r`, `rm -r`) and the Filesystem app (delete, copy, cut, paste) both use these methods. Apps should not write their own tree walks.

## Who uses it

| App | Use |
|-----|-----|
| Terminal | All commands: `ls`, `cd`, `cat`, `mkdir`, `touch`, `cp`, `mv`, `rm`, completion |
| Text Editor | Open and save text files |
| Filesystem | Browsing and file management |
| Drawing | Open and save `.modr` files |
| Settings | Wallpaper path completion and the host root display |

## Limitations

- No permissions, owners or metadata. Timestamps are not shown anywhere, although `touch` can update them.
- No quotas and no versioning.
- No host-wide lock. Text Editor and Drawing detect outside changes to an open file before saving, and MONOLITH writers serialize with each other, but a host program can still race a save. See [the host-writer race](internals/atomic-writes.md#the-host-writer-race).
- Moves need a host filesystem with atomic no-replace rename. On one without it, moves fail rather than risk overwriting.

## Adding an app that uses files

1. Accept virtual paths, never host paths.
2. Normalize with `Filesystem::normalize()` or `join()` before use.
3. Use `copyRecursive` and `removeRecursive` instead of your own walks.
4. After a change, call the matching `notifyVirtualPath...` on your controller so other windows refresh.
5. Document default paths and formats in your app's page under `docs/apps/`.
