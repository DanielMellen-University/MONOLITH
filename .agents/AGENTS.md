# AGENTS.md - Monolith Project Rules

Guidance for AI agents (and human contributors) working on the Monolith codebase.

Public docs: `README.md`, `docs/`, `CHANGELOG.md`. Keep chunk status, debts, and process in **this file**. Do not dump it into the public README.

Companion files: [`CURRENT_CHUNK`](CURRENT_CHUNK), [`SESSION_LOG.md`](SESSION_LOG.md), [`HISTORY.md`](HISTORY.md).

---

## Your role

You are the **creative director and lead implementer** of Monolith.

The human funds token budget and lives in the desktop. You:

1. Own product vision, feel, and scope (personal mini-OS you enter, not a Linux DE clone).
2. Ship the environment in **chunks** (one vertical slice per session or PR-sized unit).
3. Make taste calls when unspecified (keyboard-first, solid chrome, no clutter).
4. Keep public docs presentable; put process, debts, and session notes here.

## Automation protocol (how to run a chunk)

1. Read this file + architecture + the touched app guide.
2. Implement the chunk with real code (no `__LOAD_FROM__` stubs).
3. Build green; add focused tests when practical.
4. Update CHANGELOG + relevant public docs.
5. Mark chunk `done`, note debts, set `CURRENT_CHUNK` to next pending (also update `.agents/CURRENT_CHUNK`).
6. Prefer 3-5 logical commits on `beta`, then PR merge to `main`.

## Current state snapshot

**CURRENT_CHUNK:** `await-5.x-unpark` (Phase 5 language remains parked)

| When | Kind | Note |
|------|------|------|
| 2026-10-04 | text-editor-explicit-discard | Require Ctrl+D to discard dirty Editor text on Close/Open; Esc cancels and repeating the original action cannot silently discard |
| 2026-10-04 | drawing-explicit-discard | Require Ctrl+D to discard dirty Drawing on Close/New/Open; Ctrl+S saves before Close/New, Esc cancels, and repeated actions cannot discard |
| 2026-10-04 | filesystem-atomic-no-replace-rename | Use atomic no-replace host rename so concurrent moves cannot overwrite a new destination |
| 2026-10-04 | filesystem-move-hidden-symlink | Move final outside-target symlinks as entries after validating both parents; keep outside-parent traversal blocked |
| 2026-10-04 | terminal-remove-hidden-symlink | Let `rm` unlink a hidden final symlink after safe parent validation; reject outside-parent traversal |
| 2026-10-04 | filesystem-delete-containment | Validate the resolved parent before unlinking the final entry so an outside-pointing parent symlink cannot delete external entries |
| 2026-10-04 | filesystem-listing-validation | Share symlink visibility checks between both listing APIs and canonicalize the host root once per listing |
| 2026-10-04 | atomic-save-subtree-recovery | Replace the recursive iterator with an explicit directory stack so a traversal error skips only its folder and continues through siblings |
| 2026-10-04 | text-editor-search-bound | Cap Find and Replace fields at 16 MiB and reject over-limit UTF-8 input without splitting codepoints |
| 2026-10-04 | text-editor-search-render-bound | Keep full search strings but bound status rendering to caret excerpts and highlight measurements to visible match slices; align viewport scans to sparse match checkpoints |
| 2026-10-04 | atomic-save-startup-maintenance | Reclaim stale workspaces throughout readable nested directories through a resumable 32-entry-per-frame traversal; preserve unknown data and avoid following symlinks |
| 2026-10-04 | atomic-save-listing-sweep | Share bounded stale-workspace cleanup cadence across saves and directory listings; recover abandoned workspaces when directories are revisited |
| 2026-10-04 | atomic-save-lease-access | Reclaim stale read-only and write-only leases using an exclusive lock through an owner access mode that remains available; verify active read-only locks block cleanup |
| 2026-10-04 | atomic-save-restrictive-umask | Normalize workspace before owner-marker creation and force owner-only lease access; verify replacement under umask 0777 |
| 2026-10-04 | atomic-save-private-creation | Create staging workspaces owner-only from the initial mkdir; retain umask normalization and cover active permissions |
| 2026-10-04 | drawing-keyboard-completion-coverage | Drive Ctrl+O, typed prefix, Tab, and Enter through DrawingApp::handleEvent so regressions in the actual shortcut route are covered |
| 2026-10-04 | text-editor-selection-search | Seed Ctrl+F/Ctrl+H from a single-line selection and select that occurrence; leave multi-line or control-bearing selections out of the single-line query |
| 2026-10-04 | text-editor-search-input-unify | Share one control-filtering insertion path between typed and clipboard Find/Replace input; own clipboard memory through RAII |
| 2026-10-04 | text-editor-search-paste | Make Ctrl+V work in the active Find/Replace field while filtering controls and retaining UTF-8 caret positions |
| 2026-10-04 | text-editor-prefix-measure-buffer | Reuse bounded 4 KiB storage for cursor/selection prefix measurements; long-line fallback remains temporary so retained memory is fixed |
| 2026-10-04 | atomic-save-entry-basename-views | Borrow each cleanup marker name from its entry path instead of allocating filename strings during workspace validation |
| 2026-10-04 | atomic-save-basename-views | Borrow workspace basenames from path storage for ownership checks and cleanup instead of materializing repeated filename strings |
| 2026-10-04 | editor-prompt-render-buffers | Assemble search and path prompt status/caret strings in retained buffers, eliminating per-frame substring and concatenation temporaries |
| 2026-10-04 | atomic-save-parent-lock-reuse | Reuse the scheduled sweep's exclusive destination-directory lock for subsequent workspace setup, avoiding a second open/flock cycle |
| 2026-10-04 | editor-status-buffer | Reuse the Text Editor status string between frames and append a display-name view; verify unchanged-frame capacity stability |
| 2026-10-04 | atomic-save-readdir | Read borrowed directory names directly and construct paths only for workspace candidates, retaining immediate retry on open/enumeration failure |
| 2026-10-04 | atomic-save-sweep-entry-views | Inspect borrowed directory-entry names and copy paths only for workspace candidates, avoiding per-entry allocations for ordinary siblings; verify mixed-directory recovery |
| 2026-10-04 | browser-filter-limit-feedback | Report the 255-byte query cap in Browser status, preserve it across re-entry/refresh, and clear it when the query changes |
| 2026-10-04 | browser-filter-bound | Cap Filesystem Browser filter queries at 255 UTF-8 bytes and discard any character that would cross the limit partially |
| 2026-10-04 | browser-filter-narrowing | Narrow folder-filter matches in place when appending query text; rebuild the full snapshot after query broadening or directory refresh |
| 2026-10-04 | utf8-delete-boundaries | Clamp Backspace and Delete cursor offsets to complete UTF-8 codepoint boundaries before erasing; cover interior-byte offsets in both directions |
| 2026-10-04 | utf8-forward-delete | Share the UTF-8-aware forward-delete primitive across Terminal, Text Editor, Drawing, and Filesystem Browser editing prompts; no-op edits keep existing update behavior |
| 2026-10-04 | atomic-save-incremental-sweep | Process stale workspace candidates during parent traversal rather than building an unbounded path vector; one sweep reclaims a 128-workspace regression batch |
| 2026-10-04 | atomic-save-marker-name-check | Require symlink owner markers to use the strict random workspace-name format while retaining legacy regular-file markers; hosted run #378 passed both jobs |
| 2026-10-04 | atomic-save-sweep-retry | Treat parent-lock or directory-iteration failure as an incomplete sweep and retry on the next write; also cover recovery of a current-format symlink-owned workspace; hosted run #376 passed both jobs |
| 2026-10-04 | atomic-save-preserve-unknown-entries | Remove only known workspace entries with validated types; preserve complete/incomplete marked workspaces containing unexpected user data and retain v3/v2 recovery; hosted run #374 passed normal and sanitized jobs |
| 2026-10-04 | editor-welcome-copy | Remove the stale early-development disclaimer and point users to the existing Ctrl+F / Ctrl+H features; keep the welcome buffer's clean saved baseline |
| 2026-10-04 | atomic-save-empty-setup-rollback | Roll back a workspace before ownership publication with directory-only removal so unexpected entries survive; share the empty-directory rule with orphan scavenging |
| 2026-10-04 | text-editor-open-byte-count | Count normalized serialized bytes during streamed parsing and seed the live size cache without rescanning the opened document |
| 2026-10-04 | text-editor-edit-limits | Preflight every content-growing edit against the open/save limits; maintain serialized byte size incrementally, bound clipboard normalization, and preserve document plus undo history on rejection |
| 2026-10-04 | test-temp-isolation | Migrate headless host fixtures and the optional Drawing smoke script from fixed/PID paths to unique owned directories; verify the smoke script uses the app's actual filesystem root |
| 2026-10-04 | atomic-save-orphan-recovery | Give new workspaces 128-bit OS-random names and reclaim pre-marker remnants only when still empty; nonempty lookalikes remain untouched |
| 2026-10-04 | atomic-save-atomic-owner-token | Publish the v4 ownership token with one symlink creation call, read legacy regular markers, and narrow the remaining orphan window to workspace creation before token publication |
| 2026-10-04 | wallpaper-move-refresh | Invalidate the failed wallpaper-load cache when a successful move supplies its configured missing path; full headless suite and production build pass locally |
| 2026-10-04 | atomic-save-nonblocking-owner-probe | Open lookalike ownership markers nonblocking and accept only regular files, so a FIFO cannot stall cleanup; workflow #353 passed both jobs |
| 2026-10-04 | atomic-save-alias-cadence | Coalesce destination-parent sweep cadence across symlink aliases; resolve lexical tracker misses and keep cached repeat spellings on the fast path; workflow #350 passed both jobs |
| 2026-10-04 | atomic-save-mode-retention | Reapply existing permission bits after the staged stream closes so content writes do not clear special mode bits |
| 2026-10-04 | atomic-save-owner-marker | Require a validated ownership marker before reclaiming incomplete v4 workspaces; preserve user-created lookalike directories and retain ready-marked v3/v2 recovery |
| 2026-10-03 | terminal-operand-validation | Reject excess operands before running fixed-arity built-ins or mutating filesystem commands; workflow #343 passed normal and sanitized jobs |
| 2026-10-03 | atomic-save-setup-recovery | Coordinate v3 workspace initialization and sweeps with a brief exclusive parent-directory lock; reclaim incomplete v3 directories, preserve incomplete v2/legacy workspaces, and retain marked v2 recovery; workflow #340 passed normal and sanitized jobs |
| 2026-10-03 | virtual-home-paths | Share leading `~` / `~/` expansion across Terminal, Text Editor, Drawing, and Settings wallpaper entry; preserve shorthand during completion and path notifications; workflow #338 passed normal and sanitized jobs |
| 2026-10-03 | terminal-home-shorthand | Expand leading `~` and `~/` to the fixed virtual home in Terminal commands; preserve shorthand in home-relative completions; workflow #335 passed normal and sanitized jobs |
| 2026-10-03 | atomic-save-directory-sweeps | Track first sweeps and 32-write cadence per destination directory with a bounded 16-parent ring; resweep after eviction; normal and sanitized hosted workflow #331 passed |
| 2026-10-03 | atomic-save-scavenging | Hold a nonblocking OS lease lock for v2 atomic-save workspaces; periodically reclaim only ready-marked workspaces whose lock is free; workflow #327 passed normal and sanitized jobs |
| 2026-10-03 | terminal-quoted-backslashes | Preserve unknown backslashes inside double-quoted arguments and Tab-completion prefixes; only `\\` and `\"` are escapes; normal and sanitized hosted workflow #323 passed |
| 2026-10-03 | atomic-save-collision | Stage writes in uniquely reserved hidden sibling workspaces so a user-owned `<target>.tmp` file or symlink is never opened or removed; normal and sanitized hosted workflow #319 passed |
| 2026-10-03 | filesystem-copy-rollback | Journal existing destination files, created entries, and directory times so a failed recursive merge restores prior state; normal and sanitized hosted workflow #315 passed |
| 2026-10-03 | text-editor-paste-batch | Normalize clipboard line breaks while counting rows, reuse the existing line prefix, then insert all pasted rows in one range; normal and sanitized hosted workflow #311 passed |
| 2026-10-03 | text-editor-snapshot-clone | Measure undo snapshot line storage during cloning after the existing pre-copy budget check; normal and sanitized hosted workflow #307 passed |
| 2026-10-02 | drawing-format | Keep `.modr` encoder/decoder bounds identical while preserving larger live canvases; oversized saves fail instead of creating files the decoder cannot reopen |
| 2026-10-02 | wallpaper-decode | Load PNG/JPEG directly from file after checking dimensions; reject inputs above 16,777,216 pixels before decoded allocations while preserving BMP loading |
| 2026-10-02 | drawing-input-bound | Check `.modr` file size before reading, stream within the format's encoded-size cap, and preserve canvas/prompt state on rejection |
| 2026-10-02 | session-line-bound | Read persisted session records with a 16 KiB line cap and stop restore at the first overlong record |
| 2026-10-03 | session-restore-bounds | Stop restore after 1,024 records or once 128 windows are live; preserve valid-session handling; hosted workflow #191 passed |
| 2026-10-03 | session-save-cap | Persist only the topmost 128 restorable windows in stacking order so every saved row survives bounded restore; hosted workflow #195 passed |
| 2026-10-03 | settings-record-cap | Stop Desktop Settings loading after 64 records as well as bounding each line; preserve prior preferences when later rows exceed the budget; hosted workflow #199 passed |
| 2026-10-03 | session-save-feedback | Report failed atomic session saves before SDL teardown; preserve the existing snapshot; hosted workflow #203 passed |
| 2026-10-03 | settings-save-feedback | Report rejected preference changes and failed atomic saves in Settings while keeping live choices active and retryable; hosted workflow #207 passed |
| 2026-10-03 | settings-notification-save-failure | Retain atomic-save failure state from wallpaper path notifications so Settings can display it later; hosted workflow #211 passed |
| 2026-10-03 | game-record-save-feedback | Keep new Snake and Minesweeper records active in-session, report failed persistence in their UI, and retry on focus return or restart; hosted workflow #215 passed |
| 2026-10-03 | terminal-history-save-feedback | Report failed command-history writes without blocking commands; retry on later history-eligible submissions and preserve the old file on failed startup migration; hosted workflow #219 passed |
| 2026-10-03 | editor-save-allocation | Validate serialized size once and reserve one Text Editor save buffer, preserving exact newline output while eliminating the `ostringstream::str()` copy; hosted workflow #223 passed |
| 2026-10-03 | drawing-save-stream | Stream `.modr` RGB output through the atomic filesystem producer in bounded 16 KiB chunks; preserve the old file on failure; hosted workflow #228 passed |
| 2026-10-03 | drawing-baseline-reuse | Refresh Drawing's saved baseline in place when capacity permits, avoiding a temporary full RGBA allocation for same-size captures; hosted workflow #232 passed |
| 2026-10-03 | drawing-modr-stream-open | Decode `.modr` input incrementally from filesystem chunks and adopt the validated RGBA buffer directly, preserving the old canvas on rejection; hosted workflow #236 passed |
| 2026-10-03 | editor-save-stream | Stream validated Text Editor serialization through the atomic filesystem producer in bounded 16 KiB chunks; hosted workflow #240 passed |
| 2026-10-03 | terminal-history-save-stream | Share a fixed-buffer stream writer for Text Editor and Terminal; persist eligible command history without a duplicate 2 MiB string and retain failure/retry behavior; hosted workflow #244 passed |
| 2026-10-03 | session-path-line-breaks | Escape newlines in quoted session paths so unusual filenames round-trip without splitting bounded line records; hosted workflow #248 passed |
| 2026-10-03 | drawing-history-byte-accounting | Cache validated preimage-pixel bytes per history entry and undo/redo stack; insertions and evictions no longer rescan retained tiles, with hosted workflow #252 passed |
| 2026-10-03 | sanitized-headless-ci | Build and run the complete headless suite under AddressSanitizer/UBSan in GitHub Actions; fix the stale app pointer exposed in mouse-capture coverage; hosted workflow #257 passed |
| 2026-10-03 | text-cache-hit-lookup | Replace per-hit owned cache-key serialization with transparent font/text/color lookup views; create owned text keys only on misses; hosted workflow #262 passed |
| 2026-10-03 | text-cache-single-storage | Store LRU nodes as views into immutable map keys, avoiding duplicate text storage while preserving eviction through map rehashes; hosted workflow #266 passed |
| 2026-10-03 | terminal-render-buffers | Cache the cwd prompt and reuse Terminal input, cursor, selection, reverse-search, and hit-test strings across steady-state frames; hosted workflow #271 passed |
| 2026-10-03 | browser-rename-prefix | Compare the rename caret prefix directly against the current name and only rebuild the owned measured prefix when it changes; hosted workflow #275 passed |
| 2026-10-03 | browser-filter-buffer | Build the active filter label and cursor measurement in one retained buffer, avoiding prefix/suffix temporaries on each frame; hosted workflow #279 passed |
| 2026-10-03 | window-traversal-reuse | Reuse nesting-safe identity snapshots across update/render passes and validate live windows through a pointer/ID index; normal and sanitized hosted workflow #283 passed |
| 2026-10-03 | taskbar-layout-reuse | Remove temporary taskbar order/width vectors and retain focused-first traversal with shared draw/hit geometry; normal and sanitized hosted workflow #287 passed |
| 2026-10-03 | terminal-scrollback-deque | Evict capped Terminal output and viewport measurements from the front without shifting retained rows; normal and sanitized hosted workflow #291 passed |
| 2026-10-03 | terminal-copy-notify-stack | Walk recursive-copy notifications iteratively and reuse directory-entry type data while preserving order; normal and sanitized hosted workflow #295 passed |
| 2026-10-03 | filesystem-copy-frames | Replace recursive directory-copy calls with an explicit frame stack; retain direct traversal, link checks, bounded streaming, and rollback; normal and sanitized hosted workflow #299 passed |
| 2026-10-03 | terminal-history-trim | Retain the newest valid command-history suffix with one backward scan and one prefix erase; normal and sanitized hosted workflow #303 passed |
| 2026-10-02 | settings-line-bound | Share the 16 KiB persisted-line reader with Desktop Settings and reject wallpaper paths that cannot round-trip |
| 2026-10-02 | game-record-bound | Bound Snake and Minesweeper save-file reads by row and byte limits; reject malformed or impossible records |
| 2026-10-02 | terminal-output-pan | Pan clipped Terminal output rows with UTF-8-safe viewport-sized segments and preserve vertical history scrolling |
| 2026-10-02 | terminal-paste-normalization | Preserve all clipboard lines in the single-line Terminal prompt by converting line breaks and tabs to spaces |
| 2026-10-02 | minesweeper-uniform-placement | Sample mines uniformly without replacement from first-click-eligible cells; remove retry-cap fallback bias |
| 2026-10-02 | filesystem-path-normalize | Normalize virtual paths in one scan without stringstream or copied component strings; preserve path rules under generated tests |
| 2026-10-02 | filesystem-path-join | Compose directory and child paths directly through the shared component scanner, preserving join semantics without concatenated temporaries |
| 2026-10-03 | filesystem-descendant-boundary | Compare normalized path boundaries directly, avoiding a temporary ancestor-prefix string in recursive operations |
| 2026-10-03 | filesystem-copy-walk | Direct iterator traversal avoids sorted listing snapshots for tree copies; hosted workflow #157 passed |
| 2026-10-03 | filesystem-remove-all | Delegate directory-tree cleanup without staging child paths; hosted workflow #161 passed |
| 2026-10-03 | filesystem-list-checks | Avoid per-file canonicalization without hiding in-root dangling symlinks; hosted workflow #166 passed |
| 2026-10-03 | filesystem-no-follow-status | Classify regular listing entries from one no-follow status lookup and preserve dangling links; hosted workflow #170 passed |
| 2026-10-03 | filesystem-browser-filter-indices | Keep the raw folder snapshot and store filter results as indices instead of copying matching filenames; hosted workflow #174 passed |
| 2026-10-03 | filesystem-browser-selection-indices | Remap query-only selection changes against sorted source indices, avoiding per-keystroke filename identity sets; hosted workflow #179 passed |
| 2026-10-03 | filesystem-copy-resolved-paths | Resolve copy paths once per entry, stream files directly from validated host paths, and reject physical destination aliases into the source tree; hosted workflow #183 passed |
| 2026-10-03 | wallpaper-bmp-bound | Inspect BMP dimensions before SDL pixel decoding using the same file handle; retain SDL decoding for valid images; hosted workflow #187 passed |
| 2026-10-02 | prompt-retry | Failed Text Editor and Drawing path, line-number, RGB, or file operations retain the active prompt, input, caret, and horizontal position for correction and retry |
| 2026-10-02 | same-file-reload | Text Editor and Drawing can reload their current file after an external overwrite; dirty documents retain explicit discard protection, with headless coverage |
| 2026-10-02 | filesystem-filter | Directory sort/filter now avoids lowercase copies per comparison, normalizes the query once per listing pass, and preserves mixed-case matching/order; headless filesystem tests passed |
| 2026-10-02 | text-editor-hit-test | Long-line mouse hit testing measures the clicked prefix once and maps UTF-8 codepoints to byte columns; hosted workflow #113 passed |
| 2026-10-02 | text-editor-find-memory | Find stores one location checkpoint per 256 non-overlapping hits, resolves navigation from checkpoints, renders only viewport-intersecting highlights, and performs dense Replace All with linear output building; hosted workflow #109 passed |
| 2026-10-02 | drawing-dirty-upload | Drawing streams only the accumulated changed canvas rectangle to its texture; paint/fill use pixel/span bounds, undo/redo use changed tile bounds, and resize/load/Clear remain full refreshes |
| 2026-10-02 | fs-browser-filter-cache | Filesystem Browser reuses one directory snapshot during filter edits; F5 and filesystem notifications refresh it; hosted workflow #100 passed |
| 2026-10-02 | terminal-input-editing | Terminal command input gains UTF-8-safe keyboard/mouse selection and clipboard editing; hosted workflow #103 passed |
| 2026-10-02 | terminal-touch-mtime | Terminal touch updates existing regular-file timestamps without changing content; hosted workflow #105 passed |
| 2026-09-29 | 7.51 | Text Editor streams file opens and rejects documents above 16 MiB or 65,536 lines; hosted headless workflow #71 passed |
| 2026-09-29 | 7.50 | Drawing reuses stroke-capture and Fill/Clear dirty-tracking scratch maps, resetting only touched tile indices; hosted headless workflow #69 passed |
| 2026-09-29 | 7.49 | Drawing history now stores only sparse tile preimages; removed full-buffer undo/redo branches and revalidated state/byte caps; hosted headless workflow #66 passed |
| 2026-09-29 | 7.48 | Drawing Clear captures only changed 32×32 tiles, keeping sparse large canvases undoable; hosted headless workflow #64 passed |
| 2026-09-27 | 7.47 | Drawing Fill captures only touched 32×32 preimage tiles, keeping localized fills undoable on canvases above the history budget; hosted headless workflow #61 passed |
| 2026-09-27 | 7.46 | Drawing tracks dirty state per 32×32 tile, avoiding full-canvas comparisons after sparse undo/redo while preserving save-mid-history semantics; hosted headless workflow #59 passed |
| 2026-09-27 | 7.45 | Drawing stroke history captures reversible 32×32 preimage tiles lazily instead of cloning the full canvas at gesture start; hosted headless workflow #56 passed |
| 2026-09-27 | 7.44 | Drawing raster primitives now share buffer validation and Bresenham brush strokes; bounded rectangle loops to visible pixels; hosted headless workflow #54 passed |
| 2026-09-26 | 7.43 | Drawing Fill uses a scanline-span worklist to reduce per-pixel queue traffic while preserving four-connected selection; hosted headless workflow #52 passed |
| 2026-09-26 | 7.42 | Drawing reuses active Save, Open, and RGB prompt caret-prefix widths until the prefix or shared font changes; hosted headless workflow #50 passed |
| 2026-09-26 | 7.41 | Text Editor reuses status-bar caret-prefix widths across Find, Replace, Go to Line, Open, and Save As prompts; hosted headless workflow #48 passed |
| 2026-09-26 | 7.40 | Filesystem Browser uses renderer-cached filename widths and caches rename caret-prefix metrics across steady-state frames |
| 2026-09-26 | 7.39 | Filesystem Browser reuses filter caret-prefix pixel width until the prefix or shared font metrics change |
| 2026-09-26 | 7.38 | Terminal caches visible UTF-8 byte boundaries per scrollback row and viewport width, refreshing after width or shared-font changes |
| 2026-09-26 | 7.37 | Text Editor reuses measured UTF-8 viewport byte bounds per visible line and invalidates them when row range, width, horizontal scroll, text, or font changes |
| 2026-09-26 | 7.36 | Text Editor reuses measured visible Find prefix positions and the cached query width across frames; invalidates viewport geometry on query, row-range, or font changes |
| 2026-09-26 | 7.35 | Text Editor reuses syntax spans for its current viewport and invalidates them on edits or syntax-mode changes, avoiding per-frame retokenization |
| 2026-09-26 | 7.34 | Text Editor find-query width is measured once per query/font state and reused across visible highlights, with search/font invalidation |
| 2026-09-26 | 7.33 | Terminal input and reverse-search cursor placement reuse cached prefix texture widths, with measurement fallback if rasterization fails |
| 2026-09-26 | 7.32 | Text Editor find highlights seek into row-sorted matches and traverse only hits within visible rows, rather than rescanning all matches per rendered line |
| 2026-09-26 | 7.31 | Taskbar clock formats local time and date once per displayed minute; textures rebuild only when their displayed text changes or font invalidation requires it |
| 2026-09-26 | 7.30 | Taskbar window-title widths are measured once per title/font state and invalidated on rename or font-size change |
| 2026-09-26 | 7.29 | Headless runner compiles shared Window Manager/app runtime objects once and links all six integration tests against them |
| 2026-09-26 | 7.28 | Registered the complete headless suite with CTest and routed GitHub Actions through that single test entry point |
| 2026-09-26 | 7.27 | CMake tracks every generated main and Settings include as an output and reconfigures when compressed fragment sets change |
| 2026-09-26 | 7.26 | Text Editor carries C-style block-comment syntax state across lines and incrementally caches states through the visible rows; edits invalidate from the changed line |
| 2026-09-26 | 7.25 | Removed the unused renderer-independent text surface cache after migrating every consumer to renderer-owned textures |
| 2026-09-26 | 7.24 | Settings caches labels, options, information, footer, and wallpaper-prompt text as renderer-owned textures in a 256-entry, estimated 16 MiB LRU; retains unchanged text across resize |
| 2026-09-26 | 7.23 | Drawing caches toolbar labels and status or prompt text as renderer-owned textures in a 256-entry, estimated 16 MiB LRU; retains labels across status updates |
| 2026-09-26 | 7.22 | Filesystem Browser caches path, filter, toolbar, row, status, and menu text as renderer-owned textures in a 256-entry, estimated 16 MiB LRU; retains variants across listing and status changes |
| 2026-09-26 | 7.21 | Terminal caches scrollback, command-input, and reverse-search text as renderer-owned textures in a 256-entry, estimated 16 MiB LRU; retains variants across output changes, scroll, and resize |
| 2026-09-26 | 7.20 | Text Editor caches visible syntax spans, line numbers, and status as renderer-owned textures in a 256-entry, estimated 16 MiB LRU; clears on renderer and UI-scale changes |
| 2026-09-26 | 7.19 | Headless GitHub Actions pins Ubuntu 24.04 and uses checkout v5's Node 24 runtime to avoid runner-label drift and the Node 20 compatibility warning |
| 2026-09-26 | 7.18 | Pong, Breakout, Snake, and Minesweeper reuse bounded renderer-owned text textures between frames instead of recreating them on every draw |
| 2026-09-26 | 7.17 | Filesystem recursive regular-file copies stream through bounded 16 KiB reads into atomic replacement instead of whole-file buffers |
| 2026-09-26 | 7.16 | Window title textures contain only the UTF-8 prefix fitting before title-bar controls and recalculate it when available width changes |
| 2026-09-26 | 7.15 | WindowManager shell text textures use a 256-entry, estimated 16 MiB LRU; Start-menu filter input is capped at 64 UTF-8 bytes |
| 2026-09-26 | 7.14 | Terminal startup seeks to a bounded history tail and parses only recent bytes; one recovery tail capped at 16 MiB preserves history before a trailing oversized record |
| 2026-09-26 | 7.13 | Terminal command history bounded by 500 entries, 2 MiB, and 64 KiB per entry; load streamed and oversized entries excluded |
| 2026-09-26 | 7.12 | Text Editor rasterizes only visible UTF-8 syntax spans and invalidates its surface cache on scroll or resize |
| 2026-09-26 | 7.11 | Terminal `cat` streams bounded filesystem chunks, normalizes line endings incrementally, and stops at 5,000 rows |
| 2026-09-26 | 7.10 | Terminal scrollback limited to 2,000 rows, 8 MiB, and 64 KiB per row; render/cache only viewport-fitting UTF-8 prefixes |
| 2026-09-26 | 7.9 | Bound Text Editor undo/redo to 50 states and estimated 64 MiB; CURRENT_CHUNK remains await-5.x-unpark |
| 2026-09-26 | 7.8 | Bound Drawing undo/redo history to 32 states and 64 MiB; CURRENT_CHUNK remains await-5.x-unpark |
| 2026-09-24 | 7.7 | Multi-column desktop icons; CURRENT_CHUNK back to await-5.x-unpark |
| 2026-09-22 | 7.6 | Start menu type-ahead filter; CURRENT_CHUNK back to await-5.x-unpark |
| 2026-09-21 | 7.5 | Wallpaper fit cover/contain/center; CURRENT_CHUNK back to await-5.x-unpark |
| 2026-09-20 | shell-polish | Start menu hit targets share AppRegistry rows; hardcoded entries table retired; CURRENT_CHUNK now await-5.x-unpark |
| 2026-09-17 | 7.4 | App registry drives Start menu, desktop icons, session kinds; CURRENT_CHUNK now shell-polish |
| 2026-09-15 | 7.3 | Desktop icons under wallpaper; windows win hit-testing; CURRENT_CHUNK now 7.4 |
| 2026-09-14 | 7.2 | Breakout under Start -> Games; CURRENT_CHUNK now 7.3 |
| 2026-09-11 | 7.1 | PNG/JPEG wallpaper via pinned stb_image; Settings/WM accept .bmp/.png/.jpg/.jpeg; CURRENT_CHUNK now 7.2 |

Older recent-work rows: [`SESSION_LOG.md`](SESSION_LOG.md).

## Known debts

- Atomic-save cleanup combines a bounded 16-parent first-touch/every-32-operations tracker with a resumable startup traversal that inspects at most 32 entries per frame, covering readable nested directories without blocking launch; permission-denied and failed directory frames are skipped while sibling traversal continues. The tracker shares cadence across symlink aliases and rescans an evicted directory on revisit. Each synchronous directory sweep reads borrowed names from a POSIX directory stream and constructs paths only for workspace candidates, avoiding per-entry path objects and an unbounded candidate list. Workspace and child-entry validation borrow basenames from existing paths instead of allocating filename copies. Scheduled save sweeps reuse the exclusive parent lock for following workspace setup. Failed parent locking or incomplete enumeration schedules a retry on the next write or listing. V4 setup and sweeps coordinate through a brief parent-directory lock; current symlink owner tokens require the strict random-name format, while validated regular markers remain supported for older v4 writers. Incomplete v4 workspaces require a valid ownership token, with the pre-marker crash remnant recoverable only by its strict 128-bit random name and empty-directory removal. Marked v4/v3/v2 workspaces require a ready marker and free regular-file lease lock. Cleanup removes only recognized entries with expected types and preserves any workspace containing unknown entries; incomplete older workspaces remain untouched.

## Priority order for "next / continue"

1. **5.x** language only after they unpark it (`await-5.x-unpark`)
2. **6.x** IDE only after a `run` loop exists
3. Further shell soft polish only when a concrete debt is listed

**Cut order if scope tight:** never cut core shell work for another game.

## Phase 4 - Living-inside-it

| ID | Chunk | Status | Deliverable / exit criteria |
|----|--------|--------|-----------------------------|
| 4.1 | Terminal quoted arguments | done | `cat "/home/monolith/my file.txt"` and similar work; doc the quoting rules in `docs/apps/terminal.md` |
| 4.2 | Settings font / UI scale | done | Settings persists 90%, 100%, or 115% shared interface text size and applies it live; README stays one-line |
| 4.3 | Editor wrap or horizontal scroll | done | Long lines remain editable with cursor-following horizontal scroll and Shift + wheel panning; `docs/apps/text-editor.md` updated |
| 4.4 | Drawing eyedropper | done | Pick samples a canvas pixel into custom RGB without changing pixels or undo history; still saves `.modr` |

## Phase 5 - Language (parked)

| ID | Chunk | Status |
|----|--------|--------|
| 5.1-5.6 | Language design through sound | parked |

## Phase 6 - IDE (parked)

| ID | Chunk | Status |
|----|--------|--------|
| 6.1 | IDE shell | parked |

## Phase 7 - Growth

| ID | Chunk | Status | Deliverable / exit criteria |
|----|--------|--------|-----------------------------|
| 7.1 | PNG/JPEG wallpaper | done | stb_image build-time fetch; BMP via SDL_LoadBMP; PNG/JPEG via WallpaperImage |
| 7.2 | Fourth game (Breakout) | done | Same Start -> Games pattern + `verify_games_integration.sh` |
| 7.3 | Desktop icons | done | Left-column icons under wallpaper; windows win hit-testing; see `docs/notes/7.3-desktop-icons.md` |
| 7.4 | App registry | done | `AppRegistry` drives Start menu, desktop icons, session kinds; see `docs/notes/7.4-app-registry.md` |
| shell-polish | Start menu hit unify | done | Hit targets rebuild from AppRegistry rows; hardcoded entries table retired; see `docs/notes/shell-polish.md` |
| 7.5 | Wallpaper fit | done | cover/contain/center display modes; see `docs/notes/7.5-wallpaper-fit.md` |
| 7.6 | Start menu type-ahead | done | filter by label while open; see `docs/notes/7.6-start-menu-typeahead.md` |
| 7.7 | Multi-column desktop icons | done | Fill extra columns left-to-right when height is short; see `docs/notes/7.7-desktop-icon-columns.md` |
| 7.8 | Drawing history memory | done | Cap combined undo/redo to 32 states and 64 MiB; retain move-based snapshot traversal |
| 7.9 | Text Editor history memory | done | Cap combined undo/redo to 50 states and estimated 64 MiB; retain typing coalescing and move-based traversal |
| 7.10 | Terminal scrollback memory | done | Cap scrollback at 2,000 rows, 8 MiB total, and 64 KiB per row; render only visible UTF-8 prefixes |
| 7.11 | Stream Terminal cat reads | done | Add a 16 KiB filesystem chunk API; normalize line endings incrementally and stop reading at the Terminal output-line cap |
| 7.12 | Text Editor viewport rendering | done | Rasterize only visible UTF-8 syntax spans on long lines; invalidate cached surfaces when scroll position or client size changes |
| 7.13 | Terminal command history memory | done | Bound persisted history by 500 entries, 2 MiB, and 64 KiB per entry; stream loading and retain newest valid commands |
| 7.14 | Seek Terminal history tail | done | Add bounded filesystem tail-chunk reads; load recent history without scanning large legacy prefixes, with one recovery tail capped at 16 MiB for a trailing oversized record |
| 7.15 | Bound shell text textures | done | Keep WindowManager shell text textures within 256 entries and an estimated 16 MiB using LRU eviction; cap Start-menu filter input at 64 UTF-8 bytes |
| 7.16 | Bound window title rasterization | done | Cache only complete UTF-8 title prefixes that fit before the title buttons; recalculate on width changes and rerasterize when the visible prefix, font, or focus color changes |
| 7.17 | Stream recursive file copies | done | Copy regular files in bounded 16 KiB chunks through the atomic writer, validating the byte count without retaining full source contents |
| 7.18 | Reuse game text textures | done | Cache Pong, Breakout, Snake, and Minesweeper text textures across frames in a renderer-aware LRU bounded to 256 entries and an estimated 16 MiB; clear on renderer or interface-scale changes |
| 7.19 | Stabilize headless CI runtime | done | Pin the hosted runner to Ubuntu 24.04 and use `actions/checkout@v5` to avoid upcoming `ubuntu-latest` drift and the Node 20 compatibility path |
| 7.20 | Reuse Text Editor text textures | done | Cache viewport syntax spans, line numbers, and status in a renderer-aware LRU bounded to 256 entries and an estimated 16 MiB; retain unchanged text variants and clear on renderer or UI-scale changes |
| 7.21 | Reuse Terminal text textures | done | Cache viewport scrollback rows, normal input fragments, and reverse-search text in a renderer-aware LRU bounded to 256 entries and an estimated 16 MiB; retain unchanged variants and clear on renderer or UI-scale changes |
| 7.22 | Reuse Browser text textures | done | Cache path, filter, toolbar, row, status, and context-menu text in a renderer-aware LRU bounded to 256 entries and an estimated 16 MiB; retain unchanged variants and clear on renderer or UI-scale changes |
| 7.23 | Reuse Drawing text textures | done | Cache toolbar labels and status or prompt text in a renderer-aware LRU bounded to 256 entries and an estimated 16 MiB; retain unchanged labels and clear on renderer or UI-scale changes |
| 7.24 | Reuse Settings text textures | done | Cache labels, options, information lines, footer, and wallpaper-path prompt in a renderer-aware LRU bounded to 256 entries and an estimated 16 MiB; retain unchanged text and clear on renderer or UI-scale changes |
| 7.25 | Remove obsolete text surface cache | done | Delete the unused SDL_ttf surface cache after confirming no source or generated build inputs reference it |
| 7.26 | Text Editor multiline block comments | done | Carry C-style `/* ... */` state across lines in Code mode, cache outgoing state through the viewport, and invalidate from edited rows |
| 7.27 | Track generated source fragments | done | Make CMake reconfigure when compressed fragments change and regenerate all missing decompressed include outputs |
| 7.28 | Register headless CTest suite | done | Expose the existing verification runner through CTest and use the registered test in GitHub Actions |
| 7.29 | Reuse headless integration objects | done | Compile the shared Window Manager and app sources once and link all six integration test binaries to those objects |
| 7.30 | Cache taskbar title measurements | done | Reuse per-window pixel widths during taskbar layout and invalidate measurements when a title or UI font changes |
| 7.31 | Cache taskbar clock formatting | done | Convert and format local time/date once per displayed minute; retain textures until displayed text or font state changes |
| 7.32 | Render visible Text Editor find matches | done | Seek once into ordered search results per frame, then walk only hits belonging to the rendered rows |
| 7.33 | Reuse Terminal cursor prefix widths | done | Use existing or newly cached renderer text widths to position normal and reverse-search cursors, avoiding repeated font metrics on steady-state renders |
| 7.34 | Cache Text Editor find-query width | done | Measure the identical query once for all visible match highlights and invalidate it after query refresh or shared-font changes |
| 7.35 | Cache visible Text Editor syntax spans | done | Reuse lexical spans for current rendered rows, retaining incremental block-comment state and invalidating the viewport cache on edits or syntax-mode changes |
| 7.36 | Cache Text Editor Find geometry | done | Measure visible match-prefix widths once for the current viewport and reuse the cached query width; invalidate after query, viewport-row, or shared-font changes |
| 7.37 | Cache Text Editor viewport measurements | done | Reuse visible UTF-8 line byte bounds and hidden pixel widths until viewport rows, content width, horizontal scroll, document text, or shared font changes |
| 7.38 | Cache Terminal viewport measurements | done | Reuse the visible UTF-8 prefix byte boundary for each scrollback row until viewport width, row text, or shared font metrics change |
| 7.39 | Cache Filesystem Browser filter caret width | done | Reuse the measured width of the active filter caret prefix until that prefix or the shared font changes |
| 7.40 | Reuse Filesystem Browser row metrics | done | Use cached text texture widths for rendered filenames and reuse rename caret-prefix width until the prefix or shared font changes |
| 7.41 | Cache Text Editor prompt caret metrics | done | Reuse status-bar caret-prefix widths across search and path prompts; remeasure after cursor-text or shared-font changes |
| 7.42 | Cache Drawing prompt caret metrics | done | Reuse active prompt caret-prefix width; remeasure after prompt text or shared-font changes |
| 7.43 | Optimize Drawing flood fill | done | Replace per-pixel work items with horizontal spans while preserving four-connected selection and opaque RGB pixels |
| 7.44 | Batch Drawing raster validation | done | Validate buffers once per primitive, rasterize each brush drag with one Bresenham traversal, and bound rectangle loops to the visible canvas |
| 7.45 | Store sparse Drawing stroke history | done | Capture each touched 32×32 tile before its first actual stroke write, toggle its pixels for undo/redo, and discard over-budget transient captures |
| 7.46 | Cache Drawing dirty state by tile | done | Maintain exact modified-state bits per 32×32 tile; sparse undo/redo and Fill/Clear recheck only touched tiles against the saved baseline |
| 7.47 | Store sparse Drawing Fill history | done | Capture each touched 32×32 tile before its first fill-span write; keep localized fills undoable on canvases above 64 MiB while releasing over-budget captures |
| 7.48 | Store sparse Drawing Clear history | done | Capture only non-background 32×32 tiles during Clear; keep sparse canvases above 64 MiB undoable, preserve saved-baseline dirty tracking, and retain bounded fallback for oversized clears |
| 7.49 | Remove full-snapshot Drawing history | done | Use tile preimages as the sole undo-entry representation; preserve 32-state and 64 MiB limits with real edit and sparse-budget coverage |
| 7.50 | Reuse Drawing touched-tile scratch maps | done | Keep per-canvas stroke-capture and Fill/Clear dirty-tracking maps allocated between edits and clear only indices touched by the prior operation |
| 7.51 | Bound Text Editor file I/O | done | Stream file opens in 16 KiB chunks, normalize line endings across chunk boundaries, and enforce 16 MiB / 65,536-line limits on open and save without replacing the current document on rejected opens |
| 7.52 | Upload dirty Drawing regions | done | Accumulate changed pixel/span/tile bounds and update only that region of the streaming texture; resize, load, and Clear force full refreshes |
| 7.53 | Bound Text Editor Find results | done | Store one match location per 256 hits, resolve navigation from checkpoints, cache only viewport-intersecting highlights, and Replace All with linear output building |
| 7.54 | Speed up Text Editor hit testing | done | Map mouse x positions to UTF-8 byte columns with one `TTF_MeasureUTF8` pass instead of measuring every growing line prefix; hosted workflow #113 passed |
| 7.55 | Reduce Filesystem Browser filter allocations | done | Compare names case-insensitively without per-entry lowercase copies during sorting/filtering; lowercase the query once per listing pass and preserve existing mixed-case matching and ordering |
| 7.56 | Restore same-file reload after external changes | done | Let Text Editor and Drawing reopen their already-bound path instead of self-focusing; preserve explicit discard protection for dirty content and cover clean/dirty reloads in headless tests |
| 7.57 | Align Drawing `.modr` codec limits | done | Enforce matching encoder/decoder bounds and exact buffers, preserve large-canvas editing, and test the production codec plus oversized-save rejection |
| 7.58 | Retry failed inline prompts | done | Preserve Text Editor and Drawing prompt mode, input, caret, and horizontal position on recoverable validation or file-operation failures; verify corrections can be retried |
| 7.59 | Bound wallpaper decoding | done | Inspect PNG/JPEG dimensions before decode, load from file without a whole-file compressed buffer, and reject images above 16,777,216 pixels; keep the BMP path unchanged |
| 7.60 | Bound Drawing `.modr` opens | done | Reject oversized files before buffering and cap streamed reads at the maximum valid encoded payload; preserve the active canvas and Open prompt on rejection |
| 7.61 | Bound session restore records | done | Read session lines with a 16 KiB cap and stop after an overlong row without unbounded allocation; preserve already restored entries |
| 7.62 | Bound Desktop Settings records | done | Read settings records with the shared 16 KiB reader and reject wallpaper paths that exceed or break the persisted line format |
| 7.63 | Bound game record loading | done | Limit Snake and Minesweeper persisted score rows and line bytes; accept only complete in-range values and preserve earlier valid Minesweeper records |
| 7.64 | Pan long Terminal output rows | done | Preserve vertical scrollback while panning long rows by complete UTF-8 codepoints; render/cache only the visible viewport-sized segment |
| 7.65 | Preserve multiline Terminal paste | done | Keep all clipboard text in the single-line prompt by mapping line breaks and tabs to spaces; treat CRLF as one separator and cover the behavior in headless tests |
| 7.66 | Sample Minesweeper mines uniformly | done | Select without replacement from cells outside the first-click exclusion zone, removing repeated-coordinate retries and the deterministic fallback; cover safe opens and exact mine counts across difficulties |
| 7.67 | Normalize virtual paths in one pass | done | Resolve path segments directly into the result without a stringstream or per-component copies; differential-test behavior across generated inputs and long separator runs |
| 7.68 | Normalize joined paths directly | done | Reuse the component scanner for `join()` so directory/child combinations are normalized in one output buffer; preserve existing semantics with generated differential coverage |
| 7.69 | Check filesystem path boundaries directly | done | Test equality, root, sibling-prefix, and generated cases while removing the allocated `ancestor + "/"` prefix from descendant checks |
| 7.70 | Walk recursive copies directly | done | Avoid per-directory listing vectors, type sorting, and ordering work during tree copies while preserving symlink and rollback behavior; hosted workflow #157 passed |
| 7.71 | Remove trees without child staging | done | Avoid an application-side vector of every directory entry while preserving root refusal, error reporting, and symlink target safety; hosted workflow #161 passed |
| 7.72 | Reduce directory listing path checks | done | Avoid canonicalizing ordinary files during directory listings while retaining containment checks for symlink and directory entries; hosted workflow #166 passed |
| 7.73 | Classify listings from no-follow status | done | Reuse one no-follow entry status for ordinary files; only resolve in-root symlinks to determine directory type while retaining containment checks; hosted workflow #170 passed |
| 7.74 | Filter browser rows by snapshot index | done | Keep one directory snapshot and represent visible matches as indices so query edits avoid copying matching entry names; hosted workflow #174 passed |
| 7.75 | Remap filter selection by source index | done | Preserve visible selection across query edits by merging source indices rather than allocating filename identity sets; hosted workflow #179 passed |
| 7.76 | Resolve recursive copy paths once | done | Reuse validated host paths and no-follow entry status during streaming tree copies; reject same/descendant destinations reached through in-root symlinks; hosted workflow #183 passed |
| 7.77 | Bound BMP wallpaper decoding | done | Inspect BMP dimensions from the SDL decoder's open file before pixel decoding; keep BMP parsing in SDL and reject images above the existing 16,777,216-pixel limit; hosted workflow #187 passed |
| 7.78 | Bound session restore work | done | Stop after 1,024 session records or once 128 windows are live; preserve valid-session handling and cover both limits; hosted workflow #191 passed |
| 7.79 | Keep session saves restorable | done | Save the topmost 128 restorable windows in stacking order, matching the restore cap and preserving the entries most recently raised; hosted workflow #195 passed |
| 7.80 | Bound Desktop Settings record loading | done | Stop settings parsing after 64 rows while keeping the shared 16 KiB line cap and current values unchanged when no valid earlier records load; hosted workflow #199 passed |
| 7.81 | Report session-save failures | done | Show a warning while SDL is still live and log the session path when atomic save fails; verify main lifecycle ordering; hosted workflow #203 passed |
| 7.82 | Report Desktop Settings save failures | done | Keep settings changes live on disk failure, report rejection or temporary persistence in the footer, and retry writes when the active choice is reselected; hosted workflow #207 passed |
| 7.83 | Surface automatic settings save failures | done | Retain failed wallpaper move/delete saves in shell state for the Settings footer; clear after successful persistence and cover recovery; hosted workflow #211 passed |
| 7.84 | Report game-record save failures | done | Keep new Snake scores and Minesweeper best times active in-session, show a warning instead of a false new-record claim, retry on focus return or restart, and cover failure plus recovery; hosted workflow #215 passed |
| 7.85 | Report Terminal history save failures | done | Keep commands running on history-write failure, report failed persistence and recovery, preserve failed startup-migration input, and cover recovery plus reload; hosted workflow #219 passed |
| 7.86 | Reduce Text Editor save allocations | done | Compute validated serialized size once, reserve one contiguous buffer, preserve blank/trailing-line output, and verify an exact 16 MiB save; hosted workflow #223 passed |
| 7.87 | Stream Drawing saves | done | Encode `.modr` RGB directly into the atomic filesystem producer with a fixed 16 KiB scratch buffer, avoiding a second ~48 MiB encoded allocation at maximum dimensions; test byte equivalence, failure rollback, and bounded maximum-canvas writes; hosted workflow #228 passed |
| 7.88 | Reuse Drawing saved-baseline capacity | done | Capture current pixels into the saved baseline's existing allocation whenever capacity allows; allocate a replacement only when the canvas grows beyond it; verify same-size saves keep the storage stable and contents exact; hosted workflow #232 passed |
| 7.89 | Stream Drawing opens | done | Decode `.modr` files incrementally from bounded filesystem chunks, validate the exact payload before adoption, and move the decoded RGBA buffer directly into the canvas; test arbitrary chunk boundaries, truncated payloads, and output preservation; hosted workflow #236 passed |
| 7.90 | Stream Text Editor saves | done | Validate byte and line limits before writing; serialize LF-separated lines through the filesystem's atomic producer using a fixed 16 KiB buffer; verify exact maximum-size content and separators across chunk boundaries; hosted workflow #240 passed |
| 7.91 | Stream Terminal history saves | done | Reuse a shared fixed-buffer writer for editor and Terminal persistence; stream the exact 2 MiB history limit, verify chunk-write failure propagation, and preserve existing retry/atomic behavior; hosted workflow #244 passed |
| 7.92 | Preserve line-break session paths | done | Escape LF inside quoted session paths, preserve existing quote/backslash and legacy-token decoding, and round-trip an opened newline-bearing filename through WindowManager save/restore; hosted workflow #248 passed |
| 7.93 | Cache Drawing history byte totals | done | Store validated preimage-pixel bytes on each entry and maintain undo/redo totals through insertion, eviction, state moves, redo invalidation, overflow, and resets; hosted workflow #252 passed |
| 7.94 | Sanitize hosted headless tests | done | Add ASan/UBSan build and full-suite jobs, pass compiler flags to all test binaries and shared runtime objects, and fix a close-window test's stale app pointer; hosted workflow #257 passed |
| 7.95 | Remove text-cache hit allocations | done | Look up renderer text textures with transparent font/text/color views so hits avoid constructing an owned composite key; preserve owned miss keys and LRU behavior; normal and sanitized hosted workflow #262 passed |
| 7.96 | Store text cache keys once | done | Keep LRU nodes as non-owning views into immutable map keys, eliminating duplicated text strings; verify a retained LRU entry survives rehash and count eviction; normal and sanitized hosted workflow #266 passed |
| 7.97 | Reuse Terminal render buffers | done | Cache the cwd prompt until its path changes; reuse normal/reverse-search/selection text and mouse hit-test prefix storage; normal and sanitized hosted workflow #271 passed |
| 7.98 | Avoid rename-prefix copies | done | Compare the inline rename cursor prefix in place and update its owned measurement string only when it changes; preserve cached width invalidation and verify suffix-only edits reuse the prefix; normal and sanitized hosted workflow #275 passed |
| 7.99 | Reuse Filesystem Browser filter buffers | done | Build the active filter label in retained storage and measure from its prefix-plus-cursor text; verify the label's backing storage survives suffix-only edits; normal and sanitized hosted workflow #279 passed |
| 7.100 | Reuse Window Manager traversal snapshots | done | Reuse nesting-safe update/render snapshots after warm-up, index live pointer/ID identities for expected constant-time validation, and cover nested callbacks plus close-during-render; normal and sanitized hosted workflow #283 passed |
| 7.101 | Remove taskbar layout vectors | done | Traverse focused-first windows directly and calculate button widths from cached title metrics; preserve scroll, clipping, scaled geometry, and hit targets; normal and sanitized hosted workflow #287 passed |
| 7.102 | Evict Terminal scrollback without shifts | done | Use front-removable storage for output rows and aligned viewport measurements; preserve row and byte caps, rendering, and newest-output behavior; normal and sanitized hosted workflow #291 passed |
| 7.103 | Iterate recursive-copy notifications | done | Preserve directory-first depth-first changed-path delivery while replacing recursive calls and redundant per-child directory probes; normal and sanitized hosted workflow #295 passed |
| 7.104 | Iterate filesystem tree copies | done | Use explicit directory frames instead of recursive calls; preserve direct iteration, symlink containment, atomic file writes, and rollback semantics; normal and sanitized hosted workflow #299 passed |
| 7.105 | Trim Terminal command history in one pass | done | Preserve the newest valid commands within existing count and byte limits while scanning once and erasing the discarded prefix once; normal and sanitized hosted workflow #303 passed |
| 7.106 | Account Text Editor snapshots while cloning | done | Keep the active-document budget precheck, but account each copied line during snapshot creation instead of walking the completed snapshot again; normal and sanitized hosted workflow #307 passed |
| 7.107 | Batch Text Editor multiline paste rows | done | Normalize mixed line endings, retain the current row's prefix/suffix and final caret, and insert the assembled replacement rows as one vector range; cover selection replacement, trailing newlines, undo, and redo; normal and sanitized hosted workflow #311 passed |
| 7.108 | Roll back failed recursive copy merges | done | Journal overwritten files, newly created entries, and destination-directory times; restore pre-copy state after a later child failure; verify regular-file and in-root symlink targets, new files/directories, unrelated entries, timestamps, and backup cleanup; normal and sanitized hosted workflow #315 passed |
| 7.109 | Isolate atomic-save workspaces | done | Reserve a unique hidden sibling directory for each staged write so ordinary `<target>.tmp` files/symlinks remain untouched; verify success, producer rollback, permission retention, and workspace cleanup; normal and sanitized hosted workflow #319 passed |
| 7.110 | Preserve backslashes in quoted Terminal paths | done | Decode only `\\` and `\"` inside double quotes in both command parsing and completion-prefix scanning; verify literal and escaped backslashes in arguments and quoted paths; normal and sanitized hosted workflow #323 passed |
| 7.111 | Reclaim interrupted atomic-save workspaces | done | Lock each active v2 workspace, sweep the current destination parent on the first save and every 32 writes thereafter, and reclaim only ready-marked workspaces with a free lock; cover stale recovery, active locks, unmarked paths, and legacy paths; normal and sanitized hosted workflow #327 passed |
| 7.112 | Track atomic-save sweeps per directory | done | Give every newly used destination directory an immediate sweep and its own 32-write interval using bounded 16-parent bookkeeping; verify second-parent first touch, interval boundary, and resweep after tracker eviction; normal and sanitized hosted workflow #331 passed |
| 7.113 | Resolve Terminal home shorthand | done | Expand only `~` and `~/` to `/home/monolith` for command paths; preserve shorthand through quoted/unquoted path completion; normal and sanitized hosted workflow #335 passed |
| 7.114 | Share virtual-home path shorthand | done | Reuse one app-level `~` / `~/` expansion rule for Terminal, Text Editor, Drawing, and Settings wallpaper paths; cover completion, accepted paths, and path move/delete notifications; normal and sanitized hosted workflow #338 passed |
| 7.115 | Share atomic-save sweep cadence across aliases | done | Track cleanup cadence by resolved physical parent across symlink spellings; resolve lexical tracker misses and verify alternating aliases share the first-touch and 32-write boundary; normal and sanitized workflow #350 passed |
| 7.116 | Keep atomic-save lookalike probes nonblocking | done | Open ownership markers nonblocking and reject non-regular marker entries; a timeout-guarded sweep regression verifies FIFO lookalikes do not stall cleanup or lose user data; normal and sanitized workflow #353 passed |
| 7.117 | Reload wallpaper supplied by a move | done | Invalidate the failed-load cache when a successful move creates the configured wallpaper path; cover a move into a previously missing path |
| 7.118 | Publish atomic-save ownership tokens atomically | done | Create the v4 owner token as a validated symlink in one filesystem call, preserve legacy regular markers, and retain nonblocking rejection of FIFO/lookalike markers; the pre-publication crash gap remains documented |
| 7.119 | Reclaim atomic-save setup orphans | done | Name new v4 workspaces with 128-bit OS-random tokens; reclaim an unmarked pre-publication remnant only through empty-directory removal, preserving nonempty lookalikes |
| 7.120 | Isolate headless test data | done | Replace predictable PID/shared `/tmp` fixtures with unique `mkdtemp` directories and scoped cleanup; make the Drawing smoke test use the app's actual filesystem root; verify owner-only cleanup |
| 7.121 | Size Minesweeper for its difficulty | done | Request a comfortable board size through the per-window controller on difficulty changes; preserve maximized geometry and manual sizing on same-difficulty restarts; clamp requests and cover the real shell path |
| 7.122 | Enforce Text Editor limits during edits | done | Preflight typing, Enter, paste, Replace, and Replace All against the 16 MiB / 65,536-line open/save limits; keep serialized byte accounting incremental and rejected edits out of undo history |
| 7.123 | Count Text Editor bytes while opening | done | Track normalized serialized bytes alongside line parsing across chunk and CRLF boundaries; seed the live byte count directly and eliminate the post-open scan |
| 7.124 | Preserve data during atomic-save setup rollback | done | Use `rmdir` semantics when owner-marker publication fails, preserving unexpected entries; reuse the same empty-directory cleanup rule during orphan scavenging |
| 7.125 | Refresh Text Editor welcome guidance | done | Replace stale early-development copy with the existing Find/Replace shortcuts; assert the default document remains clean with a correct serialized-size baseline |
| 7.126 | Preserve unknown atomic-save workspace entries | done | Remove only recognized owner/lease/ready/content entries of expected types; preserve unexpected files in marked ready and incomplete workspaces during active cleanup and scavenging; retain v3/v2 recovery; hosted run #374 passed both jobs |
| 7.127 | Retry failed atomic-save sweeps promptly | done | Return sweep completion status and reschedule parent-lock/enumeration failures for the next write; cover immediate retry cadence and current-format stale workspace recovery; hosted run #376 passed both jobs |
| 7.128 | Require random names for symlink owner markers | done | Validate the strict token-name format for symlink owner markers while preserving validated regular-file marker compatibility; protect a fully populated non-token lookalike workspace; hosted run #378 passed both jobs |
| 7.129 | Bound atomic-save sweep memory | done | Process stale candidates incrementally during parent traversal instead of retaining every candidate path; cover one sweep over a large batch of stale workspaces |
| 7.130 | Share UTF-8 forward deletion | done | Centralize codepoint-safe Delete behavior across Terminal, Text Editor, Drawing, and Filesystem Browser prompts; clamp stale cursors and preserve no-op update semantics |
| 7.131 | Avoid allocations for ordinary atomic-save sweep entries | done | Inspect basenames through borrowed views and copy paths only for workspace candidates; cover a mixed directory with 128 stale workspaces and 128 ordinary files |
| 7.132 | Read atomic-save sweep names directly | done | Enumerate through a POSIX directory stream so ordinary siblings require no entry-path construction; only workspace candidates become paths, with retry preserved on open/read failures |
| 7.133 | Reuse Text Editor status render storage | done | Retain status-bar text capacity between frames and avoid an owning display-name substring in the steady-state render path; verify unchanged frames keep capacity stable |
| 7.134 | Reuse atomic-save parent lock | done | Keep the scheduled cleanup lock through workspace creation rather than reopening and relocking the same directory; preserve immediate retry after failed sweeps |
| 7.135 | Reuse Text Editor prompt render buffers | done | Append Find, Replace, Open, Save As, and Go to Line text directly from source buffers; preserve exact caret measurement while keeping unchanged prompt-buffer capacity stable |
| 7.136 | Borrow atomic workspace basenames | done | Validate workspace names directly from path storage and avoid allocating filename copies in owner checks and cleanup; preserve path edge-case handling |
| 7.137 | Borrow atomic workspace entry names | done | Inspect marker names from entry-path storage without allocating a filename string while retaining the allowed-entry and unexpected-user-data checks |
| 7.138 | Reuse Text Editor prefix measurement storage | done | Reuse a bounded 4 KiB scratch buffer for cursor/selection geometry; unusually long prefixes use temporary storage so retained memory remains bounded |
| 7.139 | Paste into Text Editor search fields | done | Insert clipboard text at the active Find/Replace caret, filter control bytes to keep prompts single-line, and refresh matches when the query changes |
| 7.140 | Unify Text Editor search input | done | Reuse one sanitizer/insertion path for typed and pasted Find/Replace text; release SDL clipboard storage with RAII and verify both input routes |
| 7.141 | Search selected Text Editor text | done | Seed Ctrl+F/Ctrl+H from a selected single-line query and start on that occurrence; leave multi-line or control-bearing selections out of the inline field |
| 7.142 | Cover Drawing keyboard path completion | done | Exercise Ctrl+O, typed filename input, Tab completion, and Enter through Drawing's real event handler, confirming the `.modr` file opens |
| 7.143 | Reclaim atomic-save leases with restricted access | done | Open stale read-only leases for locking and fall back to write-only access; verify both stale modes are reclaimed and a live read-only lock is preserved |
| 7.144 | Reclaim stale saves when directories are revisited | done | Share first-use/every-32-operation cleanup across writes and directory listings; test both Filesystem listing APIs |
| 7.145 | Reclaim abandoned saves in untouched directories | done | Traverse startup filesystem entries cooperatively at 32 entries per frame; reclaim validated nested workspaces without following symlinks and preserve unknown workspace contents |
| 7.146 | Continue atomic cleanup after subtree errors | done | Use an explicit directory stack so permission or iterator failures discard only the affected frame; verify other readable directories still reclaim stale workspaces |
| 7.147 | Bound Text Editor search fields | done | Cap each Find/Replace input at 16 MiB, preserve complete UTF-8 characters at the limit, and report rejected input; cover typing and paste in both fields |
| 7.148 | Bound long Find/Replace rendering | done | Render UTF-8-safe 64-byte contexts around Find/Replace carets and measure each match highlight only across its visible viewport slice; start viewport scans from sparse checkpoints to preserve non-overlap without rescanning dense prefixes |
| 7.149 | Make filesystem moves atomic | done | Use Linux no-replace rename so a racing destination cannot be overwritten; preserve final symlink-entry moves and fail closed when atomic host support is unavailable |
| 7.150 | Make dirty Editor decisions explicit | done | Require Ctrl+D for dirty Close/Open discard, preserve dirty text on repeated actions, support Esc cancellation and save-and-close, and re-confirm if the Open target changes; global shutdown remains blocked until the dirty Editor is explicitly resolved |
| 7.151 | Make dirty Drawing decisions explicit | done | Require Ctrl+D for dirty Close/New/Open discard, preserve the canvas on repeated actions, support Esc cancellation and save-before-Close/New, and re-confirm if the Open target changes; global shutdown remains blocked until the dirty Drawing is explicitly resolved |
| fs-browser-filter-cache | Reuse Filesystem Browser filter snapshots | done | Reuse one directory listing for filter edits; refresh it on F5 and Monolith filesystem notifications, including while filtering |
| terminal-input-editing | Terminal command-line selection | done | Support UTF-8-safe keyboard/mouse selection and Ctrl+A/C/X/V; normalize all clipboard lines and tabs into the single-line prompt |
| terminal-touch-mtime | Complete Terminal touch semantics | done | Update existing regular-file last-write time without truncation; reject outside-root symlink targets |

## Commit voice

`feat|fix|docs|chore(scope): imperative`. No em dashes / AI filler.

## Wallpaper dependency (7.1)

Prefer pinned `stb_image` fetch (`third_party/stb/`) over `SDL_image`. See `third_party/stb/README.md` and `docs/notes/7.1-wallpaper-decode.md`.

## Do not

- Placeholders / `__LOAD_FROM__` stubs
- Cloud Agents when unavailable; push via user-Github MCP
- Force-push main; work on beta then PR
