# Session log

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
