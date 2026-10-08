# Minesweeper

Open every cell that does not hide a mine.

## Using it

Open **Minesweeper** from the Start menu under **Games**. Each window is a separate game.

| Level | Board | Mines |
|-------|-------|-------|
| Beginner (default) | 9x9 | 10 |
| Intermediate | 16x16 | 40 |
| Expert | 30x16 | 99 |

- The first cell you open is never a mine. On Beginner and Intermediate its 8 neighbors are also safe.
- A cell with no neighboring mines opens its neighbors automatically.
- **Chord:** clicking an open number whose neighboring flag count matches it opens all unflagged neighbors. A wrong flag can still set off a mine.
- You win when every safe cell is open; the remaining mines are flagged for you. If you open a mine, all mines are shown, the one you hit is highlighted and wrong flags are marked **X**.
- The timer starts on the first click and stops at the end of the game. It pauses (the top bar shows **PAUSED**) while another window has focus or the Start menu is open.
- The top bar shows mines left (mines minus flags), the timer, the best time for this level and the level name.
- Changing level starts a new game and resizes the window to fit, if the desktop has room. A maximized window stays maximized.

## Keyboard and mouse

| Input | Action |
|-------|--------|
| Left click | Open a cell, or chord on an open number |
| Right click | Cycle the mark: flag, question mark, none |
| Middle click | Chord on an open number |
| Face button | New game at the same level |
| R, Enter | New game at the same level |
| 1 / 2 / 3, or the level buttons | Beginner / Intermediate / Expert (starts a new game) |
| Click after the game ends | New game |

## Best times

Best times per level are shown in the top bar and saved to `~/.monolith/minesweeper_best.txt`. A new record shows **NEW BEST!** If saving fails, **BEST TIME NOT SAVED** is shown and the save is retried on focus or at the next game. Times from 1 to 999 seconds are accepted when loading.

## Limits

- No sound.
- The timer display stops at 999 seconds.
- Expert in a small window uses small cells; numbers are hidden when they would not fit. Maximize the window for comfort.
- In a very narrow window the level buttons are hidden; 1, 2 and 3 still work.

## Developer notes

- `src/app/MinesweeperApp.{hpp,cpp}`.
- `src/detail/Random.hpp`: mines are chosen uniformly from the cells allowed by first-click safety, using the game's own random stream.
- `WindowManager::launchMinesweeper()` in `src/window/detail/wm_body_08a.inc`; Start menu action `6`.
- The timer advances in `App::update()`; `onFocusLost` and `onFocusGained` pause and resume it.
- Tests: `test_minesweeper_state` (focus pause, rendering, text reuse) and `test_minesweeper_window_size` (level-driven window resizing).
