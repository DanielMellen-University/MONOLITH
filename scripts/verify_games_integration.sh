#!/usr/bin/env bash
# Static integration checks for Snake and Minesweeper (no SDL/display required).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

fail() { echo "FAIL: $1" >&2; exit 1; }
ok()   { echo "ok: $1"; }

[[ -f src/app/SnakeApp.hpp ]] || fail "SnakeApp.hpp missing"
[[ -f src/app/SnakeApp.cpp ]] || fail "SnakeApp.cpp missing"
[[ -f src/app/MinesweeperApp.hpp ]] || fail "MinesweeperApp.hpp missing"
[[ -f src/app/MinesweeperApp.cpp ]] || fail "MinesweeperApp.cpp missing"

grep -q 'launchSnake' src/window/WindowManager.hpp || fail "launchSnake not declared"
grep -q 'launchMinesweeper' src/window/WindowManager.hpp || fail "launchMinesweeper not declared"
grep -q 'launchSnake' src/window/WindowManager.cpp || fail "launchSnake not implemented"
grep -q 'launchMinesweeper' src/window/WindowManager.cpp || fail "launchMinesweeper not implemented"
grep -q 'SnakeApp' src/window/WindowManager.cpp || fail "SnakeApp not included in WM"
grep -q 'MinesweeperApp' src/window/WindowManager.cpp || fail "MinesweeperApp not included in WM"
grep -q 'SnakeApp.cpp' CMakeLists.txt || fail "SnakeApp.cpp not in CMakeLists"
grep -q 'MinesweeperApp.cpp' CMakeLists.txt || fail "MinesweeperApp.cpp not in CMakeLists"

grep -q '{"Snake", 5}' src/window/WindowManager.cpp || fail "Start menu Snake entry missing"
grep -q '{"Minesweeper", 6}' src/window/WindowManager.cpp || fail "Start menu Minesweeper entry missing"
grep -q 'case 5: launchSnake' src/window/WindowManager.cpp || fail "Start menu Snake action missing"
grep -q 'case 6: launchMinesweeper' src/window/WindowManager.cpp || fail "Start menu Minesweeper action missing"
grep -q 'case 7: requestQuit' src/window/WindowManager.cpp || fail "Shut Down action not renumbered to 7"

grep -q 'claimNextAppInstanceTitle("Snake")' src/window/WindowManager.cpp \
  || fail "Snake instance titling missing"
grep -q 'claimNextAppInstanceTitle("Minesweeper")' src/window/WindowManager.cpp \
  || fail "Minesweeper instance titling missing"

grep -q 'w->app->update()' src/window/WindowManager.cpp \
  || fail "App::update dispatch missing in WindowManager::update"

grep -q 'apps/snake.md' docs/README.md || fail "snake.md not linked from docs hub"
grep -q 'apps/minesweeper.md' docs/README.md || fail "minesweeper.md not linked from docs hub"

ok "all games static integration checks passed"
