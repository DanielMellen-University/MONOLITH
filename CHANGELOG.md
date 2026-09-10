# Changelog

## 2026-09: Keep taskbar hit testing valid on narrow desktops

- Clamped the taskbar button viewport to non-negative logical space and disabled scroll arrows when both controls cannot fit.
- Reset stale taskbar scroll offsets after a desktop shrink, clipped partial button rendering and hit targets to the visible viewport, and prevented the clock tray from overlapping the button strip.
- Added render-backed WindowManager coverage for narrow taskbar geometry.

## 2026-09: Document Drawing file workflows

- Added a compact Drawing file workflow reference for copying, renaming, moving, and opening sketches from Filesystem Browser and Terminal.
- Clarified that renaming a file to `.modr` changes only its name; Drawing still requires a valid MODR payload.

## 2026-09: Reject stale browser file opens

- Filesystem Browser now verifies that a selected entry is a regular file before shell open routing.
- Stale rows and dangling symlinks report a status error instead of creating a failed untitled editor or Drawing window.
- Added browser state coverage for dangling entries.

## 2026-09: Keep dangling symlinks manageable

- `Filesystem::exists()` now recognizes in-root dangling symlinks as directory entries.
- Terminal `rm` can remove those entries, while outside-root symlinks remain rejected.
- Added filesystem and Terminal regression coverage.

## 2026-09: Preserve dangling filesystem entries on rename

- Non-overwriting virtual renames now recognize dangling symlinks as existing destinations.
- Added regression coverage proving both the source and dangling destination remain unchanged.

## 2026-09: Synchronize app sizes after desktop clamping

- Visible apps now receive `onResize` when a logical desktop size change moves or resizes their window frame.
- Added WindowManager coverage for the client dimensions delivered after clamping.

## 2026-09: Keep recursive filesystem operations symlink-safe

- Recursive copy now rejects symlink sources instead of following them into an alias or cycle.
- Recursive delete now removes an in-root symlink itself without deleting the directory tree it targets.
- Added regression coverage for an in-root directory symlink and documented the behavior.

## 2026-09: Clarify Drawing file workflows

- Documented the difference between Save's automatic `.modr` suffix and exact rename or `mv` destinations.
- Added a concrete rename-and-open example and clarified how to create a copy without a Drawing Save As command.

## 2026-09: Preserve Terminal reverse-search caret

- Canceling `Ctrl+R` reverse history search now restores the input caret where the search began instead of moving it to the end of the command.
- Added Terminal state coverage for canceling search from the middle of an input line.

## 2026-09: Reset Filesystem Browser transient state

- Directory navigation and external listing refreshes now clear stale context-menu, rename, and delete-confirmation state.
- Added browser state coverage for changing directories while those transient actions are active.

## 2026-09: Keep windows valid on tiny desktops

- Window clamping now keeps the window origin non-negative when a logical desktop is shorter than the title bar.
- Added a coordinate regression for undersized desktop geometry and documented the shell invariant.

## 2026-09: Keep virtual filesystem paths inside the root

- Host-path conversion now rejects symlink traversal that resolves outside Monolith's configured filesystem root.
- Directory listings omit out-of-root symlinks, preventing normal file operations from following them through the virtual filesystem.
- Added a regression covering outside-target reads, writes, path conversion, and listings.

## 2026-09: Complete Drawing app reference

- Expanded the Drawing guide with the app workflow, prompt behavior, `.modr` format, file lifecycle, keyboard and mouse reference, troubleshooting, and maintenance invariants.
- Documented that default Save names remain unique for large sketch collections and that moved UTF-8 paths keep prompt carets on character boundaries.

## 2026-09: Keep Settings path carets UTF-8 safe

- Shared path editing now clamps caret offsets to complete UTF-8 codepoint boundaries after virtual path remaps.
- Added Settings and shared UTF-8 coverage for moved paths containing multi-byte characters.

## 2026-09: Keep Text Editor UTF-8 cursor boundaries valid

- Vertical cursor movement now snaps to a complete UTF-8 codepoint boundary before editing.
- Added state coverage for moving onto a multi-byte character and inserting text safely.

## 2026-09: Keep Drawing default names unique

- Drawing's suggested Save path now continues past `sketch_999.modr` instead of falling back to an occupied `sketch.modr`.
- Added a boundary regression covering the first 999 occupied sketch names.

## 2026-09: Document Drawing save paths

- Documented that Drawing Save creates missing parent directories while Open requires an existing valid `.modr` file.
- Clarified the distinction between virtual path normalization and filesystem writes.

## 2026-09: Highlight signed editor numbers

- Text Editor syntax highlighting now keeps leading `-` and `+` signs attached to numeric tokens.
- Added state coverage for signed number spans.

## 2026-09: Align architecture app inventory

- Updated the architecture guide to include Pong in the built-in app table, launcher list, and Start menu Games grouping.

## 2026-09: Document Drawing path handling

- Documented canonical virtual path normalization for Drawing save/open prompts and bound file paths, including `.` and `..` examples.
- Clarified that normalized paths are used for titles, session records, singleton routing, and active prompts.

## 2026-09: Normalize virtual path state

- Terminal working directories, Filesystem Browser views, Settings wallpaper prompts, wallpaper state, and shared clipboard paths now remap from canonical virtual paths.
- Legacy wallpaper settings containing redundant path segments are normalized and persisted when loaded.
- Added regression coverage for normalized wallpaper settings.

## 2026-09: Keep app path callbacks canonical

- Text Editor and Drawing now normalize bound-file move callbacks before updating their file paths, titles, and active prompts.
- Settings wallpaper prompts preserve a trailing directory slash when a parent path moves.
- Added focused state coverage for callback normalization and directory prompt preservation.

## 2026-09: Keep open documents attached across virtual moves

- Filesystem Browser rename, Filesystem Browser cut/paste, and Terminal `mv` now update open Text Editor and Drawing bindings when a file or parent directory moves.
- Updated titles, Save targets, session records, and singleton focus routing follow the normalized destination path, including nested files under moved directories.
- Added regression coverage for renamed editor and Drawing files plus nested directory moves.

## 2026-09: Keep desktop path state aligned across virtual moves

- Terminal working directories and Filesystem Browser views now follow moved parent directories.
- Configured wallpaper paths are remapped and persisted when their file or parent directory moves, including an active Settings path prompt.
- Added state coverage for Terminal, Filesystem Browser, Settings, and wallpaper persistence.

## 2026-09: Recover cleanly from virtual path deletion

- Deleting a bound Editor or Drawing file now detaches the live document without discarding its in-memory content, releases its singleton, and restores a tracked untitled window title.
- Terminals and Filesystem Browsers inside a deleted directory return to the nearest existing parent; deleted wallpaper and cut-clipboard paths are cleared.
- Fixed file-backed Drawing windows so **New** correctly claims a normal numbered Drawing title after clearing its binding.
- Added deletion lifecycle coverage for documents, folders, wallpaper, Settings prompts, and working directories.

## 2026-09: Keep file prompts aligned with virtual moves

- Active Text Editor and Drawing Save/Open prompts now follow a moved bound file or directory instead of submitting the old path.
- When a prompted bound path is deleted, the prompt returns to the nearest valid parent while preserving the document buffer or canvas.
- Added focused prompt-state coverage for both apps.

## 2026-09: Keep the shared virtual clipboard aligned

- Copy and Cut paths now follow successful renames and moves from another Filesystem Browser or Terminal, including moved parent directories.
- Cut/paste cleanup now runs before its internal move notification, so a completed paste still clears the cut clipboard normally.
- Added regression coverage for a pending clipboard source that moves between directories.

## 2026-09: Keep windows inside narrow logical desktops

- Window clamping now handles desktops narrower than the normal minimum window width without producing a negative left coordinate.
- Logical desktop dimensions are clamped to positive values and the coordinate regression covers the narrow-layout edge case.

## 2026-09: Keep Text Editor find and replace counts aligned

- Text Editor Find now uses non-overlapping matches, matching Replace All and preventing navigation from reporting entries that replacement would skip.
- Added state coverage for adjacent repeated matches.

## 2026-09: Keep failed file launches untitled

- Failed initial Editor and Drawing opens now use the normal tracked untitled title instead of a misleading file-backed title.
- Failed launches remain eligible for desktop session restore while still avoiding stale file singleton bindings.
- Extended the file-open regression coverage for titles, instance tracking, and session persistence.

## 2026-09: Preserve exact quoted Terminal paths

- Single-path Terminal commands now use the lexer token directly, so quoted filenames with repeated spaces are not collapsed before lookup.
- Added command-state coverage for a filename containing repeated spaces.

## 2026-09: Normalize Terminal file output line endings

- Terminal `cat` now normalizes CRLF and lone-CR separators before writing file contents to scrollback.
- Added command-state coverage for mixed line-ending files.

## 2026-09: Normalize Text Editor file line endings

- Text Editor now normalizes CRLF and lone-CR files on load, matching its existing clipboard behavior and preventing carriage returns from appearing as document content.
- Added state coverage for Windows and classic Mac line endings.

## 2026-09: Keep UI scale persistence aligned with Settings

- Settings now rejects UI scale values that are not one of the three choices exposed by the app, keeping persisted and live values consistent.

## 2026-09: Bind dirty-open confirmation to its path

- Text Editor and Drawing now reset a pending dirty-open confirmation when the requested path changes, preventing an accidental discard for a different file.
- Added paired state coverage for changing the target after the first warning.

## 2026-09: Bound Text Editor undo history

- Text Editor undo history now retains at most 50 snapshots, avoiding an extra entry beyond the documented cap.
- Added state coverage for history growth past the limit.

## 2026-09: Normalize Terminal history line endings

- Terminal history loading now strips carriage returns from CRLF files before commands are recalled.
- Added state coverage for Windows-style history files.

## 2026-09: Read CRLF desktop settings

- Desktop settings now strip carriage returns while loading, so CRLF files preserve wallpaper, clock, scale, and color values.
- Added persistence coverage for Windows-style line endings.

## 2026-09: Preserve empty Text Editor files

- Opening a zero-byte file now creates one editable blank line instead of an extra phantom line.
- Added state coverage for empty-file loading.

## 2026-09: Document Drawing desktop lifecycle

- Documented Drawing close and shutdown guards, session restore behavior, failed-path retry behavior, and one-window-per-file routing.
- Added a contributor reference for the boundaries between raster format code, app state, and WindowManager integration.

## 2026-09: Reset canceled dirty-open confirmations

- Text Editor and Drawing now clear the pending discard arm when a dirty Open prompt is canceled or replaced by a new prompt.
- Added paired state regressions so a later Open cannot discard unsaved work without a fresh confirmation.

## 2026-09: Preserve Filesystem multi-selection through refresh

- Filesystem Browser refreshes now restore all selected entries that remain visible instead of retaining only the primary row.
- Filtering or navigation that changes the active selection now clears stale delete confirmation state.
- Extended the headless Filesystem Browser state check for multi-selection preservation and filtered delete cancellation.

## 2026-09: Clarify Drawing file naming

- Added a single Drawing reference for `.modr` save/open rules, `.mod` text-file routing, and extension behavior when renaming files in the Filesystem Browser.
- Documented that Save appends `.modr` to names without that suffix instead of replacing an existing suffix.

## 2026-09: Add wallpaper path completion

- Settings wallpaper editing now completes virtual directories and BMP filenames with Tab, including shared-prefix completion for multiple matches.
- Added the completion hint to the Settings appearance panel and documented the keyboard behavior.

## 2026-09: Guard filesystem renames against self-descendants

- Renaming now rejects moving the virtual root or a directory into itself or one of its descendants, preserving the source tree instead of relying on host filesystem behavior.
- Extended the shared filesystem roadmap check for descendant and root rename attempts.

## 2026-09: Document Drawing file lifecycle

- Added a focused Drawing reference for new sketches, existing-file saves, failed-operation recovery, resize behavior, and dirty-state confirmation.

## 2026-09: Reset Terminal history navigation after reverse search

- Accepting or canceling Ctrl+R search now clears stale Up/Down navigation state instead of allowing a later Down press to overwrite the search result.
- Extended Terminal state coverage for recalled-command reverse search.

## 2026-09: Reset Filesystem delete confirmation on selection changes

- Ctrl-click and Shift-range selection changes now cancel an armed delete confirmation before the selection can change.
- Extended Filesystem Browser state coverage for both selection paths.

## 2026-09: Stabilize Text Editor Replace All

- Replace All now processes each original match once, even when replacement text contains the search text.
- Added regression coverage for replacement expansion such as `a` to `aa`.

## 2026-09: Re-arm dirty documents after failed saves

- Failed Text Editor and Drawing saves now clear stale discard confirmations, preventing the next close or open from silently discarding unsaved work.
- Extended both headless app-state checks to cover the failed-save recovery path.

## 2026-09: Complete Drawing app documentation

- Clarified prompt focus, RGB confirmation, path completion scope, and the full Drawing verification command set.

## 2026-09: Prune completed Filesystem Browser cuts

- Partial cut/paste now removes successfully moved sources from the shared clipboard while retaining destination-conflicted sources for retry.
- Extended Filesystem Browser state coverage for partial moves.

## 2026-09: Track Drawing resizes as edits

- Resizing a loaded Drawing canvas now marks the document modified so changed dimensions cannot be closed without a save decision.
- Added a headless Drawing state regression for resize dirty tracking and history reset.

## 2026-09: Fix Terminal root path completion

- Terminal `Tab` completion now searches the virtual root when completing an absolute path that starts with `/`.
- Extended Terminal state coverage for root completion.

## 2026-09: Correct Terminal `ls` results

- `ls` now distinguishes empty directories, regular files, and missing paths instead of reporting every non-directory target as `(empty)`.
- Added a headless Terminal filesystem-command regression test.

## 2026-09: Reject unusable filesystem roots

- Filesystem initialization now fails when the configured host root is a regular file instead of a directory.
- Extended the filesystem roadmap check for the invalid-root startup case.

## 2026-09: Retry failed file-backed app opens

- Editor and Drawing no longer reserve singleton file bindings when an initial path is missing, unreadable, or corrupt.
- Added a WindowManager regression test covering retry after an Editor file appears and a Drawing file is repaired.

## 2026-09: Add Drawing keyboard reference

- Added a compact shortcut table and prompt-focus guidance to the Drawing app documentation.

## 2026-09: Reset omitted desktop settings on reload

- Loading a valid partial or legacy desktop settings file now starts from defaults instead of retaining fields from a previous snapshot.
- Extended persistence coverage for reloading into an already-configured settings object.

## 2026-09: Reject malformed desktop color settings

- Desktop background values with trailing data are now rejected instead of being partially parsed.
- Added a persistence regression test that confirms the default color survives malformed input.

## 2026-09: Preserve Minesweeper timer precision across focus changes

- Minesweeper now freezes and resumes elapsed milliseconds instead of rounding to whole seconds on every focus transition.
- Added a headless timing regression test.

## 2026-09: Preserve Text Editor identity on failed Save As

- Save As now refuses a path already bound to another editor and keeps the current path when a destination write fails.
- Added a headless Text Editor state regression test for both failure paths.

## 2026-09: Correct Snake tail collision rules

- Moving into the square the tail vacates is now legal on non-food moves, while moving into that square when food is present still ends the game.
- Added a headless Snake state regression test.

## 2026-09: Drawing app reference completion

- Documented the Drawing startup defaults and exact built-in swatch RGB values so sketches can be reproduced consistently.

## 2026-09: Stabilize Filesystem Browser filtering

- Refreshing or filtering now restores selection by entry name, clears selections that disappeared, and clamps scrolling to the current result count.

## 2026-09: Guard native window shutdown

- Routed the host window's close event through the same dirty-document confirmation used by Start menu Shut Down, preventing unsaved Editor or Drawing work from being skipped.

## 2026-09: Drawing app user guide

- Added a first-session walkthrough, reopen workflow, prompt troubleshooting table, and focused contributor verification commands to the Drawing documentation.

## 2026-09: Complete multi-document shutdown confirmation

- Shut Down now arms every dirty open document in the first request, so confirming once handles multiple Editor or Drawing windows together.

## 2026-09: Explicit filesystem read results

- Added a boolean-output `Filesystem::readFile` overload so empty files and read failures are distinguishable.
- Text Editor, Drawing, Terminal `cat`, and Terminal history loading now use the explicit read result where it matters.

## 2026-09: Verified Terminal file copies

- Terminal `cp` now routes regular files through `Filesystem::copyRecursive`, so a failed source read cannot silently create an empty destination.

## 2026-09: Drawing app documentation

- Added a consolidated Drawing guide with the save contract, `.modr` versus `.mod` routing, persisted canvas state, prompt controls, and recovery behavior.

## 2026-09: Scaled Alt+Tab title overlay

- Alt+Tab now sizes long-title overlays in logical coordinates and clips the native-size label inside the overlay at non-1x content scales.

## 2026-09: Guarded Shut Down

- Start menu Shut Down now honors each open app's dirty-document close guard before exiting, so unsaved Editor and Drawing work requires the same second confirmation as closing a window.

## 2026-09: Bounded Snake and Pong HUD text

- Snake and Pong HUD labels now render at native size and clip inside their HUD strips instead of overflowing narrow game windows.

## 2026-09: Bounded Minesweeper HUD text

- Minesweeper status, difficulty labels, and footer text now stay at native size and clip within their controls instead of covering the face button or extending past the window.

## 2026-09: Native-size Settings text

- Settings information and footer text now render at native size and clip at their panel boundaries instead of being horizontally squeezed.

## 2026-09: Native-size window titles

- Long window titles now render at native text size and clip before the title-bar controls instead of being horizontally squeezed.

## 2026-09: Drawing status and recovery reference

- Added a concise Drawing status-bar guide covering prompts, Tab completion, failed operations, invalid RGB input, and unsaved-change recovery.

## 2026-09: Native-size Filesystem chrome

- Filesystem Browser paths and status messages now stay at native text size and clip within their own regions instead of being horizontally squeezed.

## 2026-09: Clamp restored windows before focus

- Reopening a minimized Editor or Drawing window now re-applies desktop geometry clamping before showing it, keeping stale session rectangles above the taskbar and inside the desktop bounds.

## 2026-09: Drawing app reference guide

- Expanded the Drawing documentation with an at-a-glance reference, status-bar prompt model, exact `.modr` save behavior, and the distinction between persisted pixels and transient editor state.

## 2026-09: Predictable Filesystem listing order

- Filesystem Browser listings now sort names case-insensitively, with directories still grouped before files and raw names used as a deterministic tie-break.

## 2026-09: Verified Filesystem file copies

- Recursive file copies now verify the source size and complete read before writing the destination, preventing read failures from becoming empty files.
- Filesystem documentation now reflects Terminal quoting and all persisted desktop settings.

## 2026-09: Native-size Text Editor status prompts

- Long Find, Replace, Open, Save, and Go-to-line prompts now clip at native text size and keep the active caret visible.

## 2026-09: Forward Delete in Terminal input

- Terminal command input now removes the next complete UTF-8 character with Delete, matching the editor cursor model.

## 2026-09: Caret-aware Text Editor find and replace

- Find and Replace fields now support caret movement, insertion, Delete, and UTF-8-safe Backspace.
- Long search prompts keep the active caret visible without compressing the status bar.

## 2026-09: Caret-aware Terminal history search

- Reverse Ctrl+R search now supports caret movement, insertion, Delete, and UTF-8-safe Backspace.
- Long search queries keep the active caret visible in the input strip.

## 2026-09: Caret-aware Filesystem filtering

- Filesystem Browser Ctrl+F filtering now supports caret movement, insertion, Delete, and UTF-8-safe Backspace.
- Long filter queries stay at native size and keep the active caret visible.

## 2026-09: Caret-aware Drawing prompts

- Drawing Save, Open, and RGB prompts now support caret movement, insertion, Delete, and UTF-8-safe Backspace.
- Long prompts keep the active caret visible without compressing the status-bar text.

## 2026-09: Caret-aware Settings wallpaper paths

- Settings wallpaper path editing now supports caret movement, insertion, Delete, and UTF-8-safe Backspace.
- Long wallpaper paths keep the active caret visible inside the field.

## 2026-09: Caret-aware Text Editor prompts

- Text Editor Save, Open, and Go-to-line prompts now support cursor movement, insertion, Delete, and UTF-8-safe Backspace.
- Path completion edits the final component at the caret without rewriting text after it.

## 2026-09: Better Filesystem Browser rename editing

- Rename mode now supports caret navigation, insertion, Delete, and UTF-8-safe Backspace instead of editing only at the end of the name.
- Long names scroll to keep the active caret visible while renaming.

## 2026-09: Quoted Terminal path completion

- Terminal Tab completion now follows the command lexer for quoted paths and backslash-escaped spaces.
- Completions preserve opening quotes and escape syntax characters when inserted into unquoted input.

## 2026-09: Drawing documentation reference

- Documented Drawing shell routing, minimized-window reuse, session restore, and the distinct behaviors of Save, Open, and RGB prompts.

## 2026-09: Safe Terminal input reset

- Clearing Terminal input with Esc now resets the cursor and saved history state, preventing the next typed character from targeting a stale buffer offset.

## 2026-09: Restore focused file windows

- Reopening an already-open Editor or Drawing file now restores its minimized window before focusing it.

## 2026-09: Reliable minimize focus

- Minimizing the active window now hands keyboard focus to the topmost visible survivor and prevents hidden apps from receiving key events.

## 2026-09: Bounded taskbar scrolling

- Taskbar wheel and arrow scrolling now clamps to the actual window-button strip, preventing all buttons from disappearing after excessive scrolling.

## 2026-09: Native-size taskbar labels

- Taskbar window titles now stay at native size and clip within their own buttons instead of being horizontally squeezed or bleeding into neighbors.

## 2026-09: Native-size text clipping

- Terminal scrollback and Text Editor syntax spans now render at native size and clip at the viewport edge instead of being horizontally squeezed.

## 2026-09: Filesystem long-name rendering

- Filesystem list names now clip at native text size instead of being horizontally squeezed, and long rename buffers keep the caret visible.

## 2026-09: Filesystem context-menu selection

- Filesystem context menus now preserve an existing multi-selection when opened on a selected row and clear stale selection when opened on empty space.

## 2026-09: Terminal long-line editing

- Terminal input now clips to its input bar and follows the cursor horizontally through long commands.

## 2026-09: Retryable partial cut paste

- Filesystem Browser keeps a shared cut clipboard when only part of a multi-item paste succeeds, so conflicting sources can be retried after the destination is fixed.

## 2026-09: Fresh mouse position after release

- WindowManager mouse-up events now update the stored pointer position before later wheel routing.

## 2026-09: Unicode-safe Drawing paths

- Drawing save/open prompts now accept UTF-8 path text and remove complete characters with Backspace.

## 2026-09: Unicode-safe Settings paths

- Settings wallpaper path editing now removes complete UTF-8 characters with Backspace.

## 2026-09: Unicode-safe Terminal editing

- Terminal Left/Right and Backspace now move and erase complete UTF-8 characters instead of raw bytes.
- Reverse history search Backspace uses the same codepoint-safe behavior.

## 2026-09: Reliable client mouse release

- Client apps now retain left-button motion and release routing for an active drag, even when the pointer leaves the window or focus changes.
- Closing a captured window clears the capture safely.

## 2026-09: Scale-aware window input

- Window hit testing, title-bar interaction, dragging, resizing, and client mouse coordinates now convert screen pixels to logical desktop pixels consistently.
- Direct clicks no longer depend on a previous mouse-motion event to establish the correct Y coordinate.

## 2026-09: Drawing app documentation

- Added a complete Drawing workflow guide covering tool behavior, canvas resizing, undo history, `.modr` state, path prompts, and current raster-editor limitations.

## 2026-09: Unicode-safe browser prompts

- Filesystem Browser rename and filter Backspace now removes a complete UTF-8 codepoint instead of one raw byte.
- UTF-8 stepping logic is shared with the Text Editor and covered by a headless regression check.

## 2026-09: Safe multi-item cut/paste

- Filesystem Browser cut/paste now moves sources through a shared non-overwriting rename operation.
- When one destination name conflicts during a multi-item paste, only successful moves complete; conflicting originals are preserved.

## 2026-09: Consistent Drawing extension routing

- `.modr` matching is now case-insensitive across shell open-with routing, Drawing save/open, tab completion, Terminal status text, and Filesystem Browser default open.
- `.mod` remains a Text Editor file and is not treated as a Drawing document.

## 2026-09: Session restore paths

- Session files now quote editor and Drawing paths, preserving virtual filenames with spaces, quotes, or backslashes across restarts.
- Older session files with unquoted path tokens remain readable.

## 2026-09: Filesystem empty-file handling

- Reading zero-byte virtual files is now handled without indexing an empty buffer.
- The filesystem roadmap check covers reading, sizing, and copying empty files.

## 2026-09: Drawing app documentation

- Expanded the Drawing guide with a quick-start workflow, canvas resizing behavior, brush sizes, custom RGB input, path prompt editing, failure handling, and `.modr` validation limits.
- Corrected the unsaved-state indicator to match the `[modified]` status shown by the app.

## 2026-09: Shared Filesystem clipboard

- Copy and cut state is now owned by the desktop shell and shared across Filesystem Browser windows.
- Paste still operates on virtual paths only and does not connect to the host OS clipboard.

## 2026-09: Drawing eyedropper

- The Drawing toolbar now includes Pick, which samples a canvas pixel into the existing custom RGB color and returns to Pen.
- Sampling does not modify pixels or add an undo state, and sampled colors continue to save normally in `.modr` files.

## 2026-09: Text Editor horizontal scrolling

- Long lines now remain fully editable instead of clipping at the right edge.
- The cursor auto-scrolls horizontally while moving or typing; Shift + mouse wheel can pan the text viewport manually.
- Selection, find highlights, syntax colors, and mouse hit testing follow the horizontal viewport.

## 2026-09: Drawing app guide

- Documented `.modr` open routing, including the fact that `.mod` remains a text file.
- Updated Drawing developer references for the split WindowManager source layout.

## 2026-09: Settings interface text scale

- Settings now offers Small (90%), Default (100%), and Large (115%) interface text sizes.
- The shared font updates existing windows immediately, including title bars and the taskbar clock.
- The selected size persists in `~/.monolith/desktop_settings.txt` (`ui_scale_percent=`); older settings files keep the 100% default.

## 2026-09: Terminal quoted arguments

- Command lines accept double and single quotes so paths with spaces work (`cat "/home/monolith/my file.txt"`).
- Backslash escapes work outside quotes and for `\` / `"` inside double quotes.
- Unterminated quotes report a parse error instead of running a half-split command.
- Quoting rules live in `docs/apps/terminal.md`; lexer is `TerminalLexer` with a headless test.

## 2026-09: Pong under Start → Games

- Third native game: **Pong** (player vs AI, first to 5).
- Arrow keys / WASD move the paddle; Space pauses; R restarts after a win or loss.

## 2026-09: Drawing line, rectangle, and custom RGB

- Line and Rect tools paint 1px strokes on the canvas (drag from press to release).
- RGB toolbar control accepts a custom `r,g,b` color beyond the swatch set.
- Line/rect/color and `.modr` encode/decode live in `DrawingRaster` so save/load still round-trips those pixels.

## 2026-09: Filesystem Browser multi-copy, filter, rename

- Copy/Cut apply to the whole multi-selection; Paste writes those items into the current folder (`Filesystem::copyItemsInto`).
- Folder listing can be filtered by name (Ctrl+F / Filter box, case-insensitive substring).
- Rename rejects names that contain `/` (and empty / `.` / `..`) via `Filesystem::renameEntry`.

## 2026-09: Desktop wallpaper images (BMP)

- Settings → Appearance: set a wallpaper image by virtual FS path (text field + Set / Clear).
- Empty path keeps the solid color background; presets still apply underneath the image.
- Wallpaper path persists in `~/.monolith/desktop_settings.txt` (`wallpaper_path=`).
- Shell loads BMP via `SDL_LoadBMP` (no SDL_image); cover-scales over the desktop; fails soft on missing/bad files.
- Sample image seeded at `/Wallpapers/sample.bmp` from `assets/wallpapers/sample.bmp`.

## 2026-09: Taskbar clock 12/24-hour toggle

- Settings → Appearance: choose **12-hour** (default) or **24-hour** for the taskbar clock.
- Choice persists in `~/.monolith/desktop_settings.txt` (`clock_24_hour=0|1`) with older files still loading as 12-hour.
- Taskbar clock text updates immediately when the format changes.

## 2026-09: Taskbar clock

- Local-time clock on the right of the taskbar (12-hour by default, updates each minute).
- Hover the clock to see the full local date.

## 2026-07: Alt+Tab switcher and Start hotkey

- **Alt+Tab** / **Alt+Shift+Tab** cycles windows (restores minimized; overlay until Alt up).
- **Ctrl+Escape** toggles the Start menu.

## 2026-07: Editor go-to-line and coalesced undo

- **Ctrl+G** opens a status-bar line-number prompt (Enter jumps, Esc cancels).
- Consecutive typing or in-line backspace within ~1s shares one undo snapshot.

## Current (2026-07)

Focused snapshot of Monolith as of the latest `beta` / `main` tip. Older detail lives in git history.

### Desktop shell
- Overlapping windows: drag, 8-way resize, minimize / maximize, z-order, focus-after-close
- Taskbar with Start menu (**Games** category), scroll arrows + wheel when crowded, local-time clock with 12/24-hour setting (hover for date)
- Multi-instance titles with live compaction (`Terminal`, `Terminal 2`, …)
- Session restore: `~/.monolith/session.txt` on exit; restored on next launch (demo set if missing)
- Open-with routing: `.modr` → Drawing, else Text Editor (`openPath`)
- Desktop background color presets and BMP wallpaper path via Settings (`~/.monolith/desktop_settings.txt`)
- Controllers owned per window; `App::allowClose` for dirty-document guards

### Filesystem
- Virtual FS under `~/.monolith/fs/` with shared recursive copy/remove, path helpers, `fileSize`
- Terminal: shell commands, capped scrollback/history, safer `touch`, `open` uses shell routing
- Filesystem Browser: multi-select, properties, recursive delete with confirm, open-with menu, cut/copy/paste

### Productivity apps
- **Text Editor**: UTF-8, selection + clipboard, syntax highlight, find (**Ctrl+F**) and replace (**Ctrl+H**), dirty close/open
- **Drawing**: pen/eraser/fill, `.modr` save/load, undo, dirty close/new/open, efficient texture upload
- **Settings**: appearance swatches, BMP wallpaper path, taskbar clock 12/24-hour toggle, about / environment panel

### Games
- **Snake**, **Minesweeper**, and **Pong** under Start → Games (high scores / best times on host; Minesweeper pauses timer when unfocused)

### Build
- C++23, CMake, SDL2 + SDL2_ttf; see README for package names

Historical feature-by-feature entries were cleared to keep this file short. Use `git log` for full history.
