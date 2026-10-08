# Limits

Every fixed size or count limit in MONOLITH, in one place. Limits exist so that a huge or malformed file cannot make an app allocate without bound.

## Files and documents

| What | Limit | Where |
|------|-------|-------|
| Text Editor document | 16 MiB and 65,536 lines, on open, save and every edit | `TextEditorApp` |
| Text Editor Find and Replace fields | 16 MiB and 65,536 lines each | `TextEditorApp` |
| Text Editor undo and redo | 50 states and about 64 MiB combined | `TextEditorApp` |
| Drawing canvas in a `.modr` file | 1 to 4096 pixels per side | `DrawingRaster` |
| Drawing undo and redo | 32 states and 64 MiB of saved tiles combined | `DrawingApp` |
| Wallpaper image | 16,777,216 pixels | `WallpaperImage` |
| File reads and writes in apps | Streamed in 16 KiB chunks | `Filesystem`, apps |
| Filesystem filter text | 255 bytes of UTF-8 (the Linux file name limit) | `FilesystemApp` |

## Terminal

| What | Limit |
|------|-------|
| Scrollback | 2,000 rows and 8 MiB |
| One scrollback row | 64 KiB, then marked `[truncated]` |
| Command history | 500 entries and 2 MiB |
| One history entry | 64 KiB (longer commands still run but are not saved) |
| History file recovery read | 16 MiB |
| `cat` output | 5,000 lines, 64 KiB per line in progress |

## Shell

| What | Limit |
|------|-------|
| Saved session | 128 windows (the topmost 128) |
| Session file read | 1,024 records, 16 KiB per line, stops at 128 open windows |
| Settings file read | 64 records, 16 KiB per line |
| Start menu filter | 64 bytes of UTF-8 |
| Desktop icon double-click | 450 ms |
| Startup cleanup walk | 32 host entries per frame |
| Per-directory cleanup sweep | 32 entries per save or listing, started every 32 operations, 16 directories tracked |

## Text caches

| What | Limit |
|------|-------|
| Text texture cache (shell and each app) | 256 entries and an estimated 16 MiB |
| Text Editor retained prefix buffer | 4 KiB (longer prefixes use temporary storage) |

## Game records

| File | Accepted content |
|------|------------------|
| `~/.monolith/snake_highscore.txt` | Reads at most 16 rows of 32 bytes; a complete score from 0 to 397 |
| `~/.monolith/minesweeper_best.txt` | Reads at most 16 rows of 64 bytes; complete times from 1 to 999 seconds. An oversized row stops loading but keeps times already read. |

## Games

| What | Value |
|------|-------|
| Snake board | 20x20, step every 120 ms, 6 ms faster every 4 points, never below 55 ms |
| Snake turn queue | 2 turns |
| Pong match | First to 5 |
| Breakout | 10 by 5 bricks, 3 lives |
| Minesweeper timer display | Up to 999 seconds |
| Stalled frame cap (Pong, Breakout) | 50 ms |
