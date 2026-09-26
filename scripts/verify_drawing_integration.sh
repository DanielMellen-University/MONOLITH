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
grep -R -q 'launchDrawing' src/window || fail "launchDrawing not implemented"
grep -R -q 'DrawingApp' src/window || fail "DrawingApp not included in WM"
grep -q 'DrawingApp.cpp' CMakeLists.txt || fail "DrawingApp.cpp not in CMakeLists"
main_body_dir="$(mktemp -d)"
trap 'rm -rf "$main_body_dir"' EXIT
python3 src/decompress_main_bodies.py "$main_body_dir" >/dev/null \
  || fail "main body fragments could not be decompressed"
grep -R -q 'drawings' "$main_body_dir" \
  || fail "drawings dir seed missing in main body fragments"
grep -q 'AppAction::Drawing' src/window/AppRegistry.hpp || fail "Drawing registry entry missing"
grep -q 'wm_start_menu_rows.inc' src/window/detail/wm_start_menu_entries.inc \
  || fail "Start menu hit path must use registry rows"
grep -R -q 'SDL_MOUSEBUTTONUP' src/window || fail "mouse-up forwarding missing"

grep -R -q 'claimNextAppInstanceTitle("Drawing")' src/window \
  || fail "Drawing instance titling missing"

grep -q 'kModrMagic' src/app/DrawingRaster.cpp || fail "MODR format missing in DrawingRaster"
grep -q 'Ctrl+S' src/app/DrawingApp.cpp || fail "save shortcut missing"
grep -q 'Tool::Eraser' src/app/DrawingApp.cpp || fail "eraser tool missing"
grep -q 'Tool::Eyedropper' src/app/DrawingApp.cpp || fail "eyedropper tool missing"
grep -q 'getPixel' src/app/DrawingRaster.cpp || fail "pixel sampling helper missing"

ok "all static integration checks passed"
