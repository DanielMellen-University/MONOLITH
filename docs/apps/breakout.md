# Breakout

Classic Breakout as a native Monolith app. Steer the paddle, bounce the ball through the brick wall, clear the board with three lives.

## Launching

Open **Breakout** from the Start menu under **Games**. Multiple independent games are supported:

- `Breakout`
- `Breakout 2`
- `Breakout 3`

## Controls

| Input | Action |
|-------|--------|
| Arrow Left / A | Move paddle left |
| Arrow Right / D | Move paddle right |
| Space / P / click | Pause or resume |
| R | Restart |
| Enter | Resume if paused; restart after win or game over |

Focusing another window or opening the Start menu auto-pauses. Resume with Space, P, Enter, or click when you return.

## Rules

- Paddle along the bottom, bricks in five colored rows above
- Ball bounces off walls, paddle, and bricks
- Hitting a brick removes it and adds score (higher rows worth more)
- Losing the ball below the paddle costs a life; three lives
- Clearing every brick wins; zero lives is game over

## Presentation

The playfield scales to the window. Score, lives, and remaining bricks appear in a font-aware HUD whose height follows the shared interface text scale. The HUD clips at the client boundary when the window is narrow. Frame timing uses the shared wrap-safe SDL tick helper and caps a stalled update at 50 ms. Session restore reopens Breakout windows.
Direct render-size changes refresh the cached client geometry before drawing, keeping the app lifecycle consistent with normal window resizes.

## Developer Notes

Main implementation files:

- `src/app/BreakoutLogic.hpp` / `BreakoutLogic.cpp` - rules (input, tick, Playing / Won / GameOver); no SDL
- `src/app/BreakoutApp.hpp` / `BreakoutApp.cpp` - window, render, keyboard
- `src/window/WindowManager.cpp` - `launchBreakout()`, Start -> Games

Headless state test:

```bash
g++ -std=c++23 scripts/test_breakout_state.cpp src/app/BreakoutLogic.cpp -o build/test_breakout_state && ./build/test_breakout_state
```
