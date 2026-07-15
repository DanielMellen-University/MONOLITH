# Snake

Classic Snake as a native Monolith app. Steer the snake, eat food, avoid walls and yourself.

## Launching

Open **Snake** from the Start menu under **Games**. Multiple independent games are supported:

- `Snake`
- `Snake 2`
- `Snake 3`

## Controls

| Input | Action |
|-------|--------|
| Arrow keys / WASD | Change direction (queued turns) |
| Space / P | Pause or resume |
| Click | Pause or resume (or restart after game over) |
| R | Restart |
| Enter | Resume if paused; restart after game over / win |

Direction cannot reverse 180° in one step (no instant suicide). Up to two rapid turns can be queued so quick corners are not lost between steps.

Focusing another window auto-pauses the game. Resume with Space, P, Enter, or click when you return.

## Rules

- 20×20 grid; snake starts length 3, moving right
- Eat food to grow and score +1
- Mild speed-up as score rises (floor at a still-playable rate)
- Hit a wall or your body → game over
- Filling the entire board (rare) → win

## High score

The best score is saved on the host at `~/.monolith/snake_highscore.txt` and shown in the HUD. Beating it shows **NEW BEST!** on the end screen.

## Presentation

- Checkerboard board, food highlight, body gradient, and directional head eyes
- HUD shows score, best, and length
- Brief flash when food is eaten

The board letterboxes inside the window when resized or maximized.

## Current Limitations

- No sound
- Fixed 20×20 grid (not configurable)

## Developer Notes

- `src/app/SnakeApp.{hpp,cpp}`
- `WindowManager::launchSnake()` and Start menu action `5` (listed under the **Games** category)
- Game steps run from `App::update()`, dispatched by `WindowManager::update()` for non-minimized windows
