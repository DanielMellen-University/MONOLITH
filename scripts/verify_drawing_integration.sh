#!/usr/bin/env bash
# Static integration checks for the Drawing app (no SDL/display required).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

fail() { echo "FAIL: $1" >&2; exit 1; }
ok()   { echo "ok: $1"; }

[[ -f src/app/DrawingApp.hpp ]] || fail "DrawingApp.hpp missing"
[[ -f src/app/DrawingApp.cpp ]] || fail "DrawingApp.cpp missing"

grep -q 'launchDrawing' src/window/WindowManager.hpp || fail "launchDrawing not declared"
grep -q 'launchDrawing' src/window/WindowManager.cpp || fail "launchDrawing not implemented"
grep -q 'DrawingApp' src/window/WindowManager.cpp || fail "DrawingApp not included in WM"
grep -q 'DrawingApp.cpp' CMakeLists.txt || fail "DrawingApp.cpp not in CMakeLists"
grep -q 'drawings' src/main.cpp || fail "drawings dir seed missing in main.cpp"
grep -q '{"Drawing", 4}' src/window/WindowManager.cpp || fail "Start menu Drawing entry missing"
grep -q 'SDL_MOUSEBUTTONUP' src/window/WindowManager.cpp || fail "mouse-up forwarding missing"

grep -q 'claimNextAppInstanceTitle("Drawing")' src/window/WindowManager.cpp \
  || fail "Drawing instance titling missing"

grep -q 'kModrMagic' src/app/DrawingApp.cpp || fail "MODR format missing in DrawingApp"
grep -q 'Ctrl+S' src/app/DrawingApp.cpp || fail "save shortcut missing"
grep -q 'Tool::Eraser' src/app/DrawingApp.cpp || fail "eraser tool missing"

ok "all static integration checks passed"