# Session log

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
