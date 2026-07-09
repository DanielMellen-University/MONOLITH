# Minesweeper

Classic Minesweeper as a native Monolith app. Clear the board without detonating mines.

## Launching

Open **Minesweeper** from the Start menu. Multiple independent games are supported:

- `Minesweeper`
- `Minesweeper 2`
- `Minesweeper 3`

## Controls

| Input | Action |
|-------|--------|
| Left-click | Reveal cell |
| Right-click | Toggle flag |
| R / Enter | New game (same difficulty) |
| 1 / 2 / 3 | Beginner / Intermediate / Expert (restarts) |
| HUD buttons | Same as 1 / 2 / 3 |

## Difficulties

| Level | Size | Mines |
|-------|------|-------|
| Beginner (default) | 9×9 | 10 |
| Intermediate | 16×16 | 40 |
| Expert | 30×16 | 99 |

Changing difficulty immediately starts a new game.

## Rules

- **First-click safety**: the first reveal never hits a mine. On Beginner, the 3×3 neighborhood around the first click is also kept clear.
- Empty cells (zero adjacent mines) flood-reveal neighbors.
- Win when all non-mine cells are revealed (remaining mines are auto-flagged).
- Lose when a mine is revealed; all mines are shown.
- Timer starts on the first reveal and freezes on win/lose (displayed up to 999s).
- HUD shows remaining mines (total − flags), timer, and difficulty.

The board letterboxes inside the window. Expert on a small window uses small cells; maximize for comfort.

## Current Limitations

- No chord / middle-click
- No high-score persistence
- No sound
- Window size does not auto-change with difficulty

## Developer Notes

- `src/app/MinesweeperApp.{hpp,cpp}`
- `WindowManager::launchMinesweeper()` and Start menu action `6`
- Timer advances in `App::update()` while a game is in progress
