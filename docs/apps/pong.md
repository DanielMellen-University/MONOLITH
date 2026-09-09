# Pong

Classic Pong as a native Monolith app. Steer the left paddle, bounce the ball past the AI, first to 5 wins.

## Launching

Open **Pong** from the Start menu under **Games**. Multiple independent games are supported:

- `Pong`
- `Pong 2`
- `Pong 3`

## Controls

| Input | Action |
|-------|--------|
| Arrow Up / W | Move paddle up |
| Arrow Down / S | Move paddle down |
| Space / P / click | Pause or resume |
| R | Restart |
| Enter | Resume if paused; restart after game over |

Focusing another window auto-pauses. Resume with Space, P, Enter, or click when you return.

## Rules

- Player paddle on the left, simple AI on the right
- Ball bounces off top/bottom walls and paddles
- Point if the ball leaves the opponent's side
- First to 5 ends the match (You win / AI wins)

## Presentation

The playfield scales to the window. Score is shown in the HUD and clips at the client boundary when the window is narrow. Session restore reopens Pong windows.

## Developer Notes

Main implementation files:

- `src/app/PongLogic.hpp` / `PongLogic.cpp` — rules (input, tick, Playing vs GameOver); no SDL
- `src/app/PongApp.hpp` / `PongApp.cpp` — window, render, keyboard
- `src/window/WindowManager.cpp` — `launchPong()`, Start → Games

Headless state test:

```bash
g++ -std=c++23 scripts/test_pong_state.cpp src/app/PongLogic.cpp -o build/test_pong_state && ./build/test_pong_state
```
