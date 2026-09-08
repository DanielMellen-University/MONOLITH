#!/usr/bin/env bash
# Static integration checks for Snake, Minesweeper, and Pong (no SDL/display required).
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

grep -q 'launchSnake' src/window/WindowManager.hpp || fail "launchSnake not declared"
grep -q 'launchMinesweeper' src/window/WindowManager.hpp || fail "launchMinesweeper not declared"
grep -q 'launchPong' src/window/WindowManager.hpp || fail "launchPong not declared"
grep -Rq 'launchSnake' src/window || fail "launchSnake not implemented"
grep -Rq 'launchMinesweeper' src/window || fail "launchMinesweeper not implemented"
grep -Rq 'launchPong' src/window || fail "launchPong not implemented"
grep -q 'SnakeApp' src/window/WindowManager.cpp || fail "SnakeApp not included in WM"
grep -q 'MinesweeperApp' src/window/WindowManager.cpp || fail "MinesweeperApp not included in WM"
grep -q 'PongApp' src/window/WindowManager.cpp || fail "PongApp not included in WM"
grep -q 'SnakeApp.cpp' CMakeLists.txt || fail "SnakeApp.cpp not in CMakeLists"
grep -q 'MinesweeperApp.cpp' CMakeLists.txt || fail "MinesweeperApp.cpp not in CMakeLists"
grep -q 'PongApp.cpp' CMakeLists.txt || fail "PongApp.cpp not in CMakeLists"
grep -q 'PongLogic.cpp' CMakeLists.txt || fail "PongLogic.cpp not in CMakeLists"

grep -Rq '{"Snake", 5, 1}' src/window || fail "Start menu Snake entry missing (under Games)"
grep -Rq '{"Minesweeper", 6, 1}' src/window || fail "Start menu Minesweeper entry missing (under Games)"
grep -Rq '{"Pong", 7, 1}' src/window || fail "Start menu Pong entry missing (under Games)"
grep -Rq '{"Games", -1, 0}' src/window || fail "Start menu Games category header missing"
grep -Rq 'case 5: launchSnake' src/window || fail "Start menu Snake action missing"
grep -Rq 'case 6: launchMinesweeper' src/window || fail "Start menu Minesweeper action missing"
grep -Rq 'case 7: launchPong' src/window || fail "Start menu Pong action missing"
grep -Rq 'case 8: requestQuit' src/window || fail "Shut Down action not renumbered to 8"

grep -Rq 'claimNextAppInstanceTitle("Snake")' src/window \
  || fail "Snake instance titling missing"
grep -Rq 'claimNextAppInstanceTitle("Minesweeper")' src/window \
  || fail "Minesweeper instance titling missing"
grep -Rq 'claimNextAppInstanceTitle("Pong")' src/window \
  || fail "Pong instance titling missing"

grep -Rq 'w->app->update()' src/window \
  || fail "App::update dispatch missing in WindowManager::update"

grep -q 'apps/snake.md' docs/README.md || fail "snake.md not linked from docs hub"
grep -q 'apps/minesweeper.md' docs/README.md || fail "minesweeper.md not linked from docs hub"
grep -q 'apps/pong.md' docs/README.md || fail "pong.md not linked from docs hub"

grep -q 'snake_highscore' src/app/SnakeApp.cpp || fail "Snake high score path missing"
grep -q 'm_dirQueue' src/app/SnakeApp.cpp || fail "Snake direction queue missing"
grep -q 'chord' src/app/MinesweeperApp.cpp || fail "Minesweeper chord missing"
grep -q 'minesweeper_best' src/app/MinesweeperApp.cpp || fail "Minesweeper best times path missing"
grep -q 'Mark::Question' src/app/MinesweeperApp.cpp || fail "Minesweeper question marks missing"
grep -q 'State::GameOver' src/app/PongLogic.cpp || fail "Pong GameOver state missing"
grep -q 'kWinScore' src/app/PongLogic.hpp || fail "Pong win score missing"

ok "all games static integration checks passed"
