# Pong

Move the left paddle and get the ball past the computer. First to 5 points wins.

## Using it

Open **Pong** from the Start menu under **Games**. Each window is a separate game.

- You control the left paddle; the computer plays the right one.
- The ball bounces off the top and bottom walls and the paddles. Where it hits the paddle changes its angle, with a cap on vertical speed so it stays catchable.
- A point is scored when the ball passes a paddle. The first side to 5 wins.
- The game pauses when another window gets focus or the Start menu opens.

## Keyboard shortcuts

| Input | Action |
|-------|--------|
| W, Up arrow | Move up |
| S, Down arrow | Move down |
| Space, P, mouse click | Pause or resume |
| Enter | Resume, or start again after the match |
| R | Restart |

## Limits

- No sound and no two-player mode.
- The playfield scales with the window.

## Developer notes

- `src/app/PongLogic.{hpp,cpp}`: rules, with no SDL dependency.
- `src/app/PongApp.{hpp,cpp}`: window, drawing and input.
- `WindowManager::launchPong()` in `src/window/detail/wm_body_08a.inc`; Start menu action `7`.
- Frame time comes from `detail::tickDeltaSeconds()`, which handles SDL tick wraparound and caps a stalled frame at 50 ms.

Test:

```bash
g++ -std=c++23 scripts/test_pong_state.cpp src/app/PongLogic.cpp -o build/test_pong_state && ./build/test_pong_state
```
