# Minesweeper

Classic Minesweeper as a native Monolith app. Clear the board without detonating mines.

## Launching

Open **Minesweeper** from the Start menu under **Games**. Multiple independent games are supported:

- `Minesweeper`
- `Minesweeper 2`
- `Minesweeper 3`

## Controls

| Input | Action |
|-------|--------|
| Left-click | Reveal cell; on an already-open number with matching flags, **chord** (open neighbors) |
| Right-click | Cycle mark: empty → flag → question → empty |
| Middle-click | Chord on a revealed number (same as left-click chord) |
| Face button (`:)` / `B)` / `X(`) | New game at current difficulty |
| R / Enter | New game (same difficulty) |
| 1 / 2 / 3 | Beginner / Intermediate / Expert (restarts) |
| HUD difficulty buttons | Same as 1 / 2 / 3 |
| Click after win/lose | New game |

## Difficulties

| Level | Size | Mines |
|-------|------|-------|
| Beginner (default) | 9×9 | 10 |
| Intermediate | 16×16 | 40 |
| Expert | 30×16 | 99 |

Changing difficulty immediately starts a new game.

## Rules

- **First-click safety**: the first reveal never hits a mine. On Beginner and Intermediate, the 3×3 neighborhood around the first click is also kept clear.
- Empty cells (zero adjacent mines) flood-reveal neighbors.
- **Chord**: when a revealed number’s adjacent flag count equals that number, opening (left or middle) reveals all unmarked neighbors. Wrong flags can still explode.
- Win when all non-mine cells are revealed (remaining mines are auto-flagged).
- Lose when a mine is revealed; all mines are shown, the hit mine is highlighted, and incorrect flags are marked with **X**.
- Timer starts on the first reveal and freezes on win/lose (displayed up to 999s).
- HUD shows remaining mines (total − flags), timer, best time for the difficulty, and difficulty name.

## Best times

Per-difficulty best times are stored on the host at `~/.monolith/minesweeper_best.txt` and shown in the HUD. A new record shows **NEW BEST!** on the win overlay.

## Presentation

- Face button reflects play / press / win / lose
- Pressed unopened cell darkens briefly
- Win overlay shows time and best; lose overlay invites a restart

The board letterboxes inside the window. Expert on a small window uses small cells; maximize for comfort.

## Current Limitations

- No sound
- Window size does not auto-change with difficulty

## Developer Notes

- `src/app/MinesweeperApp.{hpp,cpp}`
- `WindowManager::launchMinesweeper()` and Start menu action `6` (listed under the **Games** category)
- Timer advances in `App::update()` while a game is in progress
