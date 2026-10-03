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
| 2026-10-02 | drawing-format | Keep `.modr` encoder/decoder bounds identical while preserving larger live canvases; oversized saves fail instead of creating files the decoder cannot reopen |
| 2026-10-02 | wallpaper-decode | Load PNG/JPEG directly from file after checking dimensions; reject inputs above 16,777,216 pixels before decoded allocations while preserving BMP loading |
| 2026-10-02 | drawing-input-bound | Check `.modr` file size before reading, stream within the format's encoded-size cap, and preserve canvas/prompt state on rejection |
| 2026-10-02 | session-line-bound | Read persisted session records with a 16 KiB line cap and stop restore at the first overlong record |
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
| 2026-10-02 | prompt-retry | Failed Text Editor and Drawing path, line-number, RGB, or file operations retain the active prompt, input, caret, and horizontal position for correction and retry |
| 2026-10-02 | same-file-reload | Text Editor and Drawing can reload their current file after an external overwrite; dirty documents retain the existing repeated-discard guard, with headless coverage |
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
| 7.56 | Restore same-file reload after external changes | done | Let Text Editor and Drawing reopen their already-bound path instead of self-focusing; preserve dirty-content confirmation and cover clean/dirty reloads in headless tests |
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
