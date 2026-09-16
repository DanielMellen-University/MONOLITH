# Changelog

## 2026-09: Refresh taskbar targets when focus clears

- Invalidate cached taskbar rectangles when minimizing leaves the desktop with no visible focused window, keeping focused-first button ordering aligned before the next render.
- Add geometry coverage for minimizing the last visible taskbar window while other windows remain minimized.

## 2026-09: Keep client keyboard events owned by focused apps

- Stop keyboard and text-input events after the focused app receives them, preventing a callback-triggered close from bubbling the same event into desktop-icon activation.
- Add lifecycle coverage for a focused app that closes on Enter while a desktop icon is selected.

## 2026-09: Invalidate taskbar targets when windows minimize

- Clear cached taskbar hit targets at the minimize transition, so queued input between frames sees the updated window state even when no focus handoff occurs.
- Add a no-render-gap regression around minimizing a maximized window.

## 2026-09: Restore maximized windows against the current desktop

- Reapply the current usable desktop rectangle when bringing a minimized maximized window forward, so desktop growth cannot leave it at stale dimensions for a frame.
- Add geometry coverage for minimizing a maximized window, growing the desktop, and restoring it through the shell.

## 2026-09: Isolate WindowManager draw color state

- Restore SDL draw color after each app callback and after the full WindowManager frame, preventing app-owned renderer state from leaking into later shell composition or the caller.
- Add headless coverage for inter-app isolation and exact caller draw-color restoration.

## 2026-09: Isolate WindowManager blend state

- Compose WindowManager frames with a neutral SDL draw blend mode, restore it after each app callback, and return the caller's original mode after rendering.
- Add a regression probe for an app that leaks blend state into the following window.

## 2026-09: Keep refined reverse searches on matching entries

- Restart Terminal reverse-search matching from the newest entry after query edits, so adding or removing text does not skip a command that still matches.
- Keep repeated `Ctrl+R` as the explicit older-match traversal and add regression coverage for query refinement.

## 2026-09: Preserve renderer blend state across game overlays

- Restore the caller's SDL draw blend mode after Snake and Minesweeper render translucent end-state overlays instead of forcing it to `NONE`.
- Add direct renderer-state coverage for both games and document the shared rendering contract.

## 2026-09: Trim Terminal history in bounded batches

- Keep scrollback and command history caps unchanged while removing excess entries in one range operation instead of shifting the vector once per dropped line.
- Add focused coverage for retaining the newest scrollback and command-history entries.

## 2026-09: Tear down SDL-backed apps before SDL shutdown

- Scope the Window Manager and hosted apps before renderer, font, and SDL cleanup so their texture destructors run against live SDL resources.
- Add a static lifecycle check to the headless verification runner and document the ownership boundary.

## 2026-09: Restore Terminal draft caret after history navigation

- Preserve the input caret alongside an untouched draft while navigating command history, so Down restores both the text and its editing position.
- Add focused Terminal regression coverage and document the behavior.

## 2026-09: Keep identical Text Editor replacements clean

- Treat typing or pasting the exact selected text as a selection collapse instead of a document edit, preserving clean state and undo history.
- Add focused coverage for both input paths and update the editor guide.

## 2026-09: Keep no-op Drawing strokes clean

- Defer stroke history capture until Pen, Eraser, Line, or Rect actually changes a pixel, preserving clean state and redo history for no-op gestures.
- Add focused eraser coverage and document changed-stroke history behavior.

## 2026-09: Keep no-op Drawing clears clean

- Treat Clear on an already blank canvas as a no-op, preserving the clean marker and undo history.
- Add focused Drawing state coverage and document the behavior.

## 2026-09: Keep no-op Text Editor replaces clean

- Treat Replace and Replace All operations whose replacement equals the find text as no-ops, so they do not create undo entries or a false dirty marker.
- Add focused coverage for clean-state and history preservation.

## 2026-09: Keep failed Drawing opens clean

- Establish a clean blank baseline when a missing or invalid initial `.modr` path falls back to an untitled Drawing window, so undo does not leave a false `[modified]` marker.
- Add focused coverage for failed initial opens and undo.

## 2026-09: Reset Drawing's saved baseline for New sketches

- Give a blank sketch created by **New** its own saved baseline so undoing a new edit clears `[modified]` instead of comparing against the previous file.
- Add focused coverage for the New, undo, and redo transition.

## 2026-09: Keep Drawing's modified marker truthful

- Track the last saved canvas dimensions and pixels so undo and redo clear or restore `[modified]` accurately.
- Preserve the existing rule that resizing a file-backed canvas is a modification, with focused coverage for saved-canvas undo and redo.

## 2026-09: Keep the Text Editor dirty marker truthful

- Track the last loaded or saved document content so undo and redo update the dirty state instead of always leaving the editor marked unsaved.
- Added coverage for undoing to loaded content, redoing the edit, and undoing back to a post-save baseline.

## 2026-09: Complete blank Terminal tokens

- Let Tab completion treat the empty token after whitespace as an active command or path slot, so commands such as `cd ` can complete from the current working directory.
- Added lexer and filesystem-backed coverage for empty-prefix completion while preserving closed-quote behavior.

## 2026-09: Keep Pong ball speed bounded

- Cap vertical velocity after paddle deflection so repeated edge hits cannot make the ball skip past paddles.
- Add headless coverage for the rebound direction and speed ceiling.

## 2026-09: Keep Terminal reverse search moving backward

- Preserved the active reverse-search match while extending a query, so repeated `Ctrl+R` now walks older matching commands instead of resetting to the newest result.
- Added focused coverage for three-match reverse-search navigation and documented the behavior.

## 2026-09: Keep Settings controls responsive across layout changes

- Rebuilt Settings swatch, wallpaper, clock, and text-size hit targets on demand when queued input arrives after a resize, interface-scale change, or scroll update but before the next render.
- Corrected Settings click coordinates after scrolling so visible controls remain actionable instead of applying the scroll offset twice.
- Shared the Settings control geometry between rendering and input, with focused coverage for pre-render scaled clicks across the appearance controls.

## 2026-09: Keep Drawing controls responsive across layout changes

- Rebuilt Drawing toolbar and color-swatch hit targets on demand when queued input arrives after a resize or interface-scale change but before the next render.
- Shared Drawing toolbar geometry between rendering and input, with focused coverage for pre-render scaled toolbar and swatch clicks.

## 2026-09: Keep Browser controls responsive across layout changes

- Rebuilt Filesystem Browser toolbar and filter hit targets on demand when input arrives after a resize or interface-scale change but before the next render.
- Shared the toolbar geometry between drawing and input, with focused coverage for a pre-render scaled Filter click.

## 2026-09: Keep taskbar controls aligned with their layout

- Routed Start-button and taskbar-arrow input through the same computed rectangles used by rendering, including scaled and narrow taskbars.
- Added coordinate coverage for pre-render scaled Start-button input and clicks outside the visible button band.

## 2026-09: Make taskbar input independent of frame timing

- Rebuilt taskbar button and scroll-arrow hit targets on demand when queued input arrives before the next render.
- Shared taskbar geometry between rendering and hit testing, with regression coverage for pre-render taskbar activation.

## 2026-09: Make Start-menu input independent of frame timing

- Rebuilt Start-menu geometry and actionable rows on demand when queued keyboard or mouse input arrives before the next render.
- Added coordinate coverage for pre-render keyboard selection and popup clicks.

## 2026-09: Keep Start-menu focus handoffs single-step

- Prevented Start-menu dismissal from briefly resuming the previously focused app before a click, taskbar action, or Alt+Tab hands focus to another window.
- Added focused lifecycle coverage for cross-window menu dismissal and documented the modal focus callback contract.

## 2026-09: Retry wallpapers after file creation

- Invalidated the cached wallpaper load when a missing configured image or one of its parent directories is created.
- Added WindowManager coverage for recovering a wallpaper path after creation.

## 2026-09: Keep session geometry attached to its launcher

- Applied restored geometry to the window ID created for each session entry instead of assuming the newest vector element is the launched window.
- Preserved correct session restore behavior when app creation callbacks open additional windows.

## 2026-09: Create parents for direct filesystem copies

- Made `Filesystem::copyRecursive` create missing destination parent directories for direct file copies, matching its documented tree-copy behavior.
- Added regression coverage for copying a file into a new nested destination.

## 2026-09: Keep lifecycle regressions sanitizer-clean

- Moved reentrant close assertions onto external test state so lifecycle coverage never reads an app after its window has been destroyed.

## 2026-09: Reacquire windows after reentrant focus loss

- Re-found the closing window after its `onFocusLost()` callback, so a callback that closes a sibling cannot invalidate the Window Manager's erase iterator.
- Added lifecycle coverage for sibling closes during focus-loss handling.

## 2026-09: Follow moved directories in active prompts

- Remapped Text Editor and Drawing Open/Save prompts when a directory in the prompt moves, including normalized paths and UTF-8-safe caret positions.
- Added focused coverage for directory moves in both apps.

## 2026-09: Repair prompts after parent deletion

- Passed the actual removed directory into bound Editor and Drawing windows when a parent tree is deleted, so active path prompts return to the nearest surviving parent instead of retaining a dead path.
- Added integration coverage for a bound Editor below a deleted directory.

## 2026-09: Align Replace All with Find matches

- Made Replace All consume the same non-overlapping match list as Find, so overlapping candidates such as `aa` in `aaa` resolve from the documented first match instead of selecting the last byte offset.
- Added regression coverage for overlapping candidates.

## 2026-09: Render shell titles as UTF-8

- Switched window-title and taskbar-label rendering to SDL_ttf's UTF-8 API so Unicode filenames display consistently with their measured widths.
- Added render-level coverage for a non-ASCII file-backed title.

## 2026-09: Keep outside pointer coordinates outside the desktop

- Floored scaled screen-to-logical pointer conversion so positions just above a host header or left of the desktop cannot hit an edge window.
- Added regression coverage for negative and header-offset coordinate boundaries.

## 2026-09: Preserve direct Browser renames

- Cleared inline rename state before broadcasting a successful rename, preventing the originating Browser's synchronous refresh from canceling the operation or losing its new selection and status message.
- Added regression coverage for direct renames through a re-entrant filesystem notification.

## 2026-09: Block app input while the host is unfocused

- Stopped queued key, text-input, motion, button, and wheel events from reaching the focused app or shell after the SDL host window loses focus, while preserving shell-owned key-release cleanup and focus recovery.
- Added keyboard, text-input, and pointer regression coverage for the host-unfocused state.

## 2026-09: Refresh wheel routing from the host pointer

- Updated WindowManager wheel handling to refresh SDL's current pointer position before deciding whether the taskbar or focused client owns the scroll.
- Added SDL-backed regression coverage for scrolling over a client without a preceding motion event.

## 2026-09: Enforce the shell render clip

- Applied the caller renderer clip across wallpaper, desktop icons, window chrome, and taskbar drawing instead of only restoring it after nested rendering.
- Added pixel-level coverage that rejects shell pixels outside a parent clip.

## 2026-09: Invalidate taskbar targets after scrolling

- Cleared cached taskbar rectangles after arrow and wheel scrolling so rapid input cannot activate buttons from the previous viewport before the next render.
- Added focused coverage for both taskbar scroll paths.

## 2026-09: Route binding titles through shell cache ownership

- Covered failed opens and file-binding transitions that rename bare Editor or Drawing windows, keeping title textures and taskbar hit targets synchronized there too.

## 2026-09: Invalidate taskbar targets after window changes

- Cleared cached taskbar and shell hit targets when windows are created, focused, closed, or renamed, preventing between-frame clicks from using obsolete window geometry.
- Centralized window-title updates so taskbar text caches and hit targets stay synchronized.

## 2026-09: Own SDL text input lifecycle

- Explicitly started SDL text input for the executable and stopped it during shutdown, ensuring printable input reaches native apps without relying on inherited SDL state.
- Added the matching deterministic compressor for compressed main-loop body fragments.

## 2026-09: Clarify focused SDL verification

- Documented the dummy SDL video and audio drivers needed when running an individual SDL-linked headless test outside the complete verification runner.

## 2026-09: Cover shell-owned hotkey boundaries

- Extended WindowManager input coverage to assert that `Alt+Tab` and `Ctrl+Escape` keydowns, as well as their releases, stay owned by the shell instead of reaching the focused app.

## 2026-09: Add Settings fragment compressor

- Added the checked-in counterpart to the Settings fragment decompressor, making source edits reproducibly round-trip between readable `.inc` files and tracked `.inc.z64` files.
- Documented the compressed-source workflow for contributors and agents.

## 2026-09: Invalidate stale Settings hit targets

- Cleared cached Settings swatch, clock, interface-scale, and wallpaper-control hit rectangles on client resize, interface text-scale changes, and scroll movement, preventing pre-render clicks from activating controls at their old positions.
- Added Settings state coverage for resize, scale, wheel, Home, and End invalidation paths.

## 2026-09: Invalidate stale Drawing hit targets

- Cleared cached Drawing toolbar and color-swatch hit rectangles on client resize and interface text-scale changes, preventing pre-render clicks from activating controls at their old positions.
- Added Drawing state coverage for both invalidation paths.

## 2026-09: Invalidate stale browser hit targets

- Cleared cached Filesystem Browser toolbar and filter hit rectangles on client resize and interface text-scale changes, preventing pre-render clicks from activating controls at their old positions.
- Added browser state coverage for both invalidation paths.

## 2026-09: Invalidate stale shell hit targets

- Cleared cached taskbar, clock, and Start-menu hit rectangles when desktop geometry, display scale, clock format, or interface font metrics change, preventing a pre-render event from activating a control at its old screen position.
- Added window-coordinate coverage for clock-format, content-scale, font-scale, and desktop-resize invalidation.

## 2026-09: Make Start menu keyboard-complete

- The existing Start menu now supports Up/Down selection, Enter activation, and Escape dismissal after opening with `Ctrl+Escape`.
- Keyboard selection wraps across every actionable menu item and shares the same activation path as mouse clicks.
- Added headless coverage for selection movement, wrapping, dismissal, and command activation.

## 2026-09: Defer re-entrant app closes

- WindowManager now defers window destruction requested from an app callback until the outermost callback returns, preventing synchronous observers, focus handlers, resize callbacks, input handlers, and updates from destroying the app that is still executing.
- Added lifecycle regression coverage for an observer closing the source app during a synchronous virtual-path notification.

## 2026-09: Share virtual and host atomic writes

- Virtual filesystem writes now use the same binary-capable atomic writer as settings, sessions, and game records, removing a duplicated replacement path while preserving permission, symlink, and failure cleanup behavior.
- Added regression coverage for embedded NUL bytes in virtual files.

## 2026-09: Clean up failed atomic serializers

- Shared host snapshots now convert serializer exceptions into a failed save and remove the temporary sibling, preserving the boolean persistence contract.
- Added regression coverage for cleanup after a throwing serializer.

## 2026-09: Keep Browser text inside caller clips

- Intersected Filesystem Browser path, filter, filename, rename-cursor, and status text clips with the caller renderer clip instead of temporarily widening the render boundary.
- Added pixel-level headless coverage that rejects text escaping a one-pixel caller clip.

## 2026-09: Use shared clip intersections in shell boundaries

- Routed the WindowManager client boundary and Filesystem Browser context menu through the shared renderer clip intersection helper, removing the last duplicate shell-side intersection logic.

## 2026-09: Share renderer clip handling across games

- Routed Pong, Snake, Minesweeper, and Breakout text clipping through the shared renderer boundary helper, removing four duplicate capture/intersection/restore implementations.

## 2026-09: Share Settings renderer clip handling

- Routed Settings' generated clip save, intersection, and restore paths through the shared renderer boundary helper for clipped labels, the wallpaper field, and scrolling content.

## 2026-09: Share renderer clip handling

- Centralized caller-clip capture, intersection, and restoration for WindowManager, Terminal, Text Editor, Filesystem Browser, and Drawing.
- Routed Settings completion through the same shared UTF-8 common-prefix helper and regenerated its compressed source fragment.

## 2026-09: Share UTF-8 completion prefixes

- Centralized complete UTF-8 common-prefix completion in the shared app helper so Text Editor, Drawing, and Terminal use one boundary-safe rule.

## 2026-09: Centralize UTF-8 completion safety

- Shared the complete-codepoint completion guard across Terminal, Text Editor, Drawing, and Settings so future path-completion changes keep the same input invariant.
- Added direct helper coverage to the UTF-8 unit test.

## 2026-09: Keep terminal completion UTF-8 safe

- Terminal path completion now stops ambiguous shared prefixes at complete UTF-8 codepoints, preventing a shared leading byte from becoming invalid input.
- Added Unicode ambiguity coverage to the Terminal filesystem test.

## 2026-09: Keep drawing and settings completion UTF-8 safe

- Drawing and Settings path prompts now stop ambiguous completions at complete UTF-8 codepoints, preventing a shared leading byte from becoming invalid text.
- Added Unicode ambiguity coverage to both path-completion tests.

## 2026-09: Cancel stale browser renames after refresh

- Filesystem Browser refreshes now cancel inline rename state before replacing the directory listing, preventing a later Enter from renaming a different row after an external filesystem change.
- Added regression coverage for external refreshes during rename mode.

## 2026-09: Clamp editor horizontal scroll after resize

- Text Editor horizontal scrolling now clamps after wheel input and client resizing, preventing a stale pixel offset from leaving a widened editor blank.
- Added regression coverage for widening a client while a horizontal offset is active.

## 2026-09: Make Drawing Fill linear in the connected region

- The Fill tool now marks pixels as soon as they enter its work list, avoiding duplicate queue entries on large flat canvases.
- Added state coverage that verifies a connected canvas is filled without gaps.

## 2026-09: Contain the taskbar clock tooltip

- The clock's date tooltip now stays within the usable desktop on short and narrow clients instead of being positioned above the rendered surface.
- Tooltip rendering preserves the caller's clip and has focused geometry coverage for undersized desktops.

## 2026-09: Contain Filesystem Browser context menus

- Context menus now clamp to the browser client instead of keeping a fixed minimum width that can extend off-screen in narrow windows.
- Menu rendering clips long labels and partially visible rows while preserving the caller's renderer clip.

## 2026-09: Contain the Start menu on undersized desktops

- Start menu geometry now stays within the logical desktop and the usable area above the taskbar, even when a test or future host provides an unusually small desktop.
- Menu hit targets are clipped to the visible popup so hidden rows cannot be activated outside the rendered menu.

## 2026-09: Enforce minimum frame size at window creation

- Direct WindowManager-created frames now honor the same minimum width and height already used by resize and session restore paths.
- Added coverage that the new app receives the resulting clamped client dimensions.

## 2026-09: Contain Minesweeper controls on extreme narrow clients

- The face button now yields to the actual client width, and difficulty buttons hide when no non-overlapping slot remains instead of painting over the face control.
- Added tiny-client coverage for the contained face button and keyboard-only difficulty fallback.

## 2026-09: Contain the Start button on tiny desktops

- The taskbar Start button now clips to the logical desktop width and ignores clicks beyond that edge on undersized desktops.
- Added rendering and input coverage for the narrow-taskbar boundary.

## 2026-09: Keep desktop icon labels inside their column

- Desktop icons now reserve a stable label-width hit cell, preventing longer labels such as Filesystem from bleeding into the workspace.
- Narrow desktops omit icons when the label column cannot fit horizontally.

## 2026-09: Keep editor path completion UTF-8 safe

- Ctrl+O and Save As completion now stop at complete Unicode codepoints when multiple filenames share a multibyte prefix.
- Added coverage for ambiguous and exact Unicode path completions.

## 2026-09: Preserve intentionally empty sessions

- A valid session containing only stale or no entries now keeps the desktop empty instead of triggering the demo-window fallback.
- Session loading still rejects missing files and invalid session headers.

## 2026-09: Skip stale file-backed session entries

- Session restore now skips missing or invalid file-backed Editor and Drawing entries instead of presenting blank untitled windows as successful restores.
- Added coverage for missing editor files and corrupt `.modr` session entries.

## 2026-09: Claim new files before notifications

- Text Editor and Drawing now register a newly saved file binding before broadcasting its creation, so synchronous observers focus the saving window instead of opening a duplicate.
- Added event-order coverage for both apps.

## 2026-09: Keep Drawing file singletons coherent

- Drawing Open and Save prompts now focus or reject an existing `.modr` owner before changing the current canvas or binding, matching the Text Editor singleton behavior.
- Added Drawing state coverage for duplicate Open and Save destinations.

## 2026-09: Recheck callback-created windows before shutdown

- Shutdown validation now revisits windows opened by an app's `allowClose()` callback before accepting quit, preventing a newly created dirty document from bypassing the close contract.
- Added a regression that opens and dirties an editor during shutdown validation.

## 2026-09: Guard reentrant window closes

- WindowManager now rechecks window identity after `allowClose()` and focus-loss callbacks, so an app that closes its own window during either callback cannot leave the outer close operation using a destroyed target.
- Added lifecycle coverage for both recursive close paths.

## 2026-09: Guard bound-file remaps

- Editor and Drawing binding updates now verify window identity and the expected old path before invoking callbacks, so a callback-triggered sibling close cannot invalidate the remaining remap or removal pass.
- Added lifecycle coverage for a bound-file callback closing a sibling window.

## 2026-09: Snapshot the render pass

- WindowManager now renders from a live window identity snapshot, so an app can close itself during `render()` without invalidating the frame or leaving the shell to draw a destroyed window.
- Added renderer-backed coverage for front-window removal during rendering.

## 2026-09: Guard session restore callbacks

- WindowManager now verifies restored window identity after `onResize()`, so a callback-triggered close cannot make session restore inspect a dead window or report it as successfully restored.
- Added lifecycle coverage for session geometry callbacks that remove their window.

## 2026-09: Snapshot shutdown validation

- WindowManager now validates `App::allowClose()` through the live-window snapshot, so an app can change the window set during shutdown validation without invalidating the shell's pass.
- Added regression coverage for an app opening an editor while a dirty sibling is still checked.

## 2026-09: Keep modal focus handoffs honest

- WindowManager now suppresses focus gains while the host or Start menu is inactive and resumes the current logical window after callback-triggered changes, preventing apps behind the modal menu from receiving active focus early.
- Host focus-loss dispatch now stays attached to the window that was active when the transition began, even if a captured mouse release closes it.
- Added regression coverage for Start-menu focus suspension with callback-triggered close.

## 2026-09: Guard shell input after app callbacks

- WindowManager now verifies window identity after initial sizing, resize callbacks, and focus-triggering input activation, preventing a self-closing app from leaving the shell to dereference a stale event target.
- Extended lifecycle coverage for click activation and callback-triggered target removal.

## 2026-09: Stabilize focus callback handoff

- WindowManager now publishes focus before lifecycle callbacks and verifies the target remains live, so an app can close itself from `onFocusGained()` without leaving a dangling focused window.
- Added focused regression coverage for callback-triggered focus handoff.

## 2026-09: Stabilize resize callback dispatch

- WindowManager now uses the lifecycle snapshot for logical desktop and maximized-window resize callbacks, preventing app-triggered window changes from invalidating geometry passes.
- Extended lifecycle regression coverage to include resize-time self-closes.

## 2026-09: Stabilize lifecycle broadcasts

- WindowManager now snapshots live apps before virtual-path and UI-scale callbacks, so an app can close or open windows during a notification without invalidating the broadcast loop.
- Extended lifecycle regression coverage to include virtual-path notifications and documented the callback contract.

## 2026-09: Stabilize app update dispatch

- WindowManager now snapshots live windows before calling `App::update()`, so an app can close itself through its controller without invalidating the update loop or skipping surviving apps.
- Added focused lifecycle coverage and documented the dispatch contract.

## 2026-09: Invalidate closed taskbar targets

- WindowManager now removes a closing window from the cached taskbar hit targets before destroying it, covering app-triggered closes that happen between render frames.
- Added regression coverage to verify the stale target is removed.

## 2026-09: Share game frame timing

- Pong and Breakout now use one wrap-safe SDL tick conversion helper with the same 50 ms stalled-frame cap, removing duplicated timing logic.
- Added focused coverage for normal intervals, the 32-bit tick wrap boundary, unchanged ticks, and long stalls.

## 2026-09: Complete apostrophe-safe Terminal paths

- Terminal Tab completion now preserves filenames containing apostrophes inside single-quoted paths and closes completed single-quoted file paths at the line end.
- Added lexer and Terminal state coverage for the encoded one-argument form.

## 2026-09: Reject unsafe atomic temp entries

- Virtual file writes and host text snapshots now reject symlinked or non-regular `.tmp` siblings before replacement.
- Added regression coverage that preserves both destination records and symlink targets.

## 2026-09: Isolate game randomness

- Snake and Minesweeper now keep independent random streams instead of sharing the process-global C RNG.
- Bounded food and mine coordinates now use standard uniform distributions, and the helper has focused headless coverage.
- Documented the random-state boundary in both game guides and the development script reference.

## 2026-09: Document complete Drawing verification

- Added the one-command headless suite to the Drawing developer guide alongside the focused raster, state, integration, and smoke checks.
- Documented when to use focused Drawing checks versus the complete repository runner.

## 2026-09: Add one-command headless verification

- Added `scripts/run_headless_tests.sh` to build and run the documented static checks and state tests with consistent SDL dummy drivers.
- Kept the individual development commands documented for focused iteration while making full-suite verification reproducible from the repository root.

## 2026-09: Keep Snake effects correct across tick wraparound

- Snake's food flash now uses wrap-safe SDL tick deadline arithmetic, so the brief visual effect remains correct when the 32-bit tick counter rolls over.
- Added focused coverage for active, expired, and exact-expiry deadlines around the wrap boundary.

## 2026-09: Unlink filesystem symlinks safely

- `remove` and `removeRecursive` now delete symlink entries themselves instead of resolving an in-root link and deleting its target file or directory.
- Direct outside-root symlink entries can be removed safely without exposing or modifying their targets.
- Rename now moves an in-root symlink entry without moving the target it references.
- Added regression coverage for in-root file links and direct outside-root links.

## 2026-09: Report empty Editor completions

- Text Editor path prompts now report `No path matches.` when Tab completion finds no eligible entry, matching Drawing and Terminal feedback.
- Added focused state coverage for the no-match completion path.

## 2026-09: Align Minesweeper Enter controls

- Minesweeper now accepts keypad Enter anywhere the documented Enter shortcut starts a new game.
- Added focused input coverage for resetting a board with keypad Enter.

## 2026-09: Align Snake Enter controls

- Snake now accepts keypad Enter anywhere the documented Enter shortcut resumes or restarts a match, matching Pong and Breakout.
- Added focused input coverage for resuming a paused game with keypad Enter.

## 2026-09: Unify host text persistence

- Settings, session restore, Snake scores, and Minesweeper best times now share one temporary-sibling text writer, so complete records replace the target atomically and failed replacements clean up without truncating the previous state.
- Existing regular host snapshots retain their permission bits when replaced.
- Added game persistence coverage for parent-directory creation, reloads, temporary-file cleanup, and blocked targets.

## 2026-09: Make virtual file writes atomic

- Filesystem writes now replace regular files only after the complete byte stream succeeds, preserving existing permission bits and preventing failed Editor, Drawing, Terminal, or seed writes from truncating the prior file.
- In-root file symlinks continue to update their targets, while outside-root symlinks remain rejected.
- Added regression coverage for successful overwrite, failed replacement cleanup, and in-root symlink writes.

## 2026-09: Protect desktop settings snapshots

- Desktop preferences now write to a temporary sibling and replace the live settings file atomically after the complete stream succeeds, so a failed preference save cannot leave a truncated configuration.
- Added regression coverage for successful replacement and failed replacement cleanup.

## 2026-09: Protect session snapshots during shutdown

- Session restore data is now written to a temporary sibling and atomically replaces the live snapshot only after the full stream succeeds, so an interrupted shutdown cannot leave a truncated session file.
- Added regression coverage for successful replacement and failed replacement cleanup.

## 2026-09: Make the Start menu a modal focus boundary

- Opening Start now pauses and suspends the previously focused visible app instead of only hiding its keyboard events; closing Start restores that app only when it still owns focus.
- Host focus changes while Start is open no longer resume the app behind the menu, preventing stuck game controls and background updates.

## 2026-09: Keep shell hotkey releases out of clients

- The Window Manager now consumes the `Tab` and `Escape` releases paired with shell-owned `Alt+Tab` and `Ctrl+Escape` keydowns, preventing half-delivered gestures in client apps.
- Added focused regression coverage for both shell hotkey release paths.

## 2026-09: Keep Alt+Tab key state inside the shell

- The Window Manager now consumes the Alt release that ends an Alt+Tab cycle instead of forwarding it to the focused app.
- Added mouse-capture regression coverage for the shell-owned key release.

## 2026-09: Preserve Browser selection across external renames

- Filesystem Browser now keeps a selected direct child, including its primary selection and Shift-selection anchor, when another app renames that child in the visible folder.
- Added regression coverage for selection preservation during an externally reported rename.

## 2026-09: Refresh changed Browser directories

- Filesystem Browser now refreshes the directory it is currently viewing when that directory itself receives an external change event.
- Added regression coverage for a new child appearing during a directory-level update.

## 2026-09: Report recursive copy changes

- Terminal `cp -r` now broadcasts every corresponding path when merging into an existing directory tree, keeping open listings and bound documents synchronized with nested overwrites.
- Added regression coverage for nested recursive-copy notifications.

## 2026-09: Recover pointer state after host focus loss

- SDL host-window focus loss now ends an active client drag with a matching synthetic release and clears shell/frame capture state.
- Focus callbacks now follow host focus loss and gain, with regression coverage for Drawing-style pointer capture behavior.

## 2026-09: Release bare app slots after Save As

- Saving an untitled Editor or Drawing now removes its old numbered bare-app reservation, so file-backed windows no longer distort later instance titles.
- Remaining bare Editor and Drawing windows compact immediately after that transition, with regression coverage for both app types.

## 2026-09: Prevent orphaned client mouse releases

- Window-frame and empty-desktop clicks no longer fall through as `SDL_MOUSEBUTTONUP` events to whichever app is focused afterward.
- Added Window Manager mouse-capture coverage for empty desktop and close-button interactions.

## 2026-09: Keep shell motion out of client apps

- Taskbar, empty-desktop, and window-frame drags no longer forward pointer motion to the focused client unless that client owns the active press.
- Added regression coverage for shell and frame motion routing.

## 2026-09: Align architecture documentation with the shipped shell

- Updated the architecture guide to include Breakout and describe the runtime logical desktop size used by the current Window Manager.

## 2026-09: Protect the virtual filesystem root

- Non-recursive `Filesystem::remove` now rejects `/` just like recursive removal and rename, preserving Monolith's root directory invariant.
- Added regression coverage and documented the root guard.

## 2026-09: Keep the first desktop icon row reachable

- Corrected narrow-desktop icon layout so the first icon is rendered when its tile and label exactly fit above the taskbar.
- Added exact-fit and one-pixel-short layout coverage.

## 2026-09: Handle desktop-icon timer wraparound

- Desktop-icon double-click detection now handles SDL's wrapping 32-bit tick counter without dropping a valid click pair.
- Added a wraparound regression case to the desktop-icon test.

## 2026-09: Clear desktop-icon state across shell actions

- Start menu, taskbar, and Alt+Tab interactions now clear pending desktop-icon selection and double-click history, and launching an icon clears it as well.
- Added event-level regression coverage for Start and taskbar interactions.

## 2026-09: Reset stale desktop-icon click state

- WindowManager now clears pending desktop-icon double-click history whenever icon selection is cleared, preventing an intervening window click from causing a false launch.
- Added regression coverage and updated the desktop-icon behavior note.

## 2026-09: Keep verification scripts runnable

- Restored executable mode for the desktop-icon and games integration checks.
- Updated the Drawing integration check for compressed main-body sources and documented the generated Settings include path.

## 2026-09: Preserve WindowManager renderer boundaries

- WindowManager now preserves and intersects the caller renderer clip through window frames, taskbar buttons, title labels, and the Alt+Tab overlay.
- Added full-frame renderer-clip regression coverage and updated the architecture documentation.

## 2026-09: Preserve Drawing renderer boundaries

- Drawing now intersects its status prompt clip with the caller clip and restores that clip after rendering.
- Added renderer-clip regression coverage and updated Drawing and architecture documentation.

## 2026-09: Preserve Text Editor renderer boundaries

- Text Editor now intersects document and status clips with the caller clip and restores that clip after each region.
- Added renderer-clip regression coverage and updated Text Editor and architecture documentation.

## 2026-09: Preserve Terminal renderer boundaries

- Terminal now intersects its input and history clips with the caller clip and restores that clip after each region.
- Added renderer-clip regression coverage and updated Terminal and architecture documentation.

## 2026-09: Preserve Browser renderer boundaries

- Filesystem Browser now captures and restores the caller clip through path, list, and status text regions instead of resetting to an un-intersected content rectangle.
- Added renderer-clip regression coverage and updated Browser and architecture documentation.

## 2026-09: Preserve Settings renderer boundaries

- Settings now intersects its scroll clip with the caller clip and restores that clip after rendering, including through the wallpaper field's internal text clip.
- Added renderer-clip regression coverage and updated architecture and Settings documentation.

## 2026-09: Keep Settings wallpaper actions inside narrow clients

- The wallpaper path field now shrinks to preserve the Set and Clear controls inside narrow Settings windows.
- Added render-level geometry coverage for the responsive wallpaper row and updated the Settings guide.

## 2026-09: Keep zero-height clients valid on tiny desktops

- WindowManager now clamps rendered client height to zero when a desktop is shorter than a window title bar.
- Added shared shell coverage so apps never receive negative client geometry.

## 2026-09: Keep Minesweeper controls separated in narrow clients

- Minesweeper difficulty buttons now compress against the available HUD width so they cannot overlap the face/new-game button.
- Added focused coverage for normal narrow and tiny client widths and updated the game guide.

## 2026-09: Keep Browser filter controls inside narrow clients

- Filesystem Browser now derives a client-contained filter rectangle and reduces its width when the normal minimum cannot fit.
- Added focused narrow-width geometry coverage and updated the Browser guide.

## 2026-09: Keep Terminal history clips valid in narrow clients

- Centralized Terminal history viewport geometry and clamped both dimensions when a client is narrower or shorter than its padding and input strip.
- Added focused coverage for non-negative history clip bounds and documented the tiny-client behavior.

## 2026-09: Consolidate Drawing verification documentation

- Expanded the Drawing guide's contributor verification block with the current integration, raster, state, format, and optional smoke commands.
- Linked the user-facing guide to the maintenance expectations for Drawing prompt, file-format, and shell-routing changes.

## 2026-09: Align Browser rows with rendered hit areas

- Filesystem Browser now derives its list origin from the same row geometry used for drawing.
- Clicks in the list's top padding and inter-row gaps are ignored instead of selecting a neighboring entry.
- Added focused coverage for row-boundary and padding clicks and updated the Browser guide.

## 2026-09: Contain app rendering inside client areas

- WindowManager now clips every app render to its window client rectangle and restores the renderer clip afterward.
- Tiny or undersized existing app layouts can no longer paint over title bars or the taskbar.
- Added WindowManager renderer-clip coverage and documented the shared rendering boundary.

## 2026-09: Preserve client clipping through app helpers

- Terminal, Text Editor, Drawing, and Filesystem Browser now restore their client clip after drawing clipped text regions instead of disabling it mid-render.
- The shared WindowManager client-boundary guarantee now remains active through each app's internal text and status rendering passes.

## 2026-09: Keep edited Terminal history entries stable

- Terminal history navigation now exits when the recalled command is edited or changed by completion.
- Down no longer replaces an edited history entry with the buffer saved before navigation.
- Added focused coverage and documented the input-mode transition.

## 2026-09: Keep Settings controls readable after scaling

- Settings now derives section spacing, wallpaper fields, clock and scale controls, and footer geometry from the active interface font instead of fixed text bands.
- Updated the compressed Settings body sources and added 22pt layout coverage.
- The footer is clamped inside undersized Settings clients instead of extending above or below the content area.

## 2026-09: Keep Drawing input aligned after scaling

- Drawing now maps client pointer coordinates through the displayed canvas rectangle, so preserved raster data remains correctly addressed after interface text scaling changes the canvas height.

## 2026-09: Keep Terminal input inside tiny clients

- Terminal input-bar geometry is now clamped to the client rectangle, preventing the prompt strip from extending into window chrome when a client is shorter than the normal text layout.

## 2026-09: Keep Text Editor clicks inside rendered rows

- Text Editor mouse selection now ignores the unused gap below the last complete document row, keeping clicks aligned with rendered content above the status bar.

## 2026-09: Keep Browser hit testing inside rendered rows

- Filesystem Browser now counts only complete rows and rejects mouse clicks in clipped row fragments, keeping selection aligned with what is actually drawn in tiny clients.

## 2026-09: Keep Minesweeper controls aligned after resize and scaling

- Minesweeper now derives rendered and interactive face/difficulty controls from the same client-space rectangles, keeping clicks aligned after resize and interface scaling.

## 2026-09: Clamp Browser scroll after lifecycle changes

- Filesystem Browser now clamps listing scrollback after window resizes and interface text scaling, including when no row is selected.
- Added focused no-selection coverage and updated the Browser verification guide.

## 2026-09: Keep Text Editor scrollback inside resized clients

- Text Editor now clamps vertical scrollback when a window shrinks and derives visible rows from the actual space above the status bar.
- Tiny clients no longer claim document rows that cannot be rendered.
- Added focused resize coverage and updated the editor guide and verification notes.

## 2026-09: Keep Drawing chrome aligned after text scaling

- Drawing toolbar buttons and the status bar now grow from the active interface font, keeping labels and hit areas aligned at larger text sizes.
- Text scaling preserves the existing canvas, file state, and undo history.
- Added focused Drawing coverage and updated the canvas and verification documentation.

## 2026-09: Bound Terminal scrollback to its rendered history area

- Terminal scroll limits now use the rows that actually fit above the input strip instead of an overestimated height with extra scroll allowance.
- Resizing, text scaling, Page Up, Page Down, and mouse-wheel scrolling now clamp to the oldest fully visible output; tiny clients no longer claim or paint unavailable history rows.
- Added focused Terminal coverage and updated the app guide and verification notes.

## 2026-09: Keep tiny Browser clients inside their chrome

- Filesystem Browser now reports zero visible rows when its path, toolbar, and status bands consume the client area.
- Rendering no longer paints list rows through the status bar in undersized windows.

## 2026-09: Keep Filesystem Browser status clicks out of the list

- Browser row hit-testing now stops at the scaled status bar, so clicking status feedback cannot select or activate a file.
- Tiny client areas clamp the rendered list region to a non-negative height.

## 2026-09: Keep Filesystem Browser chrome aligned after text scaling

- The path bar, toolbar buttons, list origin, and status bar now share font-aware geometry with the existing row and context-menu metrics.
- Mouse row calculations and visible-row counts use the same dynamic bands, with focused scale coverage.

## 2026-09: Keep Text Editor prompts inside the scaled status bar

- Text Editor now derives its status-bar height from the active interface font, keeping prompt text and mouse hit testing aligned at larger scales.
- Updated the editor guide and verification notes.

## 2026-09: Keep taskbar controls readable after text scaling

- Start, taskbar window buttons, scroll arrows, and the clock tray now share a font-aware control height bounded by the taskbar band.
- Larger interface text no longer gets vertically clipped inside the shell controls.

## 2026-09: Keep Minesweeper bands readable after text scaling

- Minesweeper now derives its HUD, difficulty buttons, and footer from the active interface font instead of clipping scaled labels against fixed bands.
- Board layout and hit testing use the same dynamic HUD and footer heights, with focused coverage for scaled controls.

## 2026-09: Keep game HUDs aligned after text scaling

- Snake and Pong now derive their HUD height and text position from the active interface font instead of laying out the playfield below a fixed strip.
- Added scaled-font geometry coverage for Snake and updated the game guides.

## 2026-09: Document Drawing display scaling

- Documented how shared interface text scaling affects Drawing prompts and leaves canvas data, history, and file state unchanged.

## 2026-09: Isolate shell pointer releases

- Taskbar and Start-menu clicks now consume their matching mouse-up instead of forwarding an orphaned release to the focused app.
- Added Window Manager mouse-capture coverage for taskbar interactions.

## 2026-09: Keep session focus on visible windows

- Restoring a minimized final session entry now hands keyboard focus to the topmost visible window.
- Added focus lifecycle coverage for minimized restored geometry.

## 2026-09: Keep game overlays readable after text scaling

- Snake and Minesweeper now space overlay lines from the active font height instead of a fixed 18px step.
- End-state messages remain separated when the shared interface text scale is increased.

## 2026-09: Keep Filesystem filter prompts aligned after text scaling

- The active Browser filter now discards its cached pixel offset when the shared interface font changes.
- Added lifecycle coverage alongside the existing context-menu relayout check.

## 2026-09: Keep Minesweeper controls aligned after text scaling

- Difficulty button geometry now follows the shared interface font instead of keeping a fixed 18px height.
- Rendering and hit testing use the same scaled button bounds, with focused state coverage.

## 2026-09: Keep Drawing prompts aligned after text scaling

- Drawing now resets its cached path-prompt scroll when the shared interface font changes.
- Added state coverage for the prompt invalidation hook.

## 2026-09: Measure taskbar titles at their real width

- Taskbar buttons now size long UTF-8 titles from the active font metrics instead of a character-count estimate.
- Larger interface text no longer makes file-backed taskbar labels disproportionately cramped.
- Added scaled-render coverage for measured taskbar button growth.

## 2026-09: Keep Settings prompts aligned after text scaling

- Settings now resets its cached wallpaper-field scroll when the shared interface font changes.
- Added state coverage for the prompt invalidation hook.

## 2026-09: Keep app views aligned after text scaling

- UI scale changes now notify every open app, including minimized windows.
- Terminal and Text Editor rebuild their pixel-based scroll state against the new font metrics instead of retaining stale offsets.
- Added regression coverage for scale notifications reaching a minimized app.

## 2026-09: Align wallpaper roadmap documentation

- Updated the vision, architecture, and Terminal guides to describe shipped BMP/PNG/JPEG wallpaper support accurately.
- Reserved future wording for richer wallpaper controls instead of already-shipped image formats.

## 2026-09: Keep wallpaper dispatch warning-free

- Routed Window Manager wallpaper loading directly through `WallpaperImage` instead of a translation macro around `SDL_LoadBMP`.
- Removed the resulting SDL macro redefinition warning without changing BMP, PNG, or JPEG behavior.
- Updated WindowManager test commands for generated Settings bodies and the wallpaper loader source.

## 2026-09: Keep Filesystem menus aligned after text scaling

- Open Filesystem context menus now rebuild their cached size and hit targets when Settings changes the shared interface scale.
- Added an app scale-change hook and focused menu layout coverage.

## 2026-09: Show live desktop dimensions in Settings

- Settings now reads the current logical desktop size from the WindowManager instead of displaying a hardcoded value.
- Added focused state coverage for a resized logical desktop.

## 2026-09: Add Drawing operator reference

- Added a compact daily workflow card to the Drawing guide.
- Clarified when Save writes immediately and when it opens a path prompt.

## 2026-09: Keep game boards inside small windows

- Snake and Minesweeper now fit their complete boards inside narrow or short client areas instead of letting the board cover the HUD or footer.
- Tiny-cell rendering avoids drawing glyphs and shapes that cannot fit, with geometry coverage for small Snake and Expert Minesweeper layouts.

## 2026-09: Warn about external Drawing overwrites

- Drawing now reports external changes to a bound `.modr` file while preserving the canvas and undo history.
- Saving remains an explicit overwrite, with focused coverage for external and self-generated change notifications.

## 2026-09: Warn about external Text Editor overwrites

- Text Editor now reports when another app changes its bound file without replacing the in-memory buffer.
- Saving afterward remains an explicit overwrite, while Open can load the external version; added state coverage for both paths.

## 2026-09: Remove directories containing hidden symlinks

- Recursive filesystem removal now unlinks symlink entries that safe virtual listings hide because their targets resolve outside the host root.
- Added coverage proving the containing directory is removed while the outside target remains intact.

## 2026-09: Keep tiny desktop frames above the taskbar

- Window clamping now lets very small logical desktops override the normal minimum frame height, so visible frames do not extend below the taskbar.
- Added coordinate coverage for narrow and undersized desktop geometry.

## 2026-09: Refresh Browsers after nested creation

- Open Filesystem Browser windows now refresh ancestor listings when a file or folder creation also creates missing parent directories.
- Added a regression covering a nested file save that previously left its new parent folder invisible.

## 2026-09: Document Drawing file recovery

- Documented how Drawing handles an external overwrite of its bound `.modr` file without silently replacing the in-memory canvas.
- Added concrete accepted and rejected examples for the custom RGB prompt.

## 2026-09: Refresh state after file overwrites

- Added a virtual path change notification for successful overwrites, separate from creation events.
- Open Filesystem Browser listings now refresh after overwrites, and the active BMP wallpaper reloads when its file changes.
- Added focused Terminal, Browser, and WindowManager coverage for changed paths and wallpaper invalidation.

## 2026-09: Roll back failed recursive copies

- Recursive directory copies now remove a newly created destination when a child copy fails, instead of leaving a partial tree behind.
- Added coverage for a source tree containing a broken symlink child.

## 2026-09: Refresh browsers after file creation

- Added a centralized virtual path creation notification alongside move and removal notifications.
- Terminal `mkdir`, `touch`, and `cp`, Browser creation and copy, and new Text Editor or Drawing saves now refresh open Browser listings when appropriate.
- Added focused Browser, Terminal, and WindowManager coverage for the lifecycle event.

## 2026-09: Complete Drawing app guide

- Added a compact workflow reference for creating, editing, duplicating, renaming, and moving `.modr` drawings.
- Clarified the difference between saving a sketch and managing its file through the Filesystem Browser or Terminal.

## 2026-09: Preserve Filesystem Browser range anchors

- Refreshing a Filesystem Browser listing now restores the Shift-selection anchor by entry identity instead of dropping it.
- Removing the current anchor during Ctrl-selection promotes the surviving primary selection to keep range selection usable.
- Added focused state coverage for extending a range after a refresh.

## 2026-09: Keep Text Editor shortcuts discoverable

- Text Editor status messages no longer hide the footer keyboard hints after open, save, clipboard, or other feedback actions.
- The normal footer now continues to advertise `Ctrl+F` find, `Ctrl+H` replace, and the surrounding editing shortcuts.

## 2026-09: Clarify Drawing prompt completion

- Updated the Drawing guide to distinguish Save completion, which accepts existing entries, from Open completion, which filters files to `.modr`.
- Added a prompt-completion reference and expanded the guide map.

## 2026-09: Synchronize maximized window geometry

- Maximized windows now update their frame and app client size when the logical desktop changes.
- Restoring a maximized window clamps its saved geometry above the taskbar before notifying the app.
- Added coordinate coverage for maximize, desktop resize, and restore behavior.

## 2026-09: Preserve closed quoted Terminal paths

- Terminal Tab completion now leaves a command unchanged when the cursor is immediately after a closed quoted path, preserving its closing quote.
- Added lexer and filesystem-state regression coverage for this completion boundary.

## 2026-09: Publish Drawing app guide

- Added a navigable user guide for the Drawing app, covering canvas tools, keyboard workflows, colors, file operations, `.modr` storage, unsaved changes, and troubleshooting.

## 2026-09: Preserve prompt carets across path moves

- Text Editor, Drawing, and Settings now keep an active path prompt's caret at the same suffix position when a bound file or directory moves.
- Shared UTF-8-safe remapping prevents a path change from dropping the caret to the end or leaving it inside a multibyte character.

## 2026-09: Synchronize app sizes after interactive resize clamping

- Window Manager resize notifications now use the final frame after desktop bounds are applied, keeping app canvases and layouts aligned at the desktop edge.
- Added coordinate coverage for an oversized interactive resize.

## 2026-09: Refresh Browser listings after external child changes

- Filesystem Browser now refreshes the current directory when another app renames, moves, or deletes a direct child entry.
- Preserved the existing path-following behavior when the browser's current directory itself moves or is removed, with regression coverage for direct child changes.

## 2026-09: Clarify Drawing file workflows

- Added a decision guide explaining when to use Drawing Save, Filesystem Browser copy/rename/move, or Terminal file commands.
- Documented the difference between changing a `.modr` path and creating a valid MODR drawing payload.

## 2026-09: Finish quoted Terminal path completion

- Terminal Tab completion now closes an open double-quoted path after completing a single file.
- Quoted directory completion remains open with a trailing slash for continued navigation, with regression coverage for both workflows.

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
