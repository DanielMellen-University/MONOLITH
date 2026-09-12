# Session log

Historical session rows were trimmed during the 7.1 MCP ship to keep AGENTS.md small.
Current chunk pointer: [`CURRENT_CHUNK`](CURRENT_CHUNK).

| 2026-09-12 | cleanup | Rebuilt open Filesystem context-menu geometry after live UI scale changes; added app lifecycle coverage and updated shell/browser docs. |
| 2026-09-12 | cleanup | Routed Window Manager wallpaper loading directly through WallpaperImage to remove the SDL_LoadBMP macro warning; behavior is unchanged. |
| 2026-09-12 | docs | Updated WindowManager verification commands for generated Settings bodies and WallpaperImage linkage. |
| 2026-09-12 | docs | Aligned vision, architecture, and Terminal documentation with shipped BMP/PNG/JPEG wallpaper support. |
| 2026-09-12 | cleanup | Rebuilt Terminal and Text Editor view offsets after shared text scaling; scale notifications now include minimized apps. |
| 2026-09-12 | cleanup | Sized taskbar buttons from measured UTF-8 title widths and added scaled-render coverage for long labels. |
| 2026-09-12 | cleanup | Added Settings prompt invalidation for shared text scaling and covered its cached horizontal offset. |
| 2026-09-12 | cleanup | Added Drawing prompt invalidation for shared text scaling and covered its cached horizontal offset. |
