# Session log

Historical session rows were trimmed during the 7.1 MCP ship to keep AGENTS.md small.
Current chunk pointer: [`CURRENT_CHUNK`](CURRENT_CHUNK).

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
