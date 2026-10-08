# Snake

Steer the snake, eat food to grow, and avoid the walls and your own body.

## Using it

Open **Snake** from the Start menu under **Games**. Each window is a separate game (`Snake`, `Snake 2`, ...).

- The board is 20x20. The snake starts 3 long, moving right.
- Each food is worth 1 point and adds one segment.
- The game starts at one step every 120 ms and gets 6 ms faster every 4 points, never faster than 55 ms.
- Hitting a wall or your body ends the game. Moving into the square your tail is leaving is allowed, unless you are eating food on that move.
- Filling the whole board wins.
- The game pauses when another window gets focus or the Start menu opens.

## Keyboard shortcuts

| Input | Action |
|-------|--------|
| Arrow keys, WASD | Turn (up to 2 turns are queued between steps) |
| Space, P | Pause or resume |
| Enter | Resume, or start again after the game ends |
| R | Restart |
| Mouse click | Pause or resume, or start again after the game ends |

You cannot reverse straight into yourself in one step.

## High score

The best score is shown in the top bar and saved to `~/.monolith/snake_highscore.txt`. Beating it shows **NEW BEST!** If the save fails, the game shows **BEST NOT SAVED** and tries again when the window regains focus or the game restarts. Loading accepts only a whole score from 0 to 397 (the most a 20x20 board allows).

## Limits

- No sound.
- The board size is fixed.
- The board is letterboxed in the window. In a very small window the cells shrink so the whole board stays visible.

## Developer notes

- `src/app/SnakeApp.{hpp,cpp}`.
- `src/detail/Random.hpp`: each game has its own random stream, so Snake and Minesweeper do not affect each other's sequences.
- `WindowManager::launchSnake()` in `src/window/detail/wm_body_08a.inc`; Start menu action `5`.
- Steps run in `App::update()`, which the window manager calls for windows that are not minimized.
- Test: `test_snake_state` (tail movement, growth collisions, text texture reuse).
