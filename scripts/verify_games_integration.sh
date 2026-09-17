#!/usr/bin/env bash
# Static integration checks for Snake, Minesweeper, Pong, and Breakout (no SDL/display required).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

fail() { echo "FAIL: $1" >&2; exit 1; }
ok()   { echo "ok: $1"; }

[[ -f src/app/SnakeApp.hpp ]] || fail "SnakeApp.hpp missing"
[[ -f src/app/SnakeApp.cpp ]] || fail "SnakeApp.cpp missing"
[[ -f src/app/MinesweeperApp.hpp ]] || fail "MinesweeperApp.hpp missing"
[[ -f src/app/MinesweeperApp.cpp ]] || fail "MinesweeperApp.cpp missing"
[[ -f src/app/PongApp.hpp ]] || fail "PongApp.hpp missing"
[[ -f src/app/PongApp.cpp ]] || fail "PongApp.cpp missing"
[[ -f src/app/PongLogic.hpp ]] || fail "PongLogic.hpp missing"
[[ -f src/app/PongLogic.cpp ]] || fail "PongLogic.cpp missing"
[[ -f src/app/BreakoutApp.hpp ]] || fail "BreakoutApp.hpp missing"
[[ -f src/app/BreakoutApp.cpp ]] || fail "BreakoutApp.cpp missing"
[[ -f src/app/BreakoutLogic.hpp ]] || fail "BreakoutLogic.hpp missing"
[[ -f src/app/BreakoutLogic.cpp ]] || fail "BreakoutLogic.cpp missing"

grep -q 'launchSnake' src/window/WindowManager.hpp || fail "launchSnake not declared"
grep -q 'launchMinesweeper' src/window/WindowManager.hpp || fail "launchMinesweeper not declared"
grep -q 'launchPong' src/window/WindowManager.hpp || fail "launchPong not declared"
grep -q 'launchBreakout' src/window/WindowManager.hpp || fail "launchBreakout not declared"
grep -Rq 'launchSnake' src/window || fail "launchSnake not implemented"
grep -Rq 'launchMinesweeper' src/window || fail "launchMinesweeper not implemented"
grep -Rq 'launchPong' src/window || fail "launchPong not implemented"
grep -Rq 'launchBreakout' src/window || fail "launchBreakout not implemented"
grep -q 'SnakeApp' src/window/WindowManager.cpp || fail "SnakeApp not included in WM"
grep -q 'MinesweeperApp' src/window/WindowManager.cpp || fail "MinesweeperApp not included in WM"
grep -q 'PongApp' src/window/WindowManager.cpp || fail "PongApp not included in WM"
grep -q 'BreakoutApp' src/window/WindowManager.cpp || fail "BreakoutApp not included in WM"
grep -q 'SnakeApp.cpp' CMakeLists.txt || fail "SnakeApp.cpp not in CMakeLists"
grep -q 'MinesweeperApp.cpp' CMakeLists.txt || fail "MinesweeperApp.cpp not in CMakeLists"
grep -q 'PongApp.cpp' CMakeLists.txt || fail "PongApp.cpp not in CMakeLists"
grep -q 'PongLogic.cpp' CMakeLists.txt || fail "PongLogic.cpp not in CMakeLists"
grep -q 'BreakoutApp.cpp' CMakeLists.txt || fail "BreakoutApp.cpp not in CMakeLists"
grep -q 'BreakoutLogic.cpp' CMakeLists.txt || fail "BreakoutLogic.cpp not in CMakeLists"

grep -q 'AppAction::Snake' src/window/AppRegistry.hpp || fail "Snake registry entry missing"
grep -q 'AppAction::Minesweeper' src/window/AppRegistry.hpp || fail "Minesweeper registry entry missing"
grep -q 'AppAction::Pong' src/window/AppRegistry.hpp || fail "Pong registry entry missing"
grep -q 'AppAction::Breakout' src/window/AppRegistry.hpp || fail "Breakout registry entry missing"
grep -q '"Games"' src/window/AppRegistry.hpp || fail "Start menu Games category missing in registry"
grep -Rq 'case AppAction::Snake: launchSnake' src/window || fail "Snake launchByAction missing"
grep -Rq 'case AppAction::Minesweeper: launchMinesweeper' src/window || fail "Minesweeper launchByAction missing"
grep -Rq 'case AppAction::Pong: launchPong' src/window || fail "Pong launchByAction missing"
grep -Rq 'case AppAction::Breakout: launchBreakout' src/window || fail "Breakout launchByAction missing"
grep -Rq 'case AppAction::ShutDown: requestQuit' src/window || fail "Shut Down launchByAction missing"
grep -Rq 'buildStartMenuRows' src/window || fail "Start menu rows not built from registry"

grep -Rq 'claimNextAppInstanceTitle("Snake")' src/window \
  || fail "Snake instance titling missing"
grep -Rq 'claimNextAppInstanceTitle("Minesweeper")' src/window \
  || fail "Minesweeper instance titling missing"
grep -Rq 'claimNextAppInstanceTitle("Pong")' src/window \
  || fail "Pong instance titling missing"
grep -Rq 'claimNextAppInstanceTitle("Breakout")' src/window \
  || fail "Breakout instance titling missing"

grep -Rq 'w->app->update()' src/window \
  || fail "App::update dispatch missing in WindowManager::update"

grep -q 'apps/snake.md' docs/README.md || fail "snake.md not linked from docs hub"
grep -q 'apps/minesweeper.md' docs/README.md || fail "minesweeper.md not linked from docs hub"
grep -q 'apps/pong.md' docs/README.md || fail "pong.md not linked from docs hub"
grep -q 'apps/breakout.md' docs/README.md || fail "breakout.md not linked from docs hub"

grep -q 'snake_highscore' src/app/SnakeApp.cpp || fail "Snake high score path missing"
grep -q 'm_dirQueue' src/app/SnakeApp.cpp || fail "Snake direction queue missing"
grep -q 'chord' src/app/MinesweeperApp.cpp || fail "Minesweeper chord missing"
grep -q 'minesweeper_best' src/app/MinesweeperApp.cpp || fail "Minesweeper best times path missing"
grep -q 'Mark::Question' src/app/MinesweeperApp.cpp || fail "Minesweeper question marks missing"
grep -q 'State::GameOver' src/app/PongLogic.cpp || fail "Pong GameOver state missing"
grep -q 'kWinScore' src/app/PongLogic.hpp || fail "Pong win score missing"
grep -q 'State::Won' src/app/BreakoutLogic.cpp || fail "Breakout Won state missing"
grep -q 'kStartLives' src/app/BreakoutLogic.hpp || fail "Breakout start lives missing"

ok "all games static integration checks passed"
