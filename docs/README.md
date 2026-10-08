# MONOLITH documentation

Start with the [root README](../README.md) for the overview, build steps and first run.

## Project

| Doc | What it covers |
|-----|----------------|
| [Vision](vision.md) | What MONOLITH is for and the principles behind it |
| [Roadmap](ROADMAP.md) | What 1.0 shipped, what is out of scope, what comes after |
| [1.0 checklist](1.0-CHECKLIST.md) | Tick-box status of the 1.0 milestones |
| [Architecture](architecture.md) | How the window manager, apps, rendering and input fit together |
| [Filesystem](filesystem.md) | Virtual paths, host storage under `~/.monolith/`, the `Filesystem` API |
| [Changelog](../CHANGELOG.md) | Full change history, newest first |

## Apps

| Doc | App |
|-----|-----|
| [terminal.md](apps/terminal.md) | Terminal: commands, quoting, history, completion |
| [text-editor.md](apps/text-editor.md) | Text Editor: editing, Find and Replace, saving |
| [filesystem-browser.md](apps/filesystem-browser.md) | Filesystem: browsing, file operations, filter |
| [drawing.md](apps/drawing.md) | Drawing: tools, colors, `.modr` files |
| [settings.md](apps/settings.md) | Settings: desktop color, wallpaper, clock, text size |
| [snake.md](apps/snake.md) | Snake |
| [minesweeper.md](apps/minesweeper.md) | Minesweeper |
| [pong.md](apps/pong.md) | Pong |
| [breakout.md](apps/breakout.md) | Breakout |

## Internals

Reference notes for the low-level details. The pages above link here when it matters.

| Doc | What it covers |
|-----|----------------|
| [window-manager.md](internals/window-manager.md) | Focus rules, taskbar, instance titles, callback safety, close and quit, session restore |
| [desktop-shell.md](internals/desktop-shell.md) | App registry, Start menu and type-ahead, desktop icons |
| [rendering.md](internals/rendering.md) | Clip rules, text texture caches, per-app render work |
| [atomic-writes.md](internals/atomic-writes.md) | How every file write is staged and published, workspace cleanup, the host-writer race |
| [wallpaper.md](internals/wallpaper.md) | Image decoding, fit modes, reload rules |
| [limits.md](internals/limits.md) | Every size and count limit in one table |

## Development

| Doc | What it covers |
|-----|----------------|
| [building.md](development/building.md) | Dependencies, CMake build, generated sources, running |
| [testing.md](development/testing.md) | Headless suite, sanitizer build, every test program |
| [ci.md](development/ci.md) | GitHub Actions workflow |
| [repo-layout.md](development/repo-layout.md) | What lives where in the source tree |
| [contributing.md](development/contributing.md) | Branches, commits, adding an app, doc rules |

## Files MONOLITH keeps on the host

| Path | Contents |
|------|----------|
| `~/.monolith/fs/` | The internal filesystem |
| `~/.monolith/session.txt` | Open windows, restored on next launch |
| `~/.monolith/desktop_settings.txt` | Settings app preferences |
| `~/.monolith/snake_highscore.txt` | Snake high score |
| `~/.monolith/minesweeper_best.txt` | Minesweeper best times |
