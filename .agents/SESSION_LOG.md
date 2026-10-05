# Session log

| 2026-10-04 | refactor/filesystem | Route synchronous atomic-save sweep candidates through the same parent-lock-checked workspace-reclamation dispatcher as startup maintenance, keeping traversal budgets and scheduling distinct; focused/full headless suites, production build, and hosted run #493 `verify`/`sanitize` pass. |

| 2026-10-04 | refactor/filesystem | Share v2-v4 atomic-workspace name recognition between startup DFS and synchronous per-directory sweeps; focused/full headless suites, production build, and hosted run #490 `verify`/`sanitize` pass. |

| 2026-10-04 | fix/filesystem | Cached sweep aliases now validate against the tracked directory identity outside the global mutex. Retargeted symlinks are detached from the old parent and their new physical target receives an immediate first-touch sweep; focused/full headless suites, production build, and hosted run #487 `verify`/`sanitize` pass. |

| 2026-10-04 | refactor/filesystem | Centralize bounded sweep-tracker slot lookup and registration; retry requests reuse known lexical aliases without canonical resolution. Focused atomic-file suite, full headless suite, production build, and hosted run #484 `verify`/`sanitize` pass. |

| 2026-10-04 | perf/filesystem | Release the process-wide sweep tracker mutex after advancing at most 32 directory entries and collecting workspace paths. Candidate validation/reclamation now runs outside that mutex while the per-parent lock remains held; focused atomic-file suite, full headless suite, production build, and hosted run #481 `verify`/`sanitize` pass. |

| 2026-10-04 | refactor/filesystem | Remove the unused unbounded atomic-temp directory scanner. Large recovery tests now advance the production 32-entry cursor across separately locked slices; a 128-workspace batch preserves ordinary siblings, and recovery/retry fixtures pass. Focused atomic-file suite, full headless suite, production build, and hosted run #478 `verify`/`sanitize` pass. |

| 2026-10-04 | fix/data-safety | Pin staged atomic-save inodes with `O_PATH|O_NOFOLLOW`; change temporary/final permissions through that inode, then verify the writable no-follow descriptor matches before syncing. A deterministic path-swap regression proves external symlink targets keep their mode, while restrictive-umask/mode-000 saves still pass. Focused atomic-file suite, full headless suite, production build, and hosted run #475 `verify`/`sanitize` pass; the separate host-writer check-to-replace race remains documented. |

| 2026-10-04 | test/data-safety | Cover a staged content path replaced with a symlink before sync: publication is rejected, the old destination and external target remain unchanged, and cleanup preserves the unexpected link until explicitly removed. Focused atomic-file test, full headless suite, and production build pass locally. |

| 2026-10-04 | fix/data-safety | Atomic publication now fsyncs staged file data and restored permissions before rename, aborting without replacing the target on file-sync failure. It attempts to sync the destination directory after rename; an error there cannot undo publication and does not report a false failed write. Added a mode-000 new-file regression under umask 0777; full headless suite, production build, and hosted run #470 `verify`/`sanitize` passed. |

| 2026-10-04 | test/editor | Added 14,040 exhaustive small-case comparisons between multiline Find and a straightforward reference matcher, plus a 65,536-row/32,768-row near-match stress case. Focused/full headless suites and production build pass; hosted run #467 passed `verify` and `sanitize`. |

| 2026-10-04 | perf/editor | Replaced per-candidate multiline query rechecks with KMP over exact middle-row segments and first/last-row boundary validation. Added overlapping-prefix and empty-boundary regressions; focused editor tests, full headless suite, production build, and hosted run #464 `verify`/`sanitize` passed. Commits `0b1e7e8` and `49d4787` are on `beta`. |

| 2026-10-04 | fix/editor | Text Editor Find/Replace now seeds multiline selections, matches and selects row-spanning ranges, accepts normalized multiline query/replacement input, and supports one-step Replace All undo. Added Ctrl+Enter, visible escaped newlines, bounded row-fragment highlighting with active styling that updates without geometry rebuilds, and maintained byte/line counters. Focused editor suite, full headless suite, production build, and hosted run #462 `verify`/`sanitize` passed. |

| 2026-10-04 | test/data-safety | Added a process-level regression for two conditional saves staged from the same version; verifies that final publication produces one `Written` and one `Conflict` with no staging workspaces left behind. Focused normal and ASan/UBSan checks pass; local LeakSanitizer shutdown is unavailable in this ptrace environment, while hosted run #460 passed leak-enabled `verify` and `sanitize`. |

| 2026-10-04 | fix/data-safety | Atomic writers now hold the destination parent lock across final version validation and publication, so competing Monolith atomic writers cannot replace one another between check and rename. Host tools and uncoordinated mutations remain outside the advisory lock. Production build, full headless suite, and hosted run #458 `verify`/`sanitize` pass. |

| 2026-10-04 | fix | Shared atomic writes now abort on a callback's explicit `false` result instead of publishing partial output; void callbacks remain compatible. Preservation, cleanup, and success coverage pass with the production build and hosted run #456 verify/sanitize. |

| 2026-10-04 | fix | Conditional writes for absent files now publish with `RENAME_NOREPLACE`, preserving a host/other-process create that lands after the last stamp check; the atomic-writer race regression, production build, and hosted run #454 verify/sanitize pass. Existing-file final check-to-rename limitation remains. |

| 2026-10-04 | fix | Text Editor and Drawing now recheck bound-file stamps immediately before atomic replacement. Conflicting file and symlink changes during content generation are preserved and prompt for Ctrl+D again. Production build, full headless suite, and hosted run #452 verify/sanitize pass; the residual final stamp-check-to-rename race is documented. |

| 2026-10-04 | fix/data-safety | Text Editor and Drawing now compare bound files with their loaded/saved baselines before saving, catching host edits and other-process changes without shell notifications. Unchanged file stamps avoid rereading; divergence uses Ctrl+D/Esc; Editor normalizes CRLF/CR and Drawing compares `.modr` bytes in chunks. Production build, full headless suite, and hosted runs #450 verify and sanitize pass. |
| 2026-10-04 | fix/ux | Ctrl+V now pastes system clipboard text at the Filesystem Browser filter caret only while the filter is being edited; outside that mode it still pastes files. Typed and pasted query input share control filtering and the 255-byte UTF-8 limit, with coverage for caret insertion and exact/partial multibyte capacity. Production build, full headless suite, and hosted runs #448 verify and sanitize pass. |
| 2026-10-04 | fix/data-safety | Require Ctrl+D to overwrite an externally changed bound file from Text Editor or Drawing; Esc keeps the external version, repeated saves cannot bypass the decision, and save-before-close continues only after the confirmed write succeeds. Updated app, architecture, filesystem, and changelog documentation; production build, full headless suite, and hosted run #446 pass. |
| 2026-10-04 | fix/ux | Text Editor and Drawing now retain an external-change status marker across routine status updates until reload, successful save, or unbinding. Lifecycle and rendered-status regressions, the full local headless suite, hosted `verify`, and hosted `sanitize` run #444 pass. |

| 2026-10-04 | fix/perf | Startup stale-workspace cleanup now probes candidate parent locks nonblocking so another instance cannot freeze the UI frame. Contention keeps the current iterator entry for the next frame; descriptor exhaustion defers the parent for retry after traversal frames unwind. Added held-lock and candidate-parent descriptor-pressure regressions and updated filesystem architecture/process notes. |

| 2026-10-04 | fix | Fully blocked Browser cut paste now reports that nothing moved rather than saying nothing was copied; the failed cut stays retryable. Added all-conflict copy and cut state regressions verifying both files and clipboard state are preserved; documented the status behavior. |

| 2026-10-04 | fix | Report completed-versus-selected counts for partial Filesystem Browser copy and cut paste; add state tests for conflict-skipping cases and document the status behavior. |

| 2026-10-04 | fix/perf | Preserve startup cleanup progress when root or child iterator opens reach `EMFILE`/`ENFILE`: defer the directory path and retry under a per-step open budget. Cover root initialization and a deep branching tree under descriptor pressure; document the retry behavior. |

| 2026-10-04 | perf | Startup workspace maintenance now borrows each ordinary path from its `directory_entry` and copies paths only for a cleanup candidate or real child directory, avoiding one full path copy per scanned filesystem entry without changing its 32-entry frame budget. |

| 2026-10-04 | perf | Retain three alternate lexical spellings per tracked atomic-save parent so multiple symlink aliases avoid repeat canonicalization while sharing the same cleanup cadence. Expanded the regression to rotate through three aliases of one physical directory. |

| 2026-10-04 | perf | Reuse tracked lexical paths when advancing atomic-save cleanup cursors, avoiding a canonical path walk on every slice while still resolving new aliases into the shared parent cadence. Extend the mixed-directory regression to alternate between canonical and symlink paths; update filesystem/architecture docs. |

| 2026-10-04 | perf | Bound opportunistic atomic-save cleanup to 32 directory entries per save/listing operation. Large directory scans now resume from a retained POSIX stream cursor, reacquiring the parent lock only while advancing each slice; lock/enumeration failures retry immediately. Added mixed-directory stress coverage proving the first pass is bounded, later passes reclaim all stale workspaces, and ordinary entries remain intact. Updated filesystem and architecture docs; the full local headless/sanitizer suite and production build pass. |

| 2026-10-04 | fix | Replaced Drawing's dirty Close/New/Open double-confirm with explicit Ctrl+D discard, Ctrl+S save-before-Close/New, and Esc cancel choices. Untitled Close supports Save As; pending actions finish only after success; Ctrl+S during dirty Open saves the current sketch and cancels that Open. Added regressions for same-file reload, changed targets, repeated actions, cancellation, save failures, and save continuations; updated the Drawing guide and architecture contract. |

| 2026-10-04 | fix | Replaced Text Editor dirty Close/Open double-confirm with explicit Ctrl+D discard, Ctrl+S save, and Esc cancel choices. Close-save now closes only after a successful write, including Save As for untitled text; global shutdown stays blocked until dirty Editors are explicitly resolved. Repeating Close/Enter cannot discard; Open target changes require a fresh decision. Added controller, Escape, save failure, same-file reload, repeated-action, and changed-target regressions; updated the Text Editor guide and changelog. |

| 2026-10-04 | fix | Replace the check-then-rename sequence with Linux `renameat2(RENAME_NOREPLACE)`, closing the race that could overwrite a destination created concurrently. Regular and dangling-symlink conflicts preserve both entries; unsupported host filesystems fail closed. Production build, full headless suite, final focused Filesystem test, and focused Filesystem/Terminal ASan/UBSan tests pass. |

| 2026-10-04 | fix | Filesystem rename, batch move, and Terminal `mv` now operate on a final outside-target symlink as an entry after validating both parents, while paths through outside-pointing parent symlinks remain rejected. Filesystem and Terminal regressions cover target preservation, containment, and destination/missing-source behavior. |

| 2026-10-04 | fix | Terminal `rm` now attempts Filesystem's parent-contained delete before checking target visibility, allowing safe removal of final symlinks whose targets are hidden outside the virtual root. Headless command coverage verifies file links, directory links with `-r`, and rejection through an outside parent symlink; focused Terminal tests, full headless suite, production build, and ASan/UBSan pass. |

| 2026-10-04 | fix | Resolve and contain the parent of each filesystem deletion target before removing its final entry. Regression tests reproduced and now prevent `remove()` and `removeRecursive()` from unlinking files through an outside-pointing parent symlink; direct final-symlink unlinking remains supported. |

| 2026-10-04 | cleanup/perf | Shared directory-entry visibility and symlink-containment validation between `list()` and `listEntries()`, and resolve the configured host root once per listing rather than once per directory or symlink. The focused filesystem roadmap suite, full headless matrix, production build, and focused ASan/UBSan suite pass. |

| 2026-10-04 | perf | Keep complete Find/Replace strings while rendering only 64-byte UTF-8-safe context around each caret. Highlights measure visible match slices and start from sparse checkpoints, avoiding giant off-screen measurements, dense-prefix rescans, and overlapping viewport-only false hits. Maximum-size field coverage passes. |

| 2026-10-04 | fix | Bound each Text Editor Find/Replace field to 16 MiB, preserving complete UTF-8 codepoints and reporting rejected overflow. Added exact-fit typing and over-limit typing/paste regressions for Find and Replace. |

| 2026-10-04 | fix | Startup atomic-save traversal now uses explicit directory frames; an unreadable or failed subtree no longer aborts cleanup for readable siblings. The 32-entry per-frame budget, ownership checks, and no-follow behavior remain intact. Added inaccessible-subtree and sibling-recovery coverage. |

| 2026-10-04 | fix | Atomic-save recovery now continues across the host filesystem after launch in 32-entry Window Manager maintenance steps, so a crash workspace in an untouched nested directory no longer waits for the user to revisit that path. Candidate removal still uses parent locking and strict workspace validation; symlink targets and unknown workspace contents are preserved. Added bounded-progress, nested-recovery, and symlink regressions. |

| 2026-10-04 | fix | Directory listings now share the bounded atomic-save sweep tracker with writes. Revisited directories reclaim abandoned workspaces without requiring another save; `list()` and `listEntries()` are both covered. |

| 2026-10-04 | fix | Atomic-save scavenging now locks older read-only or write-only lease files using whichever owner access mode remains available. Regressions cover both stale access modes and confirm an active read-only lease lock prevents reclamation. |

| 2026-10-04 | fix | Normalize atomic-save workspace permissions before publishing the owner token, and force lease files to owner read/write so cleanup can reopen them after a crash. A full save now passes under `umask(0777)` with private directory and lease modes; removed the resolved private-creation note from active debts. |

| 2026-10-04 | fix | Atomic-save staging directories are now created owner-only at the initial `mkdir`, removing the brief permissive setup window; permission normalization still handles restrictive umasks. Added a live-workspace permission regression and documented the behavior. |

| 2026-10-04 | test | Added an end-to-end Drawing state regression for Ctrl+O, typed filename prefix, Tab completion, and Enter through `handleEvent`; this protects the actual workflow rather than only testing the completion helper. Updated the developer verification guide. |

| 2026-10-04 | fix | Ctrl+F and Ctrl+H now use a selected single-line term as the query and begin on that occurrence, including reversed selections; multi-line and control-bearing selections stay out of the single-line field. Added headless event-path regressions and updated the Text Editor guide. |

| 2026-10-04 | cleanup | Unified typed and clipboard input for Find/Replace fields behind one sanitizer/insertion routine; clipboard allocations now release through RAII, and tests exercise both input routes. |

| 2026-10-04 | fix | Ctrl+V now inserts clipboard text at the active Find or Replace caret, filters controls to preserve single-line prompts, and refreshes matches after query pastes. Headless regressions cover UTF-8, caret placement, and both fields. |

| 2026-10-04 | perf | Reused a bounded 4 KiB Text Editor prefix scratch buffer for cursor/selection width measurement, avoiding per-frame prefix string allocations on ordinary lines without retaining storage proportional to extreme line lengths. Added exact-width, capacity-reuse, and oversized-prefix regressions. |

| 2026-10-04 | perf | Atomic workspace cleanup now reads owner, lease, ready, and content names as views into each entry path, removing a child-basename string copy from every save. The atomic-save suite still covers marker validation and preservation of unexpected data. |

| 2026-10-04 | perf | Atomic workspace validation and cleanup now inspect borrowed basenames from each path's native storage, avoiding repeated `filename().string()` allocations; tests cover relative, trailing-separator, and root paths. |

| 2026-10-04 | perf | Text Editor Find/Replace and path prompts now assemble display and caret text in retained buffers, avoiding per-frame substring and concatenation temporaries; unchanged Find/Open prompt tests verify text and capacity stability. |

| 2026-10-04 | perf | Scheduled atomic-save sweeps now reuse the destination-directory lock through the following workspace setup, removing a redundant parent open/flock cycle; bulk sweep coverage verifies the lock remains held. |

| 2026-10-04 | perf | Text Editor rendering now reuses its status-bar string buffer and appends a borrowed display-name view instead of building an owning substring each frame; an unchanged-frame regression verifies stable buffer capacity. |

| 2026-10-04 | perf | Atomic-save sweeps now use `readdir` names directly and build paths only for workspace candidates, avoiding per-entry path materialization for ordinary siblings. Directory-open and enumeration failures still schedule the next-write retry. |

| 2026-10-04 | perf | Atomic-save parent sweeps now inspect basenames through borrowed views and copy a path only for workspace candidates, avoiding path and name allocations for ordinary siblings. Mixed-directory recovery retains 128 ordinary files while reclaiming 128 stale workspaces. |

| 2026-10-04 | polish | When Browser filter input hits its UTF-8 byte cap, the status bar now explains the limit and match count; the notice survives Enter/re-entry and F5, and clears on query edits. Added focused state regressions. |

| 2026-10-04 | fix/perf | Bounded the Filesystem Browser's horizontally scrolled filter text to 255 UTF-8 bytes, matching the standard Linux filename-component limit so user input cannot produce unbounded cached text textures. Added boundary coverage for a four-byte codepoint at the limit. |

| 2026-10-04 | perf | Filesystem Browser now narrows the prior ordered source-index list in place when the filter query is appended, avoiding a full directory scan and match-vector allocation for each progressively longer query. Backspaces, interior edits, and snapshot reloads still perform a full scan; focused filter-state, filesystem-index, full headless, and build checks pass. |

| 2026-10-04 | fix | UTF-8 Backspace and Delete now clamp stale cursor offsets that land inside a multibyte codepoint before erasing, preventing either shared helper from splitting the encoded character. Added focused regressions for both directions. |

| 2026-10-04 | cleanup | Consolidated duplicated forward-Delete byte slicing into `eraseNextUtf8Codepoint`, used by the Terminal input/reverse-search and the Text Editor, Drawing, and Filesystem Browser prompts. Added unit coverage for multi-byte deletion and an out-of-range cursor no-op. |

| 2026-10-04 | perf | Atomic-save sweeps now process matching directories incrementally instead of allocating a vector sized to every stale workspace. A focused batch of 128 interrupted workspaces and the full headless suite passed. |

| 2026-10-04 | fix | Require the current symlink owner marker to live inside a strict random-token workspace name, so a prefixed lookalike cannot copy the fixed marker and have its fully populated contents scavenged; retain validated regular-file markers for legacy workspaces. Full local headless suite and Release build passed, the focused ASan/UBSan test passed, and hosted run #378 passed both jobs. |

| 2026-10-04 | fix | Atomic-save sweep attempts now report lock/enumeration failure and schedule a retry on the next write rather than consuming the full 32-write interval. Added cadence coverage plus current-format symlink-owner stale recovery coverage. Full local headless suite, Release build, focused ASan/UBSan test, and hosted run #376 both jobs passed. |

| 2026-10-04 | fix | Atomic-save teardown and scavenging now remove only validated workspace entries and use `rmdir` for final cleanup, preserving unexpected user data in both marked-ready and marked-incomplete workspaces. Require regular lease files before opening; add regressions for stale recovery, active-save cleanup, FIFO lease lookalikes, and retained v3/v2 recovery. Full local headless suite and Release build passed; full ASan/UBSan headless suite passed with LeakSanitizer disabled for the sandbox; hosted run #374 passed both jobs. |

| 2026-10-04 | polish | Replaced the Text Editor's outdated welcome disclaimer with current Find/Replace shortcut guidance and covered the unchanged clean baseline. |

| 2026-10-04 | fix | Switched unpublished atomic-workspace rollback to `rmdir` semantics and shared that rule with pre-marker scavenging; unexpected entries now survive while empty setup remnants are reclaimed. |

| 2026-10-04 | perf | Counted normalized Text Editor bytes while parsing streamed input, removing the post-open and constructor rescans; covered empty, CRLF, lone-CR, chunk-boundary, and maximum-line documents. |

| 2026-10-04 | fix | Enforced Text Editor's 16 MiB / 65,536-line limits on typing, Enter, paste, Replace, and Replace All; maintained serialized-byte accounting incrementally, bounded clipboard normalization, and added exact-boundary/rejection tests. |

| 2026-10-04 | fix | Minesweeper difficulty changes now request a board-sized window through `IWindowController`; WindowManager centers and clamps it, retains the preferred restore rectangle while maximized, and reports applied geometry back to the app. Same-difficulty restarts preserve manual sizing. Added headless shell integration coverage for difficulty switches, maximize/restore, and tiny desktops. |

| 2026-10-04 | test safety | Replace fixed and PID-derived headless fixture roots with scoped `mkdtemp` directories; stale paths can no longer be deleted after PID reuse, and parallel test runs cannot collide. Harden the Drawing smoke script to use the app's actual `$HOME/.monolith/fs` without clearing shared paths. Add focused lifecycle coverage and document the convention. Full headless suite and production build pass locally. |

| 2026-10-04 | fix | New atomic-save workspaces use 128-bit OS-random names, making the directory name available as a strict pre-marker recovery token. Cleanup removes such an incomplete workspace only through empty-directory removal, so user-added data makes reclamation fail closed; marked and legacy workspace handling is unchanged. Added token-format, empty-orphan, and nonempty-lookalike coverage. |

| 2026-10-04 | fix | Atomic-save ownership tokens now publish as a symlink in one filesystem operation, eliminating partially written marker files while retaining validated regular markers for older v4 workspaces. Focused regression failed before the change and passes afterward; FIFO/lookalike cleanup coverage remains green. The only remaining orphan window is between workspace-directory creation and token publication. |

| 2026-10-04 | fix | Successful moves now invalidate the wallpaper failed-load cache when they supply the configured image path (or a containing directory), so the next render retries it. Added a WindowManager regression for moving into a previously missing configured path, clarified Terminal behavior, and updated architecture/changelog docs. Full headless suite and production build pass locally. |

| 2026-10-04 | fix | Atomic-save cleanup now opens owner markers with `O_NONBLOCK`; a FIFO in a user-created lookalike directory previously stalled the scanner before its regular-file check. The timeout-guarded sweep regression reproduces the hang and verifies cleanup preserves the directory. Full headless suite, production build, focused ASan/UBSan run, and both hosted workflow #353 jobs passed. |

| 2026-10-04 | perf/fix | Atomic-save cleanup now keys its bounded parent cadence by resolved directory, so symlink aliases share first-touch and 32-write sweeps. Lexical tracker hits skip resolution; misses resolve before cadence lookup. Regression alternates both spellings through the interval boundary. Full headless suite, production build, focused ASan/UBSan test, and both hosted workflow #350 jobs passed. |

| 2026-10-04 | fix | Atomic saves now restore existing mode bits after writing and closing the staged file, preserving setuid that POSIX may clear during writes. The focused regression failed before the change and passes afterward, including failed-producer preservation; the complete headless suite, production build, and targeted ASan/UBSan filesystem suite pass. |

| 2026-10-04 | fix | Atomic-save workspaces now use a v4 ownership marker, so incomplete cleanup requires proof the directory was created by Monolith instead of deleting any directory that shares the internal prefix. Ready-marked v3/v2 recovery remains supported; the tiny partial-marker crash window can leave an unmarked orphan. Added lookalike-directory regressions and updated filesystem/architecture docs. |

| 2026-10-03 | fix | Terminal now rejects extra operands for fixed-arity built-ins instead of silently acting on only the first paths; `cp` and `rm` keep their documented flags but reject excess path operands. Regression tests verify rejected touch, mkdir, cp, mv, and rm commands leave entries unchanged. Full regular and ASan/UBSan headless suites passed locally with LeakSanitizer disabled due sandbox limitations; workflow #343 passed normal and full ASan/UBSan jobs. |

| 2026-10-03 | fix | Atomic-save workspaces now use v3 setup coordination: a brief parent-directory lock prevents sweeps from mistaking an active creator's incomplete directory for a crash remnant, allowing later sweeps to reclaim incomplete v3 workspaces. Marked v2 recovery remains supported; incomplete v2 and legacy workspaces are preserved. Focused recovery and lock tests added; workflow #340 passed normal and ASan/UBSan suites. |

| 2026-10-03 | fix | Text Editor Open/Save As, Drawing Open/Save, and Settings wallpaper entry now share Terminal's `~` / `~/` expansion to `/home/monolith`; path completion keeps the shorthand visible, and move/delete notifications resolve it consistently. Added shared helper/cursor mapping coverage and app-state regressions. The first hosted run exposed test-fixture issues; the focused Settings test passed locally after correction, then workflow #338 passed both normal and ASan/UBSan suites. |

| 2026-10-03 | fix | Terminal now resolves a leading `~` or `~/` to `/home/monolith` for command operands, including quoted paths. Home-relative Tab completion searches the resolved directory while retaining the typed shorthand. `cat`, `cd`, escaped-space paths, and quoted/unquoted completion regressions passed in hosted workflow #335, including ASan/UBSan; no tests were run on the user's device. |

| 2026-10-03 | fix | Atomic-save sweep cadence is now tracked independently for 16 recent destination parents: each is swept on first use and every 32 writes, and an evicted parent is swept again on revisit. Added first-touch, interval-boundary, and tracker-eviction recovery coverage; hosted workflow #331 passed normal and ASan/UBSan suites; no tests were run on the user's device. |

| 2026-10-03 | fix | Atomic-save workspaces now hold a nonblocking OS lease while active. The first atomic write and every 32 writes afterward sweep the current destination's parent, reclaiming completed v2 workspaces only when they have a ready marker and free lock; active, unmarked, and legacy workspaces remain untouched. Focused stale/active/legacy coverage and hosted workflow #327 passed normal and ASan/UBSan suites; no tests were run on the user's device. |

| 2026-10-03 | fix | Terminal command parsing and completion-prefix scanning now unescape only `\\` and `\"` in double quotes, preserving paths such as `folder\notes.txt`. Added regressions for literal/escaped backslashes in arguments and completion paths; hosted workflow #323 passed both normal and ASan/UBSan suites. |

| 2026-10-03 | fix | Atomic saves now reserve a unique hidden sibling workspace rather than truncating a fixed `<target>.tmp`; ordinary neighboring files and symlinks survive successful and failed writes. Filesystem and Desktop Settings regressions passed in hosted workflow #319, including ASan/UBSan. Workspace recovery was deferred here, then added in chunk 7.111 and refined per destination directory in 7.112. |

| 2026-10-03 | fix | Recursive directory copies now journal overwritten files before replacement, using same-volume hard links with a file-copy fallback; failed traversals restore file and directory timestamps, remove new files/subtrees, preserve symlink entries, and retain failed recovery backups. Cloud workflow #315 passed both normal and ASan/UBSan suites. |

| 2026-10-03 | perf | Text Editor multiline paste now normalizes line endings while counting rows, moves the retained prefix into the first replacement row, and performs one range insertion instead of shifting all following rows for each newline. Regression covers selection replacement, CRLF/lone-CR normalization, suffix and caret placement, trailing newlines, undo, and redo; focused editor test and production build passed; hosted workflow #311 passed both normal and sanitized jobs. |

| 2026-10-03 | perf | Text Editor undo snapshots now account copied line storage during cloning, removing the post-copy measurement pass while preserving the active-document budget precheck. Snapshot content/cursor/accounting regression, full headless suite, and production build passed; hosted workflow #307 passed both normal and sanitized jobs. |

| 2026-10-03 | perf | Terminal command-history trimming now filters invalid entries, scans backward once to retain the newest suffix within count and byte caps, and erases the discarded prefix once. Added a 256-entry boundary regression; normal and sanitized hosted workflow #303 passed. |

| 2026-10-03 | perf | Filesystem tree copying now keeps directory traversal state in an explicit frame stack instead of recursive calls, while preserving direct native iteration, atomic 16 KiB file streaming, symlink checks, and cleanup of newly created subtrees on failure. Added deep-copy and nested rollback coverage; normal and sanitized hosted workflow #299 passed. |

| 2026-10-03 | perf | Recursive Terminal copy notifications now use an explicit depth-first stack and `DirEntry` type data instead of recursive calls and an extra `isDirectory` query for every child. Expanded coverage checks exact directory-first notification order across nested, sibling, and root-level entries; normal and sanitized hosted workflow #295 passed. |

| 2026-10-03 | perf | Terminal scrollback now stores output rows and their viewport measurements in deques, avoiding a full retained-history shift when capped output evicts the oldest rows. The regression checks bounded newest-row behavior and stable identity/alignment for retained rows and measures; normal and sanitized hosted workflow #291 passed. |

| 2026-10-03 | perf | Taskbar layout now walks the focused window and remaining z-order directly, using cached title widths rather than allocating per-pass order and width vectors. Rendering and hit-target construction share the traversal; the coordinate test asserts focused-first ordering and statically rejects an owning TaskbarLayout. |

| 2026-10-03 | perf | WindowManager now reuses per-depth window identity snapshots across update and render passes and validates entries through a live pointer/ID index instead of repeated linear scans. Lifecycle and render tests cover nested callbacks, self-close, map consistency, and stable snapshot capacity after warm-up; normal and sanitized hosted workflow #283 passed. |

| 2026-10-03 | perf | Filesystem Browser now assembles the active filter label in retained storage, avoiding separate prefix/suffix substring and concatenation temporaries per render. The cursor width is measured from the same buffer only when its prefix or font changes; the state test checks the label remains correct and retains its storage across suffix-only edits. Normal and sanitized workflow #279 passed. |

| 2026-10-03 | perf | Filesystem Browser no longer allocates a temporary rename-prefix string on every render. It compares the current name prefix directly with its cached measured text, updating and rerasterizing only when the caret prefix changes. Rename-width and suffix-only-edit coverage passed in normal and sanitized hosted workflow #275. |

| 2026-10-03 | perf | Terminal now caches its abbreviated cwd prompt and reuses input, caret, selection, and reverse-search strings across frames; mouse hit-testing reuses its prefix buffer through binary-search measurements. State tests verify unchanged storage and prompt invalidation. Full hosted workflow #271 passed; the focused state test also passed locally under ASan/UBSan. |

| 2026-10-03 | perf | TextTextureCache now stores each owned font/text/color key only in the map; LRU nodes hold views into those stable keys, avoiding duplicate strings on misses. Cache stress coverage checks the oldest retained texture across map rehashes and count eviction; normal and sanitized hosted workflow #266 passed. |

| 2026-10-03 | perf | Shared text texture cache hits now hash and compare transparent font/text/color views instead of constructing an owned composite key on every draw; cache misses retain an owned text key. Coverage verifies font identity and caller-string ownership; normal and sanitized hosted workflow #262 passed. |

| 2026-10-03 | test/fix | GitHub Actions now builds the app and runs the complete headless suite under AddressSanitizer and UndefinedBehaviorSanitizer in addition to the normal job. Sanitizer coverage exposed a stale `CaptureApp*` read after the test closed that window; the Ctrl+Escape assertion now observes the still-live focused app. The runner forwards `CXXFLAGS` to all tests/shared runtime objects and keeps source locations in sanitizer traces; both hosted jobs passed in workflow #257. |

| 2026-10-03 | perf | Drawing history entries now retain their validated preimage-pixel byte count, while undo/redo stacks maintain totals during insertion, eviction, undo, redo, invalidation, and reset. This removes the prior scan across retained tile vectors on every committed edit. Regression checks compare entry and stack totals against stored pixels; hosted workflow #252 passed. |

| 2026-10-03 | fix | Session path records now escape embedded newlines as `\\n`, preventing valid filenames from splitting line-based snapshots. The decoder preserves prior quoted quote/backslash behavior and legacy unquoted tokens; format and WindowManager roundtrip tests cover actual newlines plus literal escape-like text. Full headless suite, production build, and hosted workflow #248 passed. |

| 2026-10-03 | perf | Terminal now persists its capped command history through the atomic producer using a shared fixed 16 KiB `BufferedStreamWriter`, eliminating the duplicate history string on eligible submissions. Text Editor uses the same helper. Tests cover short-write propagation, exact 2 MiB history output with non-aligned line boundaries, and existing failure/retry behavior; full headless suite, production build, and hosted workflow #244 passed. |

| 2026-10-03 | perf | Text Editor now validates its existing byte/line caps, then serializes LF-separated lines through the filesystem's atomic producer with a fixed 16 KiB scratch buffer, eliminating a second document-sized output string. Tests stream-verify exact 16 MiB content, line separators across output chunk boundaries, blank/trailing lines, and existing save failure/retry behavior; full headless suite, app build, and hosted workflow #240 passed. |

| 2026-10-03 | perf | Drawing now validates and decodes `.modr` input incrementally from bounded filesystem chunks, then adopts the decoded RGBA buffer directly instead of retaining an encoded-file string or allocating a discarded blank canvas. Invalid/incomplete input leaves the current sketch and decoder outputs unchanged; chunk-boundary and rollback coverage, AddressSanitizer, full headless suite, production build, and hosted workflow #236 passed. |

| 2026-10-03 | perf | Drawing now refreshes its saved comparison baseline in place when the current allocation has enough capacity, avoiding a transient full-canvas RGBA allocation on same-size saves, reloads, and clean resets. A Drawing state regression verifies stable storage and exact pixel contents; full headless suite, production build, and hosted workflow #232 passed. |

| 2026-10-03 | perf | Drawing now streams `.modr` RGB payloads through the atomic filesystem producer in bounded 16 KiB writes, eliminating the extra ~48 MiB encoded-file buffer at maximum canvas size. Existing files survive producer failure; format bytes match the string encoder. Maximum-canvas, atomic rollback, Drawing state, AddressSanitizer, full headless suite, production build, and hosted workflow #228 passed. |

| 2026-10-03 | perf | Text Editor save validates line/byte limits while computing exact serialized size, reserves one output buffer, and avoids the extra `ostringstream::str()` copy. Added exact 16 MiB save and blank/trailing-newline coverage; full headless suite, executable build, and hosted workflow #223 passed. |

| 2026-10-03 | fix | Terminal now reports command-history write failures without blocking commands, retries on later eligible submissions, and announces recovery; failed startup migration reports the problem and preserves the original history file. Full headless suite, executable build, and hosted workflow #219 passed. |

| 2026-10-03 | fix | Snake and Minesweeper now keep new records active in-session and replace false NEW BEST messages with persistence warnings; both retry after focus return or restart. Failure, warning rendering, recovery, full headless suite, executable build, and hosted workflow #215 passed. |

| 2026-10-03 | fix | Settings now reads the shell's last settings-save result, including failed atomic writes after wallpaper moves or deletions; successful recovery clears the warning. Full headless suite, executable build, and hosted workflow #211 passed. |

| 2026-10-03 | fix | Settings now reports rejected values and failed atomic saves in its footer without reverting live changes; reselecting the current choice retries persistence. The full headless suite and hosted workflow #207 passed. |

| 2026-10-03 | fix | A failed atomic session save now logs its target and shows a warning before SDL teardown; the previous snapshot remains unchanged. Lifecycle verification and hosted workflow #203 passed. |

| 2026-10-03 | fix | Desktop Settings now stops after 64 records as well as the shared 16 KiB line cap; a late preference beyond the budget cannot replace current values. The focused test and hosted workflow #199 passed. |

| 2026-10-03 | fix | Session save now matches the restore window cap: when more than 128 restorable windows exist, it persists the topmost 128 in existing stacking order. Focused WindowManager integration coverage and hosted workflow #195 passed. |

| 2026-10-03 | fix | Session restore now examines at most 1,024 records and stops processing entries once 128 windows are live. Integration coverage verifies both limits and valid-session behavior; hosted workflow #191 passed. |

| 2026-10-03 | fix | BMP wallpaper headers are now dimension-checked from the same open file handle passed to SDL, applying the existing 16,777,216-pixel cap before decoding. Valid BMP pixels and oversized-header rejection pass the focused test and hosted workflow #187. |

| 2026-10-03 | perf/fix | Recursive copies now reuse resolved source/destination paths and no-follow entry classification, reducing repeated path/status work; physical same-tree aliases through in-root destination symlinks are rejected. Preserved bounded atomic file copies and file-symlink destinations; focused filesystem suite and hosted workflow #183 passed. |

| 2026-10-03 | perf | Query-only Filesystem Browser refreshes now remap selection and anchor rows by source index in one ordered pass rather than rebuilding filename identity sets; regressions cover retained and filtered-out delete targets, with hosted workflow #179 passed. |

| 2026-10-03 | perf | Filesystem Browser now keeps one raw directory snapshot and represents filter results as source indices instead of copying each matching entry name; regressions cover selection across query changes and F5/notification refreshes, with hosted headless workflow #174 passed. |

| 2026-10-03 | perf | Directory listings now classify ordinary entries from one no-follow status lookup, resolving only in-root symlinks to classify directories; in-root dangling links remain visible, with hosted headless workflow #170 passed. |

| 2026-10-03 | perf | Limited listing containment canonicalization to symlinks and directories, preserving dangling in-root links as visible entries; added wide-list and symlink regressions; hosted headless workflow #166 passed. |

| 2026-10-03 | perf | Replaced recursive delete's per-directory child-path vector and application recursion with `std::filesystem::remove_all`; the virtual root stays protected and symlink targets remain untouched; hosted headless workflow #161 passed. |

| 2026-10-03 | perf | Changed recursive folder copies to traverse native directory entries directly, avoiding sorted UI listing snapshots; retained outside-root symlink filtering and failed-copy cleanup, with 96-file nested-tree coverage; hosted headless workflow #157 passed. |

| 2026-10-03 | perf | Removed the temporary `ancestor + "/"` string from recursive path-boundary checks; generated differential cases cover exact paths, root, and sibling prefixes. |

| 2026-10-03 | perf | Reused Filesystem's component scanner in `join()` to avoid combined-path temporaries and removed the extra substring allocation in host mapping; generated differential tests cover existing join semantics. |

| 2026-10-02 | perf | Replaced Filesystem path normalization's stringstream and component-string vector with a direct scan into the canonical path; differential coverage compares generated inputs against the old rules. |

| 2026-10-02 | fix | Replaced Minesweeper's bounded random retries plus deterministic fallback with uniform sampling without replacement; added seeded safe-first-open and exact mine-count coverage across difficulties and edge/corner positions. |

| 2026-10-02 | fix | Terminal clipboard paste now retains all lines in its single-line prompt by converting CR, LF, CRLF, and tabs to spaces; added mixed newline/tab regression coverage. |

| 2026-10-02 | fix | Added Shift+Page Up/Down panning for long Terminal output rows, snapping visible ranges to UTF-8 boundaries and caching only viewport-sized text; vertical scrolling remains unchanged, and new output resets the pan. |

| 2026-10-02 | fix | Bounded Snake and Minesweeper score-file reads by row and line size, rejected malformed or impossible values, and preserved earlier valid Minesweeper times when later data is oversized; added boundary and recovery regressions. |

| 2026-10-02 | fix | Shared the 16 KiB persisted-line reader between session restore and Desktop Settings; oversized settings records stop parsing, and unrepresentable wallpaper paths no longer replace or save the current setting. |

| 2026-10-02 | fix | Added a bounded 16 KiB session-line reader; oversized rows stop best-effort restore without allocating their full contents or discarding windows already restored. |

| 2026-10-02 | fix | Bound Drawing `.modr` opens by checking the format maximum before reading and enforcing it during chunked reads; oversized input preserves the current canvas and retry prompt. |

| 2026-10-02 | perf | Removed PNG/JPEG wallpaper's full compressed-file copy, checked pixel dimensions before decode, and added focused coverage; BMP continues through SDL's unchanged loader. |

| 2026-10-02 | fix | Kept Text Editor and Drawing prompts active after recoverable path, line-number, RGB, or file-operation failures, preserving input, caret, and horizontal position; added retry coverage for invalid prompts, oversized/corrupt opens, and blocked saves. |

| 2026-10-02 | fix | Aligned `.modr` encoder/decoder limits without restricting larger live canvases; oversized saves now fail rather than writing files Drawing cannot reopen, and format tests exercise the production codec. |

| 2026-10-02 | fix | Fixed same-path Open in Text Editor and Drawing so external file changes can be reloaded in the already-bound window; dirty editor text and canvas pixels still require the existing repeated discard confirmation. |

| 2026-10-02 | perf | Removed per-comparison/per-entry lowercase string allocations from Filesystem Browser sorting and filtering; each query is normalized once per pass, with mixed-case substring and ordering regressions added. |

| 2026-10-02 | perf | Replaced Text Editor mouse hit testing's growing-prefix allocation/measurement loop with one `TTF_MeasureUTF8` scan and UTF-8 codepoint-to-byte mapping; added multibyte and far-scrolled 100,000-character regressions; hosted headless workflow #113 passed. |

| 2026-10-02 | perf | Replaced Text Editor's per-hit Find vector with sparse checkpoints (one per 256 hits), limited cached highlight geometry to the visible text viewport, and changed Replace All to linear output building; added million-hit navigation/memory and dense-long-line regressions; hosted headless workflow #109 passed. |

| 2026-10-02 | perf | Drawing now accumulates changed pixel, fill-span, and undo/redo tile bounds and partially updates its streaming canvas texture; resize/load/Clear retain full refreshes, with headless dirty-region regression coverage. |

| 2026-10-02 | perf | Reused the Filesystem Browser directory snapshot across filter edits, retained refreshes from F5 and filesystem notifications, and passed hosted headless workflow #100. |

| 2026-10-02 | fix | Completed Terminal `touch` semantics: existing regular files receive a last-write-time update without content changes, outside-root symlinks stay protected, and metadata-only touches avoid content-change notifications; hosted headless workflow #105 passed. |

| 2026-10-02 | fix | Added UTF-8-safe keyboard/mouse selection and Ctrl+A/C/X/V to the Terminal command line; multiline clipboard paste stops at its first newline, with focused regression coverage and hosted headless workflow #103 passing. |

| 2026-09-29 | fix | Streamed Text Editor file opens through bounded chunks, capped documents at 16 MiB and 65,536 lines for open/save, preserved the active document on rejection, and covered cross-chunk CRLF plus both limits; hosted headless workflow #71 passed. |

| 2026-09-29 | perf | Reused Drawing's per-canvas scratch maps and reset only touched stroke-capture or Fill/Clear dirty indices; added cross-edit state coverage and passed hosted workflow #69. |

| 2026-09-29 | refactor | Removed Drawing's dead full-canvas history path now that strokes, Fill, and Clear all use sparse tiles; retained undo/redo, 32-state, and 64 MiB coverage using real edits and sparse entries; hosted workflow #66 passed. |

| 2026-09-29 | perf | Replaced Drawing Clear's full-canvas history copy with changed-tile preimages, keeping sparse large canvases undoable while retaining the 64 MiB overflow fallback; covered partial edge tiles, undo/redo, saved-baseline state, and hosted workflow #64. |

| 2026-09-27 | perf | Reused sparse 32×32 history for Drawing Fill so localized regions avoid full-canvas copies and remain undoable above 64 MiB; covered undo/redo, neighbor preservation, and hosted headless workflow #61. |

| 2026-09-27 | perf | Added per-tile Drawing dirty-state tracking so sparse undo/redo rechecks only affected tiles; covered save-mid-history restoration and fills returning to the saved baseline; hosted headless workflow #59 passed. |

| 2026-09-27 | perf | Replaced full-canvas stroke-start copies with lazy reversible 32×32 tile preimages; added sparse multi-tile, larger-than-budget undo/redo, and transient-budget fallback coverage; hosted headless workflow #56 passed. |

| 2026-09-27 | perf | Batched Drawing raster work around one validated canvas buffer per primitive, shared the Bresenham brush-stroke path, and clipped rectangle loops; added output-equivalence and invalid-buffer coverage. |

| 2026-09-26 | perf | Moved Drawing fill into the SDL-free raster module and replaced its per-pixel worklist with horizontal spans; added barrier, diagonal, no-op, and large-region coverage. |

| 2026-09-26 | perf | Reused the Drawing status prompt caret-prefix width across Save, Open, and RGB, with regression coverage for steady renders, caret movement, and UI-font invalidation. |

| 2026-09-26 | perf | Cached Text Editor status-bar caret-prefix widths across Find, Replace, Go to Line, Open, and Save As prompts; covered unchanged frames, mode changes, and UI-font invalidation. |

| 2026-09-26 | perf | Reused cached filename texture widths for Filesystem Browser row clipping, avoided per-frame filename copies, and cached inline-rename caret-prefix widths with prefix/font invalidation coverage. |

| 2026-09-26 | perf | Cached the Filesystem Browser filter caret-prefix width between renders; verified stable-prefix reuse, caret movement, and remeasurement after UI-font scaling. |

| 2026-09-26 | perf | Cached Terminal scrollback UTF-8 viewport byte boundaries per row and width, aligned them with append/trim/clear, and invalidated them on shared-font changes; added render coverage for reuse and long Unicode rows. |

| 2026-09-26 | perf | Added a viewport-bounded Text Editor cache for long-line UTF-8 slice bounds and hidden pixel offsets; invalidated for row, width, horizontal-scroll, edit, and font changes with renderer-state coverage. |
| 2026-09-26 | perf | Cached Text Editor Find prefix positions for visible rows and reused the query width across steady-state frames and active-match navigation; invalidated after search, viewport-row, and font changes with rendered-highlight regression coverage. |
| 2026-09-26 | perf | Cached lexical spans for the Text Editor viewport to avoid retokenizing unchanged visible rows every frame; invalidated on edits and syntax-mode changes, with viewport reuse and downstream color regression coverage. |
| 2026-09-26 | perf | Cached the Text Editor search-query pixel width for all visible match rectangles and invalidated it on result refresh or shared-font scaling; covered width and invalidation in the rendered Find test. |

| 2026-09-26 | perf | Reused Terminal input-prefix texture widths for cursor placement and cached reverse-search prefix textures in the existing bounded LRU; retained TTF measurement only when rasterization fails. |

| 2026-09-26 | perf | Changed Text Editor find rendering from per-visible-line scans of all document matches to one lower-bound seek plus a forward walk through visible hits; added a scrolled active-highlight pixel regression. |

| 2026-09-26 | perf | Cached taskbar clock time/date formatting by minute so steady-state renders avoid localtime and strftime calls; preserved texture reuse and font/format invalidation. |

| 2026-09-26 | perf | Cached taskbar title pixel widths per window, invalidated them on rename and font/UI-scale changes, and covered both paths in Window Manager tests. |

| 2026-09-26 | perf | Compiled the shared Window Manager and app sources once for the six integration tests; full CTest suite passed in 63 seconds locally versus 149 seconds before. |

| 2026-09-26 | chore | Registered the existing headless verification runner as `monolith_headless`, switched GitHub Actions to CTest, and verified the complete suite through `ctest --test-dir build --output-on-failure`. |

| 2026-09-26 | fix | Made CMake track every decompressed main and Settings include and reconfigure when compressed fragments are added or removed; verified incremental recovery for missing secondary outputs. |

| 2026-09-26 | fix | Carried Text Editor C-style block comments across lines in Code mode, cached outgoing state through visible rows, and invalidated from edited lines; covered closing-token continuation and opener edits. |

| 2026-09-26 | refactor | Removed the unused SDL_ttf surface-cache class after all app text paths moved to bounded renderer-owned texture caches. |

| 2026-09-26 | perf | Reused Settings labels, options, information lines, footer, and wallpaper-path prompt textures across frames and resizes with a renderer-aware 256-entry, estimated 16 MiB LRU; covered identity reuse and scale invalidation. |

| 2026-09-26 | perf | Reused Drawing toolbar and status or prompt text textures across frames with a renderer-aware 256-entry, estimated 16 MiB LRU; covered label identity, retained labels across status updates, and scale invalidation without changing canvas uploads. |

| 2026-09-26 | perf | Reused Filesystem Browser path, filter, toolbar, row, status, and menu textures across frames with a renderer-aware 256-entry, estimated 16 MiB LRU; covered cache reuse across listing refreshes and status changes. |

| 2026-09-26 | perf | Reused Terminal scrollback, command-input, and reverse-search textures across frames with a renderer-aware 256-entry, estimated 16 MiB LRU; covered reuse across modes plus clear, scroll, resize, and scale changes. |

| 2026-09-26 | perf | Reused Text Editor syntax-span, line-number, and status textures across frames with a renderer-aware 256-entry, estimated 16 MiB LRU; retained visible text across edits and scrolling, with scale invalidation coverage. |

| 2026-09-26 | chore | Pinned the headless workflow to `ubuntu-24.04` and upgraded checkout to v5's Node 24 runtime, addressing both notices from the successful hosted run. |

| 2026-09-26 | perf | Reused renderer-owned text textures across Pong, Breakout, Snake, and Minesweeper frames with a 256-entry, estimated 16 MiB LRU; covered texture identity, color keys, eviction, renderer switching, and scale invalidation. |

| 2026-09-26 | perf | Streamed recursive regular-file copies through bounded 16 KiB reads and the atomic destination writer, preserving byte-count checks and binary data; covered cross-chunk copies and overwrite behavior. |

| 2026-09-26 | perf | Rasterized only the UTF-8-safe window-title prefix that fits before the title controls and reused it until title width, visible text, font, or focus color changes; covered long Unicode titles and narrow resizes. |

| 2026-09-26 | perf | Bounded WindowManager shell text textures with a 256-entry, estimated 16 MiB LRU and capped Start-menu filter input at 64 UTF-8 bytes; covered entry/byte eviction, cache reuse, and complete-codepoint input. |

| 2026-09-26 | perf | Added filesystem tail-chunk reads and made Terminal seek to a bounded history suffix, preserving recent valid commands while avoiding scans of old prefixes; one 16 MiB recovery tail handles an oversized final record without an unbounded scan. |

| 2026-09-26 | perf | Bounded Terminal command history at 500 entries, 2 MiB total, and 64 KiB per command; streamed history loading, retained the newest valid entries, and covered legacy oversized files and commands. |

| 2026-09-26 | perf | Rasterized only viewport-intersecting UTF-8 syntax spans in the Text Editor and invalidated cached surfaces when the scroll position or client size changes; covered long-line surface bounds and scroll invalidation. |

| 2026-09-26 | perf | Added a bounded 16 KiB filesystem chunk reader and changed Terminal `cat` to normalize CRLF/lone-CR while streaming, cap its active row, and stop at the 5,000-line limit. |

| 2026-09-26 | perf | Bounded Terminal scrollback to 2,000 rows, 8 MiB, and 64 KiB per row; rasterized viewport-fitting UTF-8 prefixes and invalidated the view cache on output, scroll, resize, and scale changes. |

| 2026-09-26 | perf | Capped Text Editor undo/redo history at 50 states and estimated 64 MiB, preserved typing coalescing, and moved document buffers during history traversal. |

| 2026-09-26 | perf | Capped Drawing undo/redo history at 32 states and 64 MiB, moved snapshots between history stacks, and added state and byte-budget coverage. |

| 2026-09-26 | chore | Added GitHub Actions verification for the full Monolith build and headless suite on main/beta pushes and pull requests; restored executable modes for the runner and its direct integration checks. |

| 2026-09-16 | perf | Reused the WindowManager shell text cache for the Alt+Tab title overlay, added renderer reuse coverage, and documented the shell rendering lifecycle. |

| 2026-09-16 | perf | Cached Settings labels, options, information lines, and footer text surfaces, bounded wallpaper prompt variants, added renderer reuse coverage, and updated Settings and architecture documentation. |

| 2026-09-16 | fix | Bounded Text Editor and Drawing dynamic status or prompt text caches, added focused invalidation coverage, and updated app and architecture documentation. |

| 2026-09-16 | fix | Invalidated Filesystem Browser text surfaces when listings or status feedback change, added focused cache lifecycle coverage, and updated Browser and architecture documentation. |

| 2026-09-16 | perf | Cached Terminal scrollback text surfaces, cleared them when output is cleared or trimmed and on UI-scale changes, added renderer coverage, and updated architecture and Terminal documentation. |

| 2026-09-16 | perf | Cached Text Editor syntax spans, line numbers, and status text surfaces, cleared them after document changes and UI-scale changes, added renderer coverage, and updated architecture and editor documentation. |

| 2026-09-16 | docs | Documented Drawing's cached toolbar and status text surfaces, UI-scale invalidation, and canvas texture upload lifecycle. |

| 2026-09-16 | perf | Cached Drawing toolbar and status or prompt text surfaces, cleared them on UI-scale changes, added renderer coverage, and updated architecture and changelog notes. |

| 2026-09-16 | perf | Cached Filesystem Browser text surfaces for paths, rows, status, and menus, cleared them on UI-scale changes, added renderer coverage, and updated architecture and changelog notes. |

| 2026-09-16 | perf | Added a renderer-independent SDL_ttf surface cache for Pong, Breakout, Snake, and Minesweeper, cleared it on UI-scale changes, added Snake coverage, and documented the lifecycle. |

| 2026-09-16 | perf | Reused cached UTF-8 taskbar title textures across buttons and frames, preserved minimized and active colors, and updated the shell rendering notes. |

| 2026-09-16 | perf | Consolidated taskbar and Start-menu labels behind a text-and-color cache, invalidated it with shared font changes, added renderer coverage, and updated architecture and changelog notes. |

| 2026-09-16 | perf | Cached desktop icon glyphs and normal or selected labels between frames, invalidated them with shared font changes, and added renderer coverage plus documentation. |

| 2026-09-16 | perf | Cached the static Start-menu header texture across frames, invalidated it with font changes, and added renderer coverage plus architecture and changelog notes. |

| 2026-09-16 | fix | Ended active Text Editor selections and Drawing strokes before modal prompts, added focused gesture coverage, and updated both app guides and the changelog. |

| 2026-09-16 | fix | Ended Text Editor and Drawing gestures and cleared Minesweeper pressed previews on focus loss, added focused coverage, and updated architecture and app documentation. |

| 2026-09-16 | fix | Kept Minesweeper middle-click chords instantaneous instead of arming a release-dependent preview, added focused coverage, and documented the interaction. |

| 2026-09-16 | cleanup | Cached Filesystem Browser toolbar and filter hit targets between frames, preserved direct-render resize handling, added coverage, and updated the Browser guide and changelog. |

| 2026-09-16 | fix | Synchronized Text Editor and Terminal client geometry during direct renders, kept scroll bounds current, added focused coverage, and updated both app guides and the changelog. |

| 2026-09-16 | cleanup | Routed all game render-size changes through the normal resize lifecycle, added direct-render geometry coverage, and updated the four game guides and changelog. |

| 2026-09-16 | fix | Canceled Filesystem Browser inline renames before pointer actions, added toolbar-delete coverage, and updated the Browser guide and changelog. |

| 2026-09-16 | fix | Canceled Filesystem Browser inline renames before wheel scrolling, added stale-row coverage, and updated the Browser guide and changelog. |

| 2026-09-16 | fix | Blocked queued shell hotkeys while the SDL host is unfocused, added complete keyboard-path coverage, and updated input-routing documentation. |

| 2026-09-16 | fix | Clamped captured Drawing drags to the nearest canvas edge for pen and shape tools, added focused endpoint coverage, and updated Drawing and shell input documentation. |

| 2026-09-16 | fix | Extended captured Text Editor mouse selections to the nearest visible document edge, added focused coverage, and updated editor and shell input documentation. |

| 2026-09-16 | fix | Invalidated taskbar targets when focus clears after the last visible window is minimized, added no-render-gap coverage, and updated shell architecture/changelog notes. |

| 2026-09-16 | fix | Kept client keyboard events owned by the focused app after shell hotkeys, added callback-close coverage that blocks accidental desktop-icon activation, and updated input-routing documentation. |

| 2026-09-16 | fix | Invalidated cached taskbar targets at the minimize transition, added queued-input coverage before the next render, and updated architecture/changelog notes. |

| 2026-09-16 | fix | Reapplied current usable geometry when restoring minimized maximized windows, added desktop-growth coverage, and updated architecture/changelog notes. |

| 2026-09-16 | fix | Isolated WindowManager SDL draw color state across app callbacks and full-frame composition, added inter-app and caller-state regression coverage, and updated architecture/changelog notes. |

| 2026-09-16 | fix | Isolated WindowManager SDL blend state across app callbacks and full-frame composition, added a deliberate-leak regression probe, and updated architecture/changelog notes. |

| 2026-09-16 | fix | Restarted Terminal reverse-search matching after query edits so refinement cannot skip a still-matching command, added focused coverage, and documented the traversal contract. |

| 2026-09-16 | fix | Preserved caller SDL blend modes across Snake and Minesweeper end-state overlays, added renderer-state coverage, and updated architecture/changelog notes. |

| 2026-09-16 | refactor | Trimmed Terminal scrollback and command history caps in one range operation, added bounded-history coverage, and documented the efficiency fix. |

| 2026-09-16 | fix | Scoped Window Manager and app destruction before SDL renderer shutdown, added lifecycle verification, and updated architecture/changelog notes. |

| 2026-09-16 | fix | Preserved the Terminal draft caret across command-history navigation, added focused coverage, and updated the Terminal guide and changelog. |

| 2026-09-16 | fix | Preserved Terminal reverse-search match state so repeated Ctrl+R walks older commands, added focused coverage, and updated the Terminal guide. |

| 2026-09-16 | fix | Rebuilt Settings control targets before queued post-scale input, aligned scrolled click coordinates with the rendered controls, added focused coverage, and updated the Settings guide. |

| 2026-09-16 | fix | Rebuilt Drawing toolbar and swatch targets before queued post-scale input, shared their geometry between rendering and events, added focused coverage, and updated the Drawing guide. |

| 2026-09-16 | fix | Rebuilt Filesystem Browser toolbar and filter targets before queued post-scale input, shared their geometry between rendering and events, added focused coverage, and updated the Browser guide. |

| 2026-09-16 | refactor | Routed Start-button and taskbar-arrow input through the shared rendered taskbar layout, added scaled pre-render coverage, and updated shell architecture notes. |

| 2026-09-16 | fix | Shared taskbar geometry between rendering and hit testing, rebuilt taskbar targets before queued clicks, added pre-render activation coverage, and updated shell architecture notes. |

| 2026-09-16 | fix | Made Start-menu keyboard and mouse routing rebuild popup hit targets before the first render, with pre-render input coverage and updated shell architecture notes. |

| 2026-09-16 | fix | Kept Start-menu focus handoffs single-step for window clicks, taskbar activation, and Alt+Tab; added focused callback coverage and updated shell architecture notes. |

| 2026-09-16 | fix | Invalidated failed wallpaper-load state when the configured image or a parent directory is created, added focused recovery coverage, and updated shell architecture notes. |

| 2026-09-16 | fix | Made session restore identify each launched window by its monotonic creation ID, so app creation callbacks cannot redirect saved geometry to a nested callback-created window. |

| 2026-09-16 | fix | Made direct `Filesystem::copyRecursive` calls create missing destination parents, added nested-file copy coverage, and aligned the filesystem guide with the shared copy contract. |

| 2026-09-16 | test | Made reentrant lifecycle assertions observe external state instead of destroyed app objects, keeping the close regression sanitizer-clean. |

| 2026-09-16 | fix | Re-found the Window Manager close target after focus-loss callbacks so sibling closes cannot invalidate its erase iterator; added lifecycle coverage and architecture notes. |

| 2026-09-16 | fix | Remapped active Editor and Drawing path prompts after directory moves, preserving normalized paths and UTF-8-safe caret positions. |

| 2026-09-16 | fix | Preserved the actual removed directory in bound-file callbacks so Editor and Drawing prompts recover to a surviving parent after recursive deletion. |

| 2026-09-16 | fix | Aligned Text Editor Replace All with Find's non-overlapping match order, fixing overlapping candidates and adding regression coverage. |

| 2026-09-16 | fix | Preserved direct Filesystem Browser rename state across synchronous self-notifications, with re-entrant rename regression coverage and documentation updates. |

| 2026-09-16 | fix | Blocked queued keyboard and pointer delivery while the SDL host is unfocused, with focus regression coverage and architecture notes. |

| 2026-09-16 | fix | Refreshed SDL pointer state for wheel routing and added coverage for stationary-pointer scrolling over a client. |

| 2026-09-16 | fix | Enforced the caller renderer clip across the complete WindowManager frame and added pixel-level containment coverage for shell drawing. |

| 2026-09-16 | fix | Invalidated taskbar hit targets after arrow and wheel scrolling, with regression coverage for rapid between-frame input. |

| 2026-09-16 | fix | Routed failed-open and file-binding title changes for Editor and Drawing through the shell title/cache helper. |

| 2026-09-16 | fix | Invalidated cached shell hit targets after window creation, focus ordering, close, and title changes, and added between-frame taskbar regression coverage. |

| 2026-09-16 | fix | Started and stopped SDL text input in the executable lifecycle, added the deterministic main-body compressor, and documented both compressed-source workflows. |

| 2026-09-16 | docs | Clarified dummy SDL driver usage for focused headless tests so individual SDL-linked commands match the complete verification runner. |

| 2026-09-16 | test | Covered both keydown and keyup sides of shell-owned Alt+Tab and Ctrl+Escape gestures in the WindowManager mouse-capture regression suite. |

| 2026-09-16 | chore | Added a deterministic Settings fragment compressor to pair with the existing decompressor, and documented the readable-source round-trip workflow. |

| 2026-09-16 | fix | Invalidated Settings swatch, clock, interface-scale, and wallpaper-control hit targets after resize, interface-scale, or scroll changes, with focused Settings state coverage and app-guide/changelog notes. |

| 2026-09-16 | fix | Invalidated Drawing toolbar and color-swatch hit targets after resize and interface-scale changes, with focused Drawing state coverage and app-guide/changelog notes. |

| 2026-09-16 | fix | Invalidated Filesystem Browser toolbar and filter hit targets after resize and interface-scale changes, with focused browser state coverage and app-guide/changelog notes. |

| 2026-09-16 | fix | Invalidated cached taskbar, clock, and Start-menu hit targets after shell geometry, clock-format, or font-metric changes, with focused window-coordinate coverage and architecture/changelog notes. |

| 2026-09-15 | fix | Completed Start menu keyboard handling with wrapped Up/Down selection, Enter activation, Escape dismissal, and focused shell regression coverage. |

| 2026-09-15 | fix | Deferred WindowManager closes requested from app callbacks, added re-entrant notification coverage, and documented the callback lifetime contract. |

| 2026-09-15 | refactor | Consolidated virtual filesystem writes onto the shared binary-capable atomic writer and covered embedded NUL preservation. |

| 2026-09-15 | fix | Made shared atomic host snapshots clean up and report failure when a serializer throws, with focused persistence coverage. |

| 2026-09-15 | fix | Intersected every Filesystem Browser text subregion with the caller renderer clip and added pixel-level containment coverage. |
| 2026-09-15 | refactor | Routed WindowManager client rendering and the Filesystem Browser context menu through the shared renderer clip intersection helper. |
| 2026-09-15 | refactor | Routed Pong, Snake, Minesweeper, and Breakout text clipping through the shared renderer clip helper. |
| 2026-09-15 | refactor | Routed compressed Settings rendering through the shared renderer clip helper for clipped labels, the wallpaper field, and scrolling content. |
| 2026-09-15 | refactor | Centralized renderer clip handling across the shell and native app renderers, and routed compressed Settings completion through the shared UTF-8 prefix helper. |

| 2026-09-15 | refactor | Centralized the UTF-8-safe common completion prefix used by Text Editor, Drawing, and Terminal, with direct ASCII and ambiguous-Unicode helper coverage. |

| 2026-09-15 | refactor | Centralized the UTF-8 completion boundary guard in `Utf8.hpp`, switched all four completion clients to it, and added direct helper coverage. |

| 2026-09-15 | fix | Made Terminal path completion stop at complete UTF-8 codepoints, added ambiguous Unicode coverage, and updated the Terminal guide and changelog. |

| 2026-09-15 | fix | Made Drawing and Settings path completion stop at complete UTF-8 codepoints, added ambiguous Unicode coverage for both prompts, and updated their guides and changelog. |

| 2026-09-15 | fix | Canceled Filesystem Browser inline renames before external listing refreshes, added stale-row regression coverage, and updated the browser guide and changelog. |

| 2026-09-15 | fix | Clamped Text Editor horizontal scroll to the current line after wheel input and resize, added widened-client regression coverage, and updated the editor guide and changelog. |

| 2026-09-15 | fix | Made Drawing Fill mark pixels on enqueue to avoid duplicate work on large regions, added connected-canvas coverage, and updated the Drawing guide and changelog. |

| 2026-09-15 | fix | Contained the taskbar clock date tooltip within the usable desktop on short and narrow clients, preserved caller clipping, and added focused geometry coverage plus architecture/changelog notes. |

| 2026-09-15 | fix | Contained Filesystem Browser context menus within undersized clients, clipped their popup rendering, and added focused geometry coverage plus app docs. |

| 2026-09-15 | fix | Contained the Start menu within undersized logical desktops, clipped partially visible menu rows and hit targets, and added shell geometry coverage plus docs. |

| 2026-09-15 | fix | Enforced the WindowManager minimum frame size during direct creation, verified the initial app resize dimensions, and updated architecture/changelog notes. |

| 2026-09-15 | fix | Contained Minesweeper's face button on extreme narrow clients, hid impossible overlapping difficulty controls while retaining keyboard shortcuts, and added focused geometry coverage plus docs. |

| 2026-09-15 | fix | Clipped the taskbar Start button and its hit target to the logical desktop edge on tiny desktops, added rendering/input coverage, and updated architecture and changelog notes. |

| 2026-09-15 | fix | Gave desktop icons a stable label-width cell, omitted them on too-narrow desktops, added layout coverage, and updated the 7.3 behavior note and changelog. |

| 2026-09-15 | fix | Kept Text Editor path completion on complete UTF-8 boundaries, covered ambiguous and exact Unicode filenames, and updated the Text Editor guide and changelog. |

| 2026-09-15 | fix | Preserved valid empty sessions instead of seeding demo windows after stale-entry filtering, updated the load contract, and extended session coverage. |

| 2026-09-15 | fix | Skipped stale file-backed session entries instead of restoring blank apps, added missing-editor and corrupt-Drawing coverage, and updated architecture/changelog/development docs. |

| 2026-09-15 | fix | Claimed new Text Editor and Drawing file bindings before creation notifications, added event-order coverage, and updated app/architecture/changelog docs. |

| 2026-09-15 | fix | Added Drawing singleton focus/rejection through the window controller, covered duplicate Open and Save destinations, and updated Drawing/architecture/changelog docs. |

| 2026-09-15 | fix | Rechecked callback-created windows during shutdown validation, added coverage for a dirty editor opened from `allowClose()`, and updated architecture/changelog/development docs. |

| 2026-09-15 | fix | Rechecked window identity after `allowClose()` and focus-loss callbacks, added recursive-close lifecycle coverage, and updated architecture/changelog/development docs. |

| 2026-09-15 | fix | Identity-checked editor and Drawing bound-file remaps before lifecycle callbacks, added sibling-close coverage, and updated architecture/changelog/development docs. |

| 2026-09-15 | fix | Made WindowManager rendering use a live identity snapshot, added renderer-backed self-close coverage, and updated architecture/changelog/development docs. |

| 2026-09-15 | fix | Guarded session restore geometry callbacks and counted only live restored entries; added resize-close coverage and updated architecture/changelog/development docs. |

| 2026-09-15 | fix | Routed shutdown allow-close validation through the live window snapshot; added coverage for callback-created editor windows and updated architecture/changelog/development docs. |

| 2026-09-15 | fix | Kept focus callbacks aligned with actual host and Start-menu activity, deferred modal handoffs across callback-triggered closes, and added focused Start-menu regression coverage with updated architecture/changelog/development docs. |

| 2026-09-15 | fix | Guarded shell input and window creation against app callbacks that close or replace their target; added click-activation and resize-triggered removal coverage and updated architecture/changelog/development docs. |

| 2026-09-15 | fix | Published WindowManager focus before lifecycle callbacks and guarded focus-gained handoff against self-closes; added focused callback coverage and updated architecture/changelog/development docs. |

| 2026-09-15 | fix | Routed logical desktop, maximized-window, and app-update callbacks through the stable WindowManager window snapshot; added resize-time self-close coverage and updated architecture/changelog/development docs. |

| 2026-09-15 | fix | Made virtual-path and UI-scale app callbacks use a live window snapshot, preventing app-triggered lifecycle changes from invalidating broadcasts; extended lifecycle coverage and updated architecture/changelog/development docs. |

| 2026-09-15 | fix | Made WindowManager app-update dispatch resilient to controller-driven self-closes with a live pointer snapshot; added lifecycle coverage and updated architecture/changelog/development docs. |

| 2026-09-15 | fix | Removed closing windows from cached taskbar hit targets before destruction, preventing stale raw pointers after app-triggered closes; added WindowManager coverage and updated architecture/changelog docs. |

| 2026-09-15 | fix | Shared Pong and Breakout frame timing through a wrap-safe SDL tick helper with a 50 ms stall cap; added boundary coverage and updated the game, architecture, and developer documentation. |

| 2026-09-15 | fix | Made Terminal single-quoted path completion encode apostrophes safely, close completed file paths, and preserve the resulting argument through the lexer; added focused coverage and updated the Terminal guide. |

| 2026-09-15 | fix | Rejected symlinked and non-regular atomic temporary siblings for virtual files and host snapshots, added target-preservation coverage, and updated the persistence contract. |

| 2026-09-15 | fix | Isolated Snake food and Minesweeper mine placement behind independent bounded random streams, removed process-global RNG coupling, added helper coverage, and updated both game guides plus development docs. |

| 2026-09-15 | docs | Documented the complete headless suite in the Drawing developer guide, including the focused-versus-full verification workflow; updated the public changelog. |

| 2026-09-15 | chore | Added one-command headless verification for all documented static and state checks, including generated source setup and consistent SDL dummy drivers, and documented the focused-command escape hatch. |

| 2026-09-15 | fix | Made Snake's food flash deadline wrap-safe across the 32-bit SDL tick counter, added active/expired boundary coverage, and documented the effect contract. |

| 2026-09-15 | fix | Made Filesystem remove operations unlink symlink entries without following them and made rename move in-root symlink entries without moving their targets, including hidden direct outside-root links; added target-preservation coverage and documented the safety contract. |

| 2026-09-15 | fix | Made Text Editor Tab completion report `No path matches.` instead of failing silently, added focused prompt coverage, and aligned the editor guide with the feedback contract. |

| 2026-09-15 | fix | Aligned Minesweeper with the other app keyboard contracts by accepting keypad Enter for a new game, added focused reset coverage, and recorded the shortcut behavior. |

| 2026-09-15 | fix | Aligned Snake with the other games by accepting keypad Enter for resume/restart, added focused input coverage, and recorded the keyboard contract. |

| 2026-09-15 | fix | Unified Settings, session, Snake, and Minesweeper host text snapshots behind one atomic writer, retained existing regular-file permissions, added score reload and failed-replacement coverage, and updated the persistence documentation. |

| 2026-09-15 | fix | Made virtual regular-file writes atomic with temporary siblings, retained existing permission bits, preserved in-root file symlink traversal, and kept outside-root links rejected; added focused filesystem coverage and updated the filesystem contracts. |

| 2026-09-15 | fix | Protected desktop settings with validated temporary writes and atomic replacement, preserving the prior configuration when replacement fails; added focused persistence coverage and aligned Settings and architecture documentation. |

| 2026-09-15 | fix | Protected session snapshots with validated temporary writes and atomic replacement, preserving the previous file when replacement fails; added focused save/cleanup coverage and documented the persistence contract. |

| 2026-09-15 | fix | Made the Start menu a modal focus boundary: active apps pause while it is open, held game controls are cleared through focus loss, and host focus changes cannot resume apps behind the menu; added focused coverage and updated architecture notes. |

| 2026-09-15 | fix | Kept `Tab` and `Escape` releases paired with shell-consumed `Alt+Tab` and `Ctrl+Escape` keydowns, added focused input coverage, and documented the complete gesture boundary. |

| 2026-09-15 | fix | Kept the Alt+Tab modifier release inside WindowManager so shell-owned key state cannot leak into clients; added focused input regression coverage and documented the contract. |

| 2026-09-15 | fix | Preserved Filesystem Browser primary selection and Shift-selection anchors across external renames in the visible folder; added focused regression coverage and updated Browser/architecture docs. |

| 2026-09-15 | fix | Refreshed a Filesystem Browser when its current directory is the changed path, covering directory-level external updates and documenting the recursive-tree behavior. |

| 2026-09-15 | fix | Made Terminal recursive copies into existing trees broadcast every changed destination path, preventing stale nested app state; added focused filesystem notification coverage and updated Terminal/architecture docs. |

| 2026-09-15 | fix | Recovered client and shell pointer state across host SDL focus loss, synthesized the missing client release, and covered host focus callbacks in the mouse-capture test. |

| 2026-09-15 | fix | Released bare Editor and Drawing instance reservations when Save As/Save makes a window file-backed; compacted remaining titles and added WindowManager coverage plus app documentation. |

| 2026-09-15 | fix | Prevented orphaned client mouse releases after empty-desktop and window-frame clicks; added capture regression coverage and updated architecture/changelog docs. |

| 2026-09-15 | fix | Prevented taskbar, empty-desktop, and window-frame motion from reaching focused clients without an active client press; added shell-motion regression coverage. |

| 2026-09-15 | docs | Aligned the architecture guide with shipped Breakout launchers and runtime logical desktop sizing; updated the public changelog. |

| 2026-09-15 | fix | Hardened non-recursive filesystem removal against deleting the virtual root; added root-preservation coverage and updated filesystem documentation. |

| 2026-09-15 | fix | Corrected desktop-icon row counting so the first tile and label render at the exact usable-height boundary; added exact-fit layout coverage and updated the 7.3 note. |

| 2026-09-15 | fix | Made desktop-icon double-click timing wrap-safe across SDL's 32-bit tick counter; added a boundary regression case and updated the 7.3 behavior note. |

| 2026-09-15 | fix | Cleared desktop-icon selection and double-click history across Start menu, taskbar, Alt+Tab, and icon activation paths; added shell event coverage and updated the 7.3 behavior note. |

| 2026-09-15 | fix | Cleared desktop-icon double-click history with selection state so unrelated window clicks cannot trigger a false launch; added WindowManager regression coverage and updated the 7.3 behavior note. |

| 2026-09-15 | fix | Repaired verification script executability, taught the Drawing check to inspect compressed main bodies, and fixed the documented Settings test include path; the full documented headless suite passed. |

| 2026-09-12 | fix | Preserved the caller renderer clip through the WindowManager frame, title labels, taskbar buttons, and Alt+Tab overlay; added full-frame coverage and updated architecture docs. |

| 2026-09-12 | fix | Preserved the caller renderer clip through Drawing status rendering; added frame-level clip coverage and updated Drawing and architecture docs. |

| 2026-09-12 | fix | Preserved the caller renderer clip through Text Editor document and status regions; added frame-level clip coverage and updated Text Editor and architecture docs. |

| 2026-09-12 | fix | Preserved the caller renderer clip through Terminal input and history regions; added frame-level clip coverage and updated Terminal and architecture docs. |

| 2026-09-12 | fix | Preserved the caller renderer clip through Filesystem Browser path, list, and status regions; added frame-level clip coverage and updated Browser and architecture docs. |

| 2026-09-12 | fix | Preserved and intersected the caller renderer clip through Settings scroll and wallpaper-field rendering; added clip-restoration coverage and updated architecture and Settings docs. |

| 2026-09-12 | fix | Made the Settings wallpaper field yield width to keep Set and Clear inside narrow clients; added render-level geometry coverage and updated the Settings guide. |

| 2026-09-12 | fix | Clamped WindowManager client rendering height at zero on desktops shorter than a title bar; added shared shell coverage and updated architecture notes. |

| 2026-09-12 | fix | Made Minesweeper difficulty controls compress to the available HUD width without overlapping the face button; added narrow and tiny-client coverage and updated the guide. |

| 2026-09-12 | fix | Kept the Filesystem Browser filter control inside narrow client widths by sharing a clamped filter rectangle; added focused geometry coverage and updated the Browser guide. |

| 2026-09-12 | fix | Centralized Terminal history viewport geometry and clamped narrow-client clip dimensions; added focused non-negative-bound coverage and updated the Terminal guide. |

Historical session rows were trimmed during the 7.1 MCP ship to keep AGENTS.md small.
Current chunk pointer: [`CURRENT_CHUNK`](CURRENT_CHUNK).

| 2026-09-16 | fix | Kept Start-menu Return and keypad Enter activation shell-owned through key release, added focused input coverage, and updated the architecture and changelog notes. |

| 2026-09-16 | fix | Kept the Start-menu Escape dismissal gesture shell-owned through key release, added focused input coverage, and updated the architecture and changelog notes. |

| 2026-09-16 | fix | Rendered WindowManager title-bar and taskbar labels through SDL_ttf's UTF-8 API so Unicode filenames match their measured widths; added a render regression and updated architecture/changelog docs. |

| 2026-09-16 | fix | Floored screen-to-logical pointer conversion at negative and header-offset boundaries so outside clicks cannot map onto logical row or column zero; added coordinate regression coverage and updated architecture/changelog docs. |

| 2026-09-12 | fix | Made Filesystem Browser report zero rows and stop rendering the list when undersized chrome leaves no client area; added tiny-client coverage and updated docs. |
| 2026-09-12 | fix | Restricted Filesystem Browser selection and context-menu hit testing to complete rendered rows; added partial-row coverage and updated docs. |
| 2026-09-12 | fix | Restricted Text Editor mouse selection to complete rendered rows above the status bar; added gap-click coverage and updated docs. |
| 2026-09-12 | fix | Clamped Terminal input-bar geometry inside undersized client rectangles; added focused containment coverage and updated docs. |
| 2026-09-12 | fix | Mapped Drawing pointer input through the scaled display canvas while preserving raster dimensions and history; added edge mapping coverage and updated docs. |
| 2026-09-12 | fix | Made Settings section and control bands follow active font metrics, including compressed body sources; added 22pt coverage and updated docs. |
| 2026-09-12 | fix | Clamped the Settings footer inside undersized client rectangles; added tiny-client coverage and updated docs. |
| 2026-09-12 | docs | Expanded the Drawing contributor verification block with the current focused checks and optional smoke command. |
| 2026-09-12 | fix | Aligned Filesystem Browser list hit testing with the rendered row origin and gaps; added padding and boundary coverage and updated the Browser guide. |
| 2026-09-12 | fix | Clipped WindowManager app rendering to each client rectangle and restored the renderer clip; added shared render-boundary coverage and updated architecture docs. |
| 2026-09-12 | fix | Preserved the WindowManager client clip through Terminal, Text Editor, Drawing, and Filesystem Browser text-region helpers. |
| 2026-09-12 | fix | Made Terminal history navigation exit on edits and completion so Down preserves user changes; added focused coverage and updated the Terminal guide. |
| 2026-09-12 | fix | Unified Minesweeper rendered and interactive face/difficulty button geometry across resize and interface scaling; added focused hitbox coverage and updated docs. |
| 2026-09-12 | fix | Clamped Filesystem Browser scrollback after resize and text scaling even without a selected row; added lifecycle coverage and updated the Browser guide. |
| 2026-09-12 | fix | Clamped Text Editor scrollback after window resizes and removed false visible rows from tiny clients; added focused coverage and updated the editor guide. |
| 2026-09-12 | fix | Made Drawing toolbar and status geometry follow the active font without changing canvas data or undo history; added scaled coverage and updated the Drawing guide. |
| 2026-09-12 | fix | Bound Terminal scrollback to the rows actually rendered above the input strip, including resize and text-scale clamping; added focused coverage and updated the Terminal guide. |
| 2026-09-12 | fix | Kept Filesystem Browser status-bar clicks out of list selection and clamped tiny list rendering; added scaled hit-test coverage and updated browser docs. |
| 2026-09-12 | cleanup | Aligned Filesystem Browser path, toolbar, list, and status geometry with active font metrics; updated row hit testing, scale coverage, and browser docs. |
| 2026-09-12 | cleanup | Made Text Editor status-bar layout and click exclusion follow active font metrics; updated editor documentation and verification notes. |
| 2026-09-12 | cleanup | Made taskbar buttons, scroll arrows, and the clock tray derive their height from the active font while staying inside the taskbar band; updated architecture and changelog notes. |
| 2026-09-12 | cleanup | Made Minesweeper HUD, footer, difficulty controls, board layout, and hit testing follow active font metrics; added scaled-control coverage and updated the guide. |
| 2026-09-12 | cleanup | Derived Snake and Pong HUD geometry from active font metrics so text scaling keeps game fields below the interface strip; added Snake coverage and updated game guides. |
| 2026-09-12 | docs | Documented Drawing behavior when the shared interface text scale changes, including prompt remeasurement and preserved canvas state. |
| 2026-09-12 | cleanup | Rebuilt open Filesystem context-menu geometry after live UI scale changes; added app lifecycle coverage and updated shell/browser docs. |
| 2026-09-12 | cleanup | Routed Window Manager wallpaper loading directly through WallpaperImage to remove the SDL_LoadBMP macro warning; behavior is unchanged. |
| 2026-09-12 | docs | Updated WindowManager verification commands for generated Settings bodies and WallpaperImage linkage. |
| 2026-09-12 | docs | Aligned vision, architecture, and Terminal documentation with shipped BMP/PNG/JPEG wallpaper support. |
| 2026-09-12 | cleanup | Rebuilt Terminal and Text Editor view offsets after shared text scaling; scale notifications now include minimized apps. |
| 2026-09-12 | cleanup | Sized taskbar buttons from measured UTF-8 title widths and added scaled-render coverage for long labels. |
| 2026-09-12 | cleanup | Added Settings prompt invalidation for shared text scaling and covered its cached horizontal offset. |
| 2026-09-12 | cleanup | Added Drawing prompt invalidation for shared text scaling and covered its cached horizontal offset. |
| 2026-09-12 | cleanup | Sized Minesweeper difficulty controls from the shared font and kept their hit areas aligned across UI scales. |
| 2026-09-12 | cleanup | Reset the Filesystem Browser filter prompt's cached offset after shared text scaling and covered the lifecycle path. |
| 2026-09-12 | cleanup | Sized Snake and Minesweeper overlay spacing from active font metrics to prevent scaled-text overlap. |
| 2026-09-12 | cleanup | Fixed session restore focus handoff when the last restored entry is minimized and covered focus notifications. |
| 2026-09-12 | cleanup | Isolated taskbar and Start-menu pointer releases from client apps and covered the shell capture path. |
| 2026-09-16 | fix | Capped Pong vertical ball speed after paddle deflection so repeated edge hits cannot make the ball skip past paddles; added focused coverage and updated the Pong guide. |
| 2026-09-16 | fix | Made blank Terminal command/path slots active Tab-completion targets after whitespace; added lexer and filesystem-backed coverage and updated the Terminal guide. |
| 2026-09-16 | fix | Made Text Editor dirty state compare against the last loaded or saved content so undo/redo clears and restores the marker accurately; added focused coverage and updated the editor guide. |
| 2026-09-16 | fix | Made Drawing dirty state compare against the last saved canvas so undo/redo clears and restores `[modified]` accurately while resize behavior stays intact; added focused coverage and updated the Drawing guide. |
| 2026-09-16 | fix | Reset Drawing's saved canvas baseline when **New** creates a blank sketch so undo/redo reports `[modified]` accurately after leaving a file-backed drawing; added focused coverage and updated the Drawing guide. |

| 2026-09-16 | fix | Established a clean Drawing baseline when a missing or invalid initial `.modr` open falls back to an untitled canvas, so undo cannot leave a false `[modified]` marker; added focused coverage and updated the Drawing guide. |
| 2026-09-16 | fix | Kept Text Editor Replace and Replace All no-ops out of undo history and the dirty marker when replacement text matches the find text; added focused coverage and updated the editor guide. |
| 2026-09-16 | fix | Kept Drawing Clear no-ops out of undo history and the modified marker when the canvas is already blank; added focused coverage and updated the Drawing guide. |
| 2026-09-16 | fix | Deferred Drawing stroke history until Pen, Eraser, Line, or Rect changes a pixel, so no-op gestures preserve clean state and redo history; added focused coverage and updated the Drawing guide. |
| 2026-09-16 | fix | Kept Text Editor typing and paste operations clean when they exactly reproduce the selected text; added focused coverage and updated the editor guide. |
