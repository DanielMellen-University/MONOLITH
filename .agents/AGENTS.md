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
5. Mark chunk `done` and note debts. After 1.0 there is no next chunk: leave `CURRENT_CHUNK` (and `.agents/CURRENT_CHUNK`) at `1.0-released` unless a new chunk is agreed.
6. Prefer 3-5 logical commits on `beta`. Push with local git, wait for green headless CI on `beta`, then fast-forward `main` to the same commit with a plain push (no PR) and confirm `main` == `beta`.

## Current state snapshot

**CURRENT_CHUNK:** `1.0-released` (portfolio tag `v1.0.0`; no active chunk)

| When | Kind | Note |
|------|------|------|
| 2026-10-08 | 1.0-release-packaging | CMake, Settings About, and Terminal `version` report 1.0 (October 2026); README and CHANGELOG describe the portfolio ship; app set stays frozen |
| 2026-10-05 | snake-gameover-frame-allocation | Format the score/best overlay into a bounded stack buffer; a warmed game-over frame preserves the exact line and performs zero C++ heap allocations; full headless suite and Release build pass |
| 2026-10-05 | minesweeper-hud-frame-allocation | Format the live timer HUD and win-overlay time/best text into bounded stack buffers; warmed gameplay and win renders now allocate no C++ heap memory; full headless suite and Release build pass |
| 2026-10-05 | drawing-path-prompt-frame-allocation | Assemble Drawing status/caret text directly from path-buffer slices into retained strings; warmed active path-prompt rendering preserves caret metrics with zero C++ heap allocations |
| 2026-10-05 | filesystem-browser-status-frame-allocation | Reuse status render storage and append the visible item count, selected filename, and status message directly; warmed selected-row rendering now allocates no C++ heap memory |
| 2026-10-05 | terminal-scrollback-frame-allocation | Pass viewport-clipped history rows to the shared text cache as borrowed views; a warmed frame with a heap-sized clipped UTF-8 row now performs zero C++ heap allocations |
| 2026-10-05 | text-editor-find-frame-allocation | Pass visible syntax slices to the shared text cache as borrowed views and format Find match counts into a stack buffer; a warmed active Find frame preserves its count with zero C++ heap allocations |
| 2026-10-05 | start-menu-frame-allocation | Build Start-menu registry rows once; filter rows and actionable hit targets use bounded arrays; remove the unreachable placeholder-table tail; a warmed filtered menu frame now performs zero C++ heap allocations |

Older recent-work rows: [`SESSION_LOG.md`](SESSION_LOG.md).

## Known debts

- Atomic-save cleanup combines a bounded 16-parent first-touch/every-32-operations tracker with three alternate lexical aliases per parent, resumable 32-entry slices for synchronous save/listing sweeps, and a startup traversal. The duplicate unbounded whole-directory scanner is gone; traversal and cadence remain intentionally separate because their scheduling differs. Cleanup covers readable nested directories without blocking launch or turning one operation into an unbounded scan. Synchronous cursors and retry requests reuse tracked lexical paths without repeating canonical resolution, hold the parent lock for only one bounded slice, and release the process-wide tracker mutex before validating/reclaiming candidates; lock contention retries from the retained position, while enumeration failure restarts the pass. Startup maintenance borrows ordinary directory-entry paths and copies only cleanup candidates or child directories, defers root/child paths on `EMFILE`/`ENFILE` until active frames unwind, and skips other permission-denied or failed frames without losing readable siblings. The tracker shares cadence across symlink aliases and rescans an evicted directory on revisit. Sweeps read borrowed names from a POSIX directory stream and construct paths only for workspace candidates, avoiding per-entry path objects and an unbounded candidate list. Workspace and child-entry validation borrow basenames from existing paths instead of allocating filename copies. Scheduled save sweeps reuse the exclusive parent lock for following workspace setup. Startup and synchronous sweeps share candidate recognition, a lock-checked reclamation dispatcher, and one v4 ownership/type/ready classification. Reclamation opens candidates relative to the locked parent with `O_NOFOLLOW`, validates and removes known entries through the pinned workspace descriptor, and retains the original parent descriptor for failed-save rollback. Workspace setup, buffered output, permission recovery, sync, replace/no-replace rename, and rollback now remain anchored to retained parent/workspace descriptors; `O_PATH` recovers owner access when restrictive umask creates a mode-`000` directory. V4 setup and sweeps coordinate through a brief parent-directory lock; current symlink owner tokens require the strict random-name format, while validated regular markers remain supported for older v4 writers. Incomplete v4 workspaces require a valid ownership token, with the pre-marker crash remnant recoverable only by its strict 128-bit random name and empty-directory removal. Marked v4/v3/v2 workspaces require a ready marker and free regular-file lease lock. Cleanup removes only recognized entries with expected types and preserves any workspace containing unknown entries; incomplete older workspaces remain untouched.
- Text Editor and Drawing detect ordinary host/other-process changes to a bound file by checking its file stamp and comparing content exactly when the stamp changes. Bound saves check the expected stamp again immediately before atomic replacement and preserve changes arriving during content generation. Expected-absent targets use no-replace rename; final validation and publication serialize against other Monolith atomic writers through the parent-directory lock. The final check verifies the visible parent still identifies the pinned publication directory and compares the entry relative to that descriptor, preventing a parent-alias retarget from splitting validation and publication. Host tools and uncoordinated filesystem mutations do not honor the lock and can still race after the final check.

## Priority order for "next / continue"

1. No required chunk after the 1.0 tag. Post-1.0 polish only when a concrete debt is listed.
2. Custom language and IDE stay in the separate MONOLITH 2 repository. Do not start them in this tree.
3. Keep `wip/7.52-drawing-modr-io` parked.

**Cut order if scope tight:** never cut core shell work for another game.

## 1.0 release

| ID | Chunk | Status | Deliverable / exit criteria |
|----|--------|--------|-----------------------------|
| 1.0-docs-retarget | Planning docs | done | Roadmap and checklist; language called out as MONOLITH 2 |
| 1.0-stability-audit | Stability bar | done | Headless and sanitizer green; host-writer race documented only |
| 1.0-surface-freeze | App freeze | done | Current apps only; drawing WIP stays parked |
| 1.0-release-packaging | Version and notes | done | 1.0 strings, README, CHANGELOG; tag `v1.0.0` |

## Phase 4 - Living-inside-it

| ID | Chunk | Status | Deliverable / exit criteria |
|----|--------|--------|-----------------------------|
| 4.1 | Terminal quoted arguments | done | `cat "/home/monolith/my file.txt"` and similar work; doc the quoting rules in `docs/apps/terminal.md` |
| 4.2 | Settings font / UI scale | done | Settings persists 90%, 100%, or 115% shared interface text size and applies it live; README stays one-line |
| 4.3 | Editor wrap or horizontal scroll | done | Long lines remain editable with cursor-following horizontal scroll and Shift + wheel panning; `docs/apps/text-editor.md` updated |
| 4.4 | Drawing eyedropper | done | Pick samples a canvas pixel into custom RGB without changing pixels or undo history; still saves `.modr` |

## Phase 7 - Growth

| ID | Chunk | Status | Deliverable / exit criteria |
|----|--------|--------|-----------------------------|
| 7.1 | PNG/JPEG wallpaper | done | stb_image build-time fetch; BMP via SDL_LoadBMP; PNG/JPEG via WallpaperImage |
| 7.2 | Fourth game (Breakout) | done | Same Start -> Games pattern + `verify_games_integration.sh` |
| 7.3 | Desktop icons | done | Left-column icons under wallpaper; windows win hit-testing; see `docs/internals/desktop-shell.md` |
| 7.4 | App registry | done | `AppRegistry` drives Start menu, desktop icons, session kinds; see `docs/internals/desktop-shell.md` |
| shell-polish | Start menu hit unify | done | Hit targets rebuild from AppRegistry rows; hardcoded entries table retired; see `docs/internals/desktop-shell.md` |
| 7.5 | Wallpaper fit | done | cover/contain/center display modes; see `docs/internals/wallpaper.md` |
| 7.6 | Start menu type-ahead | done | filter by label while open; see `docs/internals/desktop-shell.md` |
| 7.7 | Multi-column desktop icons | done | Fill extra columns left-to-right when height is short; see `docs/internals/desktop-shell.md` |
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
| 7.152 | Bound opportunistic atomic-save sweeps | done | Inspect at most 32 directory entries per save/listing sweep step; retain resumable per-parent cursors and reacquire the parent lock for each slice; ensure large mixed directories are eventually reclaimed without deleting ordinary entries |
| 7.153 | Report fully blocked Browser pastes accurately | done | Distinguish a failed copy from a failed move in status text, preserve fully blocked cut clipboard entries for retry, and verify that conflicting source/destination files remain untouched |
| 7.154 | Keep startup cleanup nonblocking | done | Probe candidate parent locks nonblocking, resume a contended candidate from the retained directory iterator on a later frame, and defer candidate parents on descriptor exhaustion until active frames unwind; verify both retry paths |
| 7.155 | Paste into the active Browser filter | done | Route Ctrl+V to sanitized system text only while filter editing is active; preserve file paste afterward and cover caret insertion, control filtering, exact-fit UTF-8, and partial-character rejection |
| 7.156 | Detect unannounced changes before bound saves | done | Skip content reads when the bound-file stamp is unchanged; stream-compare changed versions against Text Editor/Drawing baselines, preserve divergence behind Ctrl+D/Esc, normalize Editor line endings, and document the remaining check-to-replace race |
| 7.157 | Recheck bound-file versions before replacement | done | Add conditional atomic writes with a final stamp check; guard Text Editor/Drawing bound saves, preserve newer files on staged-time conflicts, retry via Ctrl+D, document the remaining final check-to-rename race, and pass hosted run #452 |
| 7.158 | Preserve creates racing an absent-target save | done | Publish expected-absent conditional writes with Linux no-replace rename; report a concurrent create as Conflict and prove the target survives a last-instant race; hosted run #454 passed |
| 7.159 | Abort atomic writes on callback failure | done | Honor boolean callback failure in the shared atomic writer while preserving void callbacks; verify the old target remains intact, staged workspace is removed, and success callbacks still commit; hosted run #456 passed |
| 7.160 | Serialize atomic file publication | done | Hold the destination parent lock across final conditional validation and rename, serializing competing Monolith atomic writers; cover lock ownership and retain external no-replace coverage; production build, full headless suite, and hosted run #458 passed both jobs |
| 7.161 | Verify cross-process conditional writes | done | Stage two independent processes against one file stamp, release together, and require exactly one `Written` and one `Conflict`; focused Filesystem suite and hosted run #460 passed both jobs |
| 7.162 | Complete multiline Text Editor Find and Replace | done | Seed multiline selections; match, navigate, select, and replace across row boundaries; normalize pasted line endings; provide Ctrl+Enter and row-fragment highlights with live active state; enforce 16 MiB/65,536-line field limits; focused and full headless suites, production build, hosted run #462 verify/sanitize passed |
| 7.163 | Bound multiline Find scan cost | done | Use KMP over exact middle-row query segments with boundary-only checks, preserving overlapping-prefix fallback, empty boundary segments, non-overlapping ranges, and sparse checkpoints; focused/full headless suites, production build, hosted run #464 verify/sanitize passed |
| 7.164 | Stress multiline Find matching | done | Differentially compare ranges against an independent reference across 14,040 small documents and queries; exercise the full 65,536-row bound with a long near-match; focused/full headless suites, production build, and hosted run #467 passed both jobs |
| 7.165 | Sync atomic-save data before publication | done | Fsync staged file contents and restored permission bits before rename; attempt parent-directory fsync after publication without misreporting an already-completed rename; cover mode-000 new files under umask 0777; full headless suite, production build, hosted run #470 passed both jobs |
| 7.166 | Pin atomic-save permission recovery | done | Pin staged regular files with `O_PATH|O_NOFOLLOW`, adjust mode through the pinned inode with an older-kernel fallback, then verify the writable no-follow descriptor before syncing; cover a replaced staged pathname pointing outside plus mode-000 saves; focused/full headless suites, production build, hosted run #475 verify/sanitize passed |
| 7.167 | Remove the unbounded atomic cleanup path | done | Delete the duplicate whole-directory scanner and make tests complete cleanup through the production 32-entry resumable cursor, reacquiring the parent lock per slice; cover a large batch and ordinary-entry preservation; focused/full headless suites, production build, hosted run #478 verify/sanitize passed |
| 7.168 | Release tracker mutex before cleanup I/O | done | Copy each slice's bounded workspace candidates while advancing its cursor, then validate/reclaim those candidates after releasing the global sweep-tracker mutex; retain the destination-directory lock through cleanup; focused/full headless suites, production build, and hosted run #481 verify/sanitize passed |
| 7.169 | Centralize atomic cleanup tracker state | done | Share lexical/physical slot lookup and bounded-slot registration across due checks, retries, and cursor steps; reuse known lexical aliases on retry instead of canonicalizing again; focused/full headless suites, production build, and hosted run #484 verify/sanitize passed |
| 7.170 | Refresh retargeted cleanup aliases | done | Compare cached alias and tracked-parent directory identities outside the global mutex; detach stale spellings and associate a retargeted symlink with the correct first-touch cleanup cadence; focused/full headless suites, production build, and hosted run #487 verify/sanitize passed |
| 7.171 | Share atomic cleanup candidate recognition | done | Use one v2-v4 workspace-name predicate in startup DFS and synchronous directory sweeps; retain each path's existing bounded traversal and locking semantics; focused/full headless suites, production build, and hosted run #490 verify/sanitize passed |
| 7.172 | Share atomic cleanup reclamation | done | Dispatch synchronous sweep candidates through the shared parent-lock-checked workspace reclaimer already used by startup DFS; retain distinct bounded traversals and scheduling; focused/full headless suites, production build, and hosted run #493 verify/sanitize passed |
| 7.173 | Classify atomic cleanup candidates once | done | Share one v4 owner/directory/ready-marker classification before routing incomplete workspaces or lease-protected stale workspaces; preserve strict marker, lease, and unexpected-entry checks; focused/full headless suites, production build, and hosted run #496 verify/sanitize passed |
| 7.174 | Compile atomic-save implementation once | done | Move non-template atomic-save, cleanup, locking, and sync implementations out of the 900-line header into `AtomicFile.cpp`; retain API declarations and writer templates in the header and wire production plus standalone tests; production build, full headless suite, and hosted run #499 verify/sanitize passed |
| 7.175 | Anchor atomic cleanup to directory descriptors | done | Open each candidate relative to its locked parent with `O_NOFOLLOW`; validate and remove known children through the pinned directory FD, retain the originating parent FD for failed-save rollback, and test alias retargeting against a valid-looking same-name trap; focused/full headless suites, production build, and hosted run #502 verify/sanitize passed |
| 7.176 | Anchor atomic staging through publication | done | Create workspace entries relative to the locked parent; retain workspace/parent FDs through buffered output, permission recovery, file sync, atomic rename, and rollback; test alias retargeting and large binary writes; focused atomic suite, full headless suite, production build, and hosted run #505 verify/sanitize passed |
| 7.177 | Preserve atomic output seeking | done | Keep `seekp`/`tellp` behavior on the descriptor-backed writer by flushing buffered bytes before repositioning; cover in-place overwrite and stream positions with a regression, focused atomic suite, production build, and hosted run #509 verify/sanitize passed |
| 7.178 | Narrow conditional atomic publication window | done | Prepare destination rename operands before the final expected-stamp callback so only single-component validation and the rename syscall follow it; document that uncoordinated host writers can still race; focused/full headless suites, production build, and hosted run #512 verify/sanitize passed |
| 7.179 | Anchor conditional validation to publication parent | done | Verify the parent pathname still identifies the pinned publication directory and compare the expected target stamp through that descriptor; reject parent-symlink retargets during content generation; full headless suite, production build, and hosted run #515 verify/sanitize passed; retain the documented race against host writers after validation |
| 7.180 | Apply final window drag and resize positions | done | Commit mouse-up pointer coordinates before clearing active gesture state so motion and release batched before `update()` do not drop the last frame; preserve taskbar clamping and resize notifications; full headless suite, production build, and hosted run #518 verify/sanitize passed |
| 7.181 | Finalize frame gestures on host focus loss | done | Use the same final drag/resize commit path for mouse-up and host-focus loss; apply last known pointer geometry, preserve clamping and resize notifications, and cover focus regain; full headless suite, production build, and hosted run #521 verify/sanitize passed |
| 7.182 | Enforce text texture cache byte budget | done | Preflight text dimensions and reject individual estimated RGBA rasters above 16 MiB before allocation; verify oversized labels cannot exceed the LRU byte budget; full headless suite, Release build, focused ASan/UBSan, and hosted run #524 verify/sanitize passed |
| 7.183 | Share app and shell text caches | done | Route Window Manager labels through the renderer-aware TextTextureCache, remove duplicate cache/eviction code, avoid heap allocations on warmed long-label hits, and include text/bookkeeping in estimated bytes; full headless suite, Release build, and hosted run #527 verify/sanitize passed |
| 7.184 | Remove desktop icon frame allocations | done | Replace per-frame registry and placement vectors with fixed-capacity arrays; a warmed Window Manager frame with a long Alt+Tab title now performs zero C++ heap allocations; focused coordinate and icon tests pass |
| 7.185 | Remove Start-menu frame allocations | done | Build registry rows once and use bounded arrays for filtered output and action hit targets; remove dead placeholder-table code; warmed filtered Window Manager rendering performs zero C++ heap allocations |
| 7.186 | Remove Text Editor Find frame allocations | done | Pass syntax slices to the text cache as borrowed views and stack-format active match counts; a warmed Find frame retains its displayed count with zero C++ heap allocations; full headless suite and Release build pass |
| 7.187 | Remove Terminal scrollback frame allocations | done | Pass clipped history rows directly to the text cache as borrowed views; a warmed frame with an actually clipped UTF-8 row performs zero C++ heap allocations; focused Terminal state suite passes |
| 7.188 | Remove Filesystem Browser status frame allocations | done | Keep status text in retained storage and append the item count, selected name, and feedback directly; a warmed selected-row frame preserves the filename with zero C++ heap allocations; focused browser state suite passes |
| 7.189 | Remove Drawing path-prompt frame allocations | done | Assemble status and caret text directly from prompt-buffer slices into retained strings; a warmed active path-prompt frame preserves caret metrics with zero C++ heap allocations; focused Drawing state suite passes |
| 7.190 | Remove Minesweeper HUD and overlay frame allocations | done | Format live timer and win-overlay time/best labels into bounded stack buffers; warmed gameplay HUD and win-overlay renders perform zero C++ heap allocations and preserve exact text; full headless suite and Release build pass |
| 7.191 | Remove Snake game-over overlay frame allocations | done | Format the score/best line into a bounded stack buffer; warmed game-over rendering performs zero C++ heap allocations and preserves exact text and spacing; full headless suite and Release build pass |
| 7.192 | Remove Text Editor Find geometry rebuild allocations | done | Measure visible prefixes and matches with a 4 KiB stack buffer, retaining temporary fallback only for exceptionally large slices; isolated geometry-rebuild regression, full headless suite, and Release build pass |
| 7.193 | Batch Snake checkerboard rendering | done | Cache the 200 alternating-cell rectangles until board geometry changes and submit them in one SDL batch per frame; pixel checks preserve tile colors and cover resize rebuilds; full headless suite and Release build pass |
| 7.194 | Reuse Text Editor syntax spans while scrolling | done | Shift cached span vectors for same-sized overlapping viewports so scrolling retokenizes only newly exposed rows; bidirectional overlap-storage and allocation regression, full headless suite, and Release build pass |
| 7.195 | Batch Breakout brick rendering | done | Cache brick screen rectangles while playfield geometry is unchanged and batch alive bricks by row color; software-render pixel regression covers palette, cleared-brick gaps, and resize rebuilds; full headless suite and Release build pass |
| 7.196 | Isolate atomic output buffering | done | Move the descriptor-backed buffered/seekable stream implementation out of atomic workspace lifecycle code into `AtomicTempOutput.cpp`; preserve large-write, seek, failure, and publish behavior; focused/full headless suites and Release build pass |
| fs-browser-filter-cache | Reuse Filesystem Browser filter snapshots | done | Reuse one directory listing for filter edits; refresh it on F5 and Monolith filesystem notifications, including while filtering |
| terminal-input-editing | Terminal command-line selection | done | Support UTF-8-safe keyboard/mouse selection and Ctrl+A/C/X/V; normalize all clipboard lines and tabs into the single-line prompt |
| terminal-touch-mtime | Complete Terminal touch semantics | done | Update existing regular-file last-write time without truncation; reject outside-root symlink targets |

## Commit voice

`feat|fix|docs|chore(scope): imperative`. No em dashes / AI filler.

## Wallpaper dependency (7.1)

Prefer pinned `stb_image` fetch (`third_party/stb/`) over `SDL_image`. See `third_party/stb/README.md` and `docs/internals/wallpaper.md`.

## Do not

- Placeholders / `__LOAD_FROM__` stubs
- Cloud Agents when unavailable; pushes through an MCP connector (push with local git; `gh` is the credential helper)
- Force-push or reset `main`/`beta`; land work on `beta` first, then fast-forward `main` once CI is green (no PR)
