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
- Timer **pauses while the Minesweeper window is unfocused** (HUD shows **PAUSED**) and resumes from the exact elapsed time when you focus it again — same idea as Snake.
- HUD shows remaining mines (total − flags), timer, best time for the difficulty, and difficulty name.

## Best times

Per-difficulty best times are stored on the host at `~/.monolith/minesweeper_best.txt` and shown in the HUD. A new record shows **NEW BEST!** on the win overlay.

## Presentation

- Face button reflects play / press / win / lose
- Face and difficulty button hitboxes share the same client-space geometry as their drawn controls, including after resize and interface-scale changes
- Pressed unopened cell darkens briefly
- The HUD status line clips before the face button, and the footer clips at the window edge, so narrow windows do not cover controls with text
- HUD, footer, and difficulty button geometry grow with the shared interface font, keeping the 90%, 100%, and 115% scale choices aligned without clipping labels
- Win / lose messages are a **centered vertical stack** on the board (title, time/best or restart hint, optional **NEW BEST!**)
- Overlay line spacing follows the active interface font so scaled text does not overlap on end-state screens

The board letterboxes inside the window. Expert on a small window uses compressed cells so the full board remains inside the client area; maximize for comfort. Tiny cells prioritize keeping the board and footer contained over drawing number glyphs that would not fit.

## Current Limitations

- No sound
- Window size does not auto-change with difficulty

## Developer Notes

- `src/app/MinesweeperApp.{hpp,cpp}`
- `scripts/test_minesweeper_state.cpp` covers focus pause/resume without rendering
- `WindowManager::launchMinesweeper()` and Start menu action `6` (listed under the **Games** category)
- Timer advances in `App::update()` while playing and focused; `onFocusLost` / `onFocusGained` freeze and resume
