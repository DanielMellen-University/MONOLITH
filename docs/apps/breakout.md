# Breakout

Bounce the ball off your paddle and break every brick. You have three lives.

## Using it

Open **Breakout** from the Start menu under **Games**. Each window is a separate game.

- The wall has 10 columns and 5 rows of bricks.
- Breaking a brick scores more the higher its row: 10 points for the bottom row up to 50 for the top row.
- Missing the ball costs a life. Losing all 3 ends the game. Clearing every brick wins.
- The top bar shows score, lives and bricks left.
- The game pauses when another window gets focus or the Start menu opens.

## Keyboard shortcuts

| Input | Action |
|-------|--------|
| A, Left arrow | Move left |
| D, Right arrow | Move right |
| Space, P, mouse click | Pause or resume |
| Enter | Resume, or start again after the game ends |
| R | Restart |

## Limits

- No sound, power-ups or extra levels.
- The playfield scales with the window.

## Developer notes

- `src/app/BreakoutLogic.{hpp,cpp}`: rules, with no SDL dependency.
- `src/app/BreakoutApp.{hpp,cpp}`: window, drawing and input.
- `WindowManager::launchBreakout()` in `src/window/detail/wm_body_08a.inc`; Start menu action `8`.
- Frame time comes from `detail::tickDeltaSeconds()`, as in Pong.

Tests:

```bash
g++ -std=c++23 scripts/test_breakout_state.cpp src/app/BreakoutLogic.cpp -o build/test_breakout_state && ./build/test_breakout_state
```

`test_breakout_render` (in the full suite) checks drawing.
