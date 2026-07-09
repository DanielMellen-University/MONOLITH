# Snake

Classic Snake as a native Monolith app. Steer the snake, eat food, avoid walls and yourself.

## Launching

Open **Snake** from the Start menu. Multiple independent games are supported:

- `Snake`
- `Snake 2`
- `Snake 3`

## Controls

| Input | Action |
|-------|--------|
| Arrow keys / WASD | Change direction |
| Space / P | Pause or resume |
| R / Enter | Restart |

Direction cannot reverse 180° in one step (no instant suicide).

Focusing another window auto-pauses the game. Resume with Space or P when you return.

## Rules

- 20×20 grid; snake starts length 3, moving right
- Eat food to grow and score +1
- Mild speed-up every 5 points (floor at a still-playable rate)
- Hit a wall or your body → game over
- Filling the entire board (rare) → win

The board letterboxes inside the window when resized or maximized.

## Current Limitations

- No high-score persistence
- No sound
- No configurable grid size or starting speed

## Developer Notes

- `src/app/SnakeApp.{hpp,cpp}`
- `WindowManager::launchSnake()` and Start menu action `5`
- Game steps run from `App::update()`, dispatched by `WindowManager::update()` for non-minimized windows
