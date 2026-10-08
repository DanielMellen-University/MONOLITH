# Atomic writes

Every file MONOLITH writes, whether inside the internal filesystem or on the host (`session.txt`, `desktop_settings.txt`, game scores), goes through one atomic writer. A failed or interrupted write leaves the previous file exactly as it was.

Code: `src/detail/AtomicFile.{hpp,cpp}` (workspace, cleanup, publication) and `src/detail/AtomicTempOutput.cpp` (the buffered output stream). `Filesystem::writeFile`, `writeFileWithProducer` and `writeFileWithProducerIfStampMatches` are built on it.

## How a write works

1. **Reserve a workspace.** A hidden directory is created next to the target with a 128-bit random name and owner-only permissions. The writer publishes an ownership token (a symlink) in one step and holds an OS lock on a lease file for as long as the save runs. A short exclusive lock on the destination directory covers this setup.
2. **Write the content.** Data streams into a `content` file in the workspace through a buffered, seekable stream on a file descriptor. Callers can stream generated data (`writeFileWithProducer`) instead of building the whole file in memory. A producer that returns `false` aborts the save.
3. **Restore permissions.** If the target exists, its permission bits are applied to the staged file after it is fully written and closed, so special bits survive. A new file gets the process's normal creation mode.
4. **Sync.** The staged file is pinned with a no-follow descriptor, permission changes are made through that descriptor, a writable no-follow open must match the same device and inode, and then the file is `fsync`ed. A sync failure aborts the save.
5. **Publish.** The file is renamed over the target with `renameat`, relative to the directory descriptor opened in step 1. If the target is expected to be absent, `renameat2` with `RENAME_NOREPLACE` is used instead, so a file created by someone else in the meantime is kept and reported as a conflict.
6. **Sync the directory.** The writer then tries to `fsync` the directory. A failure here cannot undo the rename, so the save still counts as done; durability of the directory entry is best-effort on filesystems that reject directory sync.

Other rules:

- An in-root symlink to a file is written through to its target instead of being replaced.
- A neighbor such as `notes.txt.tmp` is ordinary user data. The writer never uses or removes it.
- Every step works through descriptors kept from step 1. Retargeting a parent symlink during the save cannot redirect staging, sync, rename or rollback.
- If a restrictive umask creates a mode-000 workspace, owner access is restored through an `O_PATH` descriptor. The lease file is set to owner read and write so an abandoned workspace stays reclaimable.

## Conditional writes

`writeFileWithProducerIfStampMatches` is used when Text Editor and Drawing save to a file they already have open.

- A *stamp* is the file's device, inode, size, and nanosecond modification and change times (`Filesystem::fileStamp`).
- The stamp is checked before staging and again right before the rename. The final check confirms that the parent path still names the pinned directory and reads the target relative to that descriptor.
- Results: `Written`, `Conflict` (the file changed, or appeared when it was expected to be absent) or `Failed`.
- The final check and the rename run under a short advisory lock on the destination directory, so two MONOLITH writers cannot slip a replacement between each other's check and rename.

Before saving, the apps also compare stamps with the version they last loaded or saved. An unchanged stamp needs no extra read. A changed stamp triggers an exact streamed comparison (Text Editor compares normalized lines, Drawing compares the serialized `.modr` bytes in chunks), and if the content differs the app asks for confirmation with Ctrl+D.

## The host-writer race

The advisory lock only works between MONOLITH writers. A host program that does not take the lock (a text editor, `cp`, a script) can still replace an existing target in the short window between MONOLITH's final stamp check and its rename. In that case MONOLITH's save wins and the other program's change is lost.

The window is kept small: the destination name is prepared before the final check, leaving only a single-component check and the rename syscall. For targets expected to be absent, `RENAME_NOREPLACE` closes the race completely.

Closing it fully would need a locking protocol that ordinary host tools do not follow, so it stays a documented limitation.

## Workspace cleanup

A crash can leave a workspace behind. Cleanup reclaims it without ever touching user files.

**What may be removed.** A workspace is reclaimed only if:

- it has a recognized workspace name (versions v2 to v4),
- it is marked as MONOLITH-owned (a valid ownership token for v4; a ready marker for completed v2 to v4 workspaces),
- its lease lock is free, and
- it contains only the expected `owner`, `lease`, `ready` and `content` entries, each of the expected type.

One exception: a v4 workspace that crashed before its token was written may be removed if its name has the exact random-token format and the directory is still empty. Removal uses empty-directory semantics, so anything added in the meantime blocks it.

Anything unexpected keeps the whole workspace. Unmarked incomplete workspaces from older versions are left alone. Symlink ownership tokens are only accepted in directories with the strict random name; regular-file markers from earlier v4 writers are still accepted. Marker probes skip non-regular, non-symlink entries without blocking, so a FIFO with a matching name cannot stall cleanup. Lease entries must be regular files. Older lease files that are only owner-readable or only owner-writable can still be locked.

**When it runs.**

- *Per directory.* The first save or listing in a directory, and every 32nd save or listing after that, starts a sweep. Each operation examines at most 32 entries and saves its position, so a large folder never turns one save into a full scan. The parent lock is held only during each slice. Sweeps read names straight from the directory stream and only build paths for workspace candidates. A scheduled save sweep keeps its exclusive parent lock for the save's own setup.
- *Tracking.* The last 16 physical directories are tracked, each with up to three alternate spellings (for example through symlinks), so aliases share one schedule without repeated path resolution. A retargeted symlink is detected and gets its own first-touch sweep. A directory evicted from tracking is swept again when next used. If locking or listing fails, the next operation retries.
- *At startup.* After launch the window manager gives the filesystem a budget of 32 host entries per frame to walk the whole tree. Directories nobody visits are still cleaned eventually, without a blocking scan at launch. The walk does not follow symlinks and skips unreadable directories. If it runs out of file descriptors, that directory is retried after open handles are released. A candidate whose parent lock is held by another MONOLITH instance is retried on a later frame instead of stalling the UI.

**How removal is done.** Candidates are opened relative to the locked parent with `O_NOFOLLOW`, and entries are validated and removed through that pinned descriptor. A failed save rolls back through its original parent descriptor, so a retargeted alias cannot redirect deletion into another directory with the same name.

Startup and per-directory sweeps share the name classifier and the lock-checked reclamation code; only their scheduling differs.

## Tests

- `scripts/test_atomic_file.cpp`: sweep cadence, setup locking, recovery of incomplete and marked workspaces, active lease skipping, preservation of unknown and legacy workspaces.
- `scripts/test_fs_roadmap.cpp`: the same rules through the `Filesystem` API, plus startup maintenance one entry at a time, lock contention, descriptor exhaustion and unreadable directories.
