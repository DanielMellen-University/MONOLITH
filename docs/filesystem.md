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

Paths are normalized by `Filesystem::normalize()` — `..`, `.`, duplicate slashes, and relative segments are resolved consistently across Terminal, Filesystem Browser, and Drawing. Normalization and `join()` scan components directly into the canonical output instead of allocating a stream, a combined path, or separate strings for every component.

User-facing path entry in Terminal, Text Editor Open/Save As, Drawing Open/Save, and Settings wallpaper controls accepts a leading `~` or `~/` as `/home/monolith`. Completion searches the resolved directory and keeps the shorthand visible; accepted paths are passed to the Filesystem as canonical absolute paths. `~name` is not expanded. This is an app-level input rule; `Filesystem::normalize()` itself does not expand tildes.

## Host Persistence

Virtual paths map under a host directory, typically:

```text
~/.monolith/fs/
```

For example, `/home/monolith/welcome.txt` is stored at:

```text
~/.monolith/fs/home/monolith/welcome.txt
```

The host root is created on startup if it does not exist. Startup rejects a host path that exists but is not a directory. The Settings app displays the actual host path.

Atomic-save workspaces are created with owner-only permissions at the initial `mkdir`, closing the setup window before permission normalization. Normalization remains in place in case the host umask removed owner access. The active lease file is also normalized to owner read/write so abandoned workspaces remain reclaimable under restrictive umasks. Cleanup can also lock older lease files that are only owner-readable or only owner-writable by opening them with the available access mode.

Regular file writes stage into a uniquely reserved, hidden sibling workspace and atomically replace the destination only after the complete byte stream succeeds. Existing permission bits are restored after the staged stream is fully written and closed, preserving special bits that the writes themselves could clear. An in-root file symlink is updated through its target instead of being replaced. A neighboring file such as `notes.txt.tmp` is ordinary user data and remains untouched. New v4 workspaces use a 128-bit OS-random name, publish an ownership token as a symlink in one filesystem operation, hold an OS file lock while a save is active, and use a brief exclusive lock on the destination directory while the workspace is initialized. Symlink owner tokens are accepted only in directories with the strict 128-bit random-name format; validated regular-file markers from earlier v4 writers remain supported. If publishing the ownership token fails, setup rollback removes the workspace only if it is still empty. The first save or directory listing in a destination directory and every 32 combined saves/listings there starts an opportunistic sweep; while a sweep is pending, each later save or listing advances it by one slice. Each operation examines at most 32 directory entries, and a resumable directory cursor advances until the pass completes. Small folders can finish in one operation, while a large folder no longer turns one save/listing into an unbounded second scan. The parent lock is held only during each slice; the process-wide tracker mutex is released before workspace validation and reclamation, and there is no separate whole-directory sweep path. Tracking covers 16 recent physical directories and remembers up to three alternate lexical paths per directory, so repeated symlink aliases share a cadence without repeated resolution; an evicted directory is swept again when next used. Sweeps inspect borrowed entry names and construct paths only for recognized workspace candidates. A scheduled save sweep retains its exclusive destination-directory lock through the following workspace setup, avoiding a second parent open/flock cycle. Workspace and child-entry ownership checks borrow names from their existing path storage instead of allocating repeated filename copies. If the parent lock or directory enumeration fails, the next save or listing retries immediately. Tracked lexical paths, including retry requests and active cursor paths, avoid repeating canonical resolution; a new spelling resolves before it is associated with an existing physical-directory cadence. Marker probes reject non-regular, non-symlink entries without blocking, so a user-owned FIFO lookalike cannot stall cleanup. Sweeps reclaim incomplete v4 workspaces only when their ownership token is valid, except a pre-marker remnant with a valid random-token name may be removed if it is still empty; removal fails closed if any entry has appeared. Marked v4/v3/v2 workspaces are reclaimed only when their lease lock is free, and lease entries must be regular files before they are opened. Cleanup removes only expected `owner`, `lease`, `ready`, and `content` entries of the expected types; any unexpected child preserves the workspace and all of its contents. Active workspaces are skipped, and unmarked incomplete workspaces from older versions remain untouched.

Cached symlink aliases are checked against their tracked directory identity outside the tracker mutex. If a link is retargeted, that spelling is detached from the old directory and the new target receives its own first-touch cleanup sweep.

Startup maintenance and per-directory sweeps share the same v2-v4 workspace-name classifier and lock-checked workspace-reclamation dispatcher while retaining their separate bounded traversal and scheduling policies.

After startup, the Window Manager gives the filesystem a 32-entry maintenance budget per frame to walk the host tree and check reserved workspace names. Stale saves in directories nobody revisits are therefore eventually reclaimed without a startup-wide blocking scan. The traversal borrows ordinary entry paths from its directory iterator and copies only cleanup candidates or child directories that must outlive the current iterator step. It skips permission-denied directories and does not follow symlinks. If opening the root or a child hits the process or system descriptor limit, that directory is deferred until active traversal frames unwind and release handles, then retried within the same bounded walk; other iterator errors drop only the affected directory frame so readable siblings still get scanned. Ordinary saves and listings continue their local cleanup checks. A workspace is removed only through the same parent-lock, ownership, expected-entry, and free-lease checks used by ordinary directory sweeps.

Related host files (not inside the virtual tree):

| Host path | Purpose |
|-----------|---------|
| `~/.monolith/desktop_settings.txt` | Desktop background color, wallpaper path, clock format, and interface text scale |
| `~/.monolith/session.txt` | Open windows for session restore |
| `~/.monolith/snake_highscore.txt` | Snake high score (games host file) |
| `~/.monolith/minesweeper_best.txt` | Minesweeper best times (game host file) |

All Monolith text snapshots, including game records, use the same unique temporary-workspace replacement path. A failed stream or replacement leaves the previous host record intact and cleans known temporary entries; unexpected workspace entries are preserved. Replacing an existing regular snapshot also retains its permission bits; a new snapshot uses the host process's normal creation mode. A hard process termination releases both locks: a later sweep can reclaim an owned incomplete v4 workspace or a marked v4/v3/v2 workspace with partial content, provided no unexpected entries have been added.

## API Overview

The `monolith::fs::Filesystem` class provides:

- `exists`, `isFile`, `isDirectory`
- `createDirectory`, `remove`, `removeRecursive`, `rename`, `renameEntry`
- `readFile`, `readFileChunks`, `readFileTailChunks`, `writeFile`, `writeFileWithProducer`, `updateModifiedTime` (last-write time only), `fileSize`, `fileStamp` (`readFile(path, out)` reports read success separately from empty content)
- `writeFileWithProducerIfStampMatches` (conditional atomic replacement, with `Written`, `Conflict`, and `Failed` results)
- `copyRecursive` (file or directory tree; blocks copy into self/descendant)
- `copyItemsInto` (multi-source paste into a directory, via `copyRecursive`)
- `moveItemsInto` (multi-source cut/paste into a directory, via non-overwriting rename)
- `list`, `listEntries` (typed entries for the graphical browser; `listEntries` keeps directories first and sorts names case-insensitively)
- `filterEntries` / `entryNameMatches` (case-insensitive name search)
- `isValidEntryName` (rejects empty, `.`, `..`, and names containing `/`)
- Path helpers: `normalize`, `join`, `baseName`, `isSameOrDescendant`, `toHostPath`, `hostRoot`

`toHostPath()` returns an empty string when an existing symlink in the virtual path resolves outside the configured host root. Directory listings share one visibility check, classify ordinary entries from their no-follow status, and resolve the configured root once per listing; symlink targets and directories are checked for containment before they are exposed. In-root dangling symlinks remain visible as entries. `exists()` treats an in-root dangling symlink as an existing directory entry, so it can be removed or protected as a rename destination.

Implementation: `src/fs/Filesystem.hpp`, `src/fs/Filesystem.cpp`.

`readFile()` materializes the entire file. Consumers that can process data incrementally can use `readFileChunks()`, which passes at most 16 KiB at a time as a temporary `string_view`; the Text Editor uses it to stream line parsing and rejects documents above 16 MiB or 65,536 lines. `fileStamp()` returns a regular file's device/inode identity, size, and nanosecond modification/change times for cheap version checks; callers that require content certainty can stream-compare when the stamp changes. `readFileTailChunks()` seeks to the last requested number of bytes before streaming, and reports whether it skipped a prefix; the first chunk may start inside a logical record. Views are valid only during their callback. Returning `false` from the callback stops reading early and counts as success; path, open, read, or callback failures return `false` from either method. `copyRecursive()` streams regular files in 16 KiB chunks into atomic replacement without buffering each complete file. It resolves the root source and destination once, classifies each source entry once, and derives ordinary child paths from validated parents instead of repeating virtual-path and containment checks through the public API. Directory copies walk entries directly, avoiding the sorting and temporary entry vectors used for graphical listings, and use an explicit frame stack instead of consuming one C++ call frame per directory. Destination symlinks are resolved and checked, and copies are rejected if their physical destination is the source itself or a descendant, including aliases introduced by in-root symlinks. Before replacing a destination file, the copy journals its original through a same-volume hard link when possible, falling back to a bounded-memory file copy. If a later child fails, overwritten files, newly created entries, and existing destination-directory modification times are restored. `removeRecursive()` delegates directory-tree removal to `std::filesystem::remove_all`, avoiding a temporary vector of every direct child while retaining non-following symlink removal.

`writeFileWithProducer()` streams generated content into an atomically reserved hidden sibling workspace without requiring one complete output string. The producer returns `false` on generation or stream failure to discard the workspace and preserve the previous destination; `writeFile()` uses this same path for fixed strings. The shared atomic writer accepts either a void callback or a boolean-returning callback; `false` aborts without replacing the target. Before sync, the staged path is pinned as a regular file with a no-follow descriptor. Permission recovery changes that pinned inode directly (with a `/proc/self/fd` fallback on older kernels), then a writable no-follow open must match its device/inode before final permissions are restored and `fsync` runs. This prevents a staged-path symlink swap from redirecting permission changes to another file. After the complete stream and final permissions are set, the staged file is synced before publication; a file-sync failure aborts and preserves the previous destination. After rename, the writer attempts to `fsync` the destination directory. A directory-sync failure cannot safely undo a completed rename, so publication still reports success and crash durability of that directory entry is best-effort on filesystems that reject directory sync. `writeFileWithProducerIfStampMatches()` also checks that the target's device/inode, size, and high-resolution change stamp match the expected version (or that it is still absent) before staging and immediately before replacement. It returns `Conflict` when the target version changed, `Failed` for other write errors, and `Written` on success. Final validation and publication use a short-lived advisory lock on the destination directory, serializing competing Monolith atomic writers so an existing target cannot change between their check and rename. If the expected target is absent, final publication uses Linux `RENAME_NOREPLACE`, so an uncoordinated creator is preserved and reported as a conflict. Host tools and other writers that do not honor the advisory lock can still race with an existing-target replacement. Drawing uses the streaming writer with a bounded 16 KiB `.modr` encoder.

### Recursive operations

| Method | Behavior |
|--------|----------|
| `remove(path)` | Removes one file or empty directory. Symlink entries are unlinked without touching their targets. Refuses virtual root `/`. |
| `removeRecursive(path)` | Deletes a file or whole directory tree (children first); a symlink entry is unlinked without traversing it. Refuses virtual root `/`. |
| `copyRecursive(src, dst)` | Copies a file or tree; creates missing destination parent directories, then rolls back overwritten files, new entries, and existing directory timestamps if a child copy fails. Fails if the resolved destination is the source or under it, including through an in-root symlink. |
| `copyItemsInto(srcs, destDir)` | Copies each source into `destDir` under its basename (uses `copyRecursive`). Skips existing names, self-copy, and invalid names. Returns the count copied. |
| `moveItemsInto(srcs, destDir)` | Moves each source into `destDir` under its basename. Uses non-overwriting rename, so existing destination names leave their original sources untouched. Final symlink entries can move even when their targets are outside the virtual root. Returns the count moved. |
| `rename(old, new)` | Atomically renames or moves one entry without overwriting, including when another process creates the destination concurrently. Existing regular entries and dangling symlinks both block the destination. Final symlink sources move as entries without moving their targets, even if those targets are outside the root. Source and destination parents must resolve inside the root. Rejects the virtual root and destinations that are the source or inside its subtree. |
| `renameEntry(dir, old, new)` | Renames one entry in `dir`. Rejects names that fail `isValidEntryName` (including `/`). |
| `filterEntries(entries, query)` | Case-insensitive substring filter on entry names. Empty query returns all. |
| `isSameOrDescendant(a, p)` | True when `p` is `a` or a path under `a` (after normalize); compares the component boundary without building an `a + "/"` prefix string. |

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

- No permissions, ownership, or metadata browsing layer; timestamps are not displayed, though Terminal `touch` can update a file's last-write time.
- Symlink targets that resolve outside the host root are rejected and omitted from virtual directory listings. Deletion and rename validate the resolved parent before touching an entry, so traversing an outside-pointing directory symlink cannot affect entries in the external tree. Removing or moving a final symlink that is itself inside the root operates on the link without touching its target, including through Terminal `rm` and `mv`. Symlinks that remain inside the root are still host filesystem entries, not a separate metadata layer.
- Recursive copy rejects in-root symlink sources instead of traversing them and omits links resolving outside the root. It also rejects destination aliases into the source tree. Recursive remove deletes a symlink entry itself and never walks through that link into its target tree.
- A failed recursive copy rolls back files and directories changed by that operation; a rollback error is reported to stderr and preserves any remaining backup file for recovery.
- Rename uses the host's atomic no-replace operation, so concurrent moves cannot overwrite a newly created destination. A host filesystem that does not support atomic no-replace rename causes the move to fail rather than fall back to a racy overwrite.
- No quotas or versioning; `readFile` has no size cap (apps should refuse huge files if needed).
- No host-wide file lock or general compare-and-swap protocol. Before overwriting a bound file, Text Editor and Drawing compare a file stamp with the last loaded/saved version; changed stamps trigger a streamed content comparison and require the existing Ctrl+D confirmation when content differs or the file is missing. This catches ordinary host-side edits and writes from another Monolith process while unchanged saves need only a metadata check. Conditional writes serialize final validation and publication against other Monolith atomic writers in the same directory; an absent expected target also uses no-replace rename. Host tools and uncoordinated filesystem mutations do not honor the advisory lock and can still race with an existing-target replacement. Save As remains an explicit target choice.
- Empty files are valid and read as an empty string; callers that need to distinguish an empty file from an I/O failure should use the boolean-output `readFile(path, out)` overload.
- Terminal supports filenames with spaces through quoted or backslash-escaped arguments.
- The one-argument `readFile` overload returns an empty string for both empty files and some I/O failures; use the boolean-output overload when the distinction matters.

## Developer Notes

When adding a new app that reads or writes files:

1. Accept virtual paths, not host paths.
2. Use `Filesystem::normalize()` (or `join`) before operations.
3. Prefer `copyRecursive` / `removeRecursive` over hand-rolled walks.
4. Document default paths and formats in `docs/apps/<app>.md`.
5. Update this file only if the shared API or path conventions change.
