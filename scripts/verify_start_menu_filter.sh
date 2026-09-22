#!/usr/bin/env bash
# Static integration checks for Start menu type-ahead filter (7.6).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

fail() { echo "FAIL: $1" >&2; exit 1; }
ok() { echo "ok: $1"; }

grep -q 'filterStartMenuRows' src/window/AppRegistry.hpp \
  || fail "filterStartMenuRows missing from AppRegistry"
grep -q 'startMenuLabelMatches' src/window/AppRegistry.hpp \
  || fail "startMenuLabelMatches missing"
grep -q 'm_startMenuFilter' src/window/WindowManager_private.inc \
  || fail "m_startMenuFilter state missing"
grep -q 'applyStartMenuFilterEdit' src/window/WindowManager_private.inc \
  || fail "applyStartMenuFilterEdit not declared"
grep -q 'invalidateStartMenuHitTargets' src/window/WindowManager_private.inc \
  || fail "invalidateStartMenuHitTargets not declared"

grep -q 'filterStartMenuRows' src/window/detail/wm_start_menu_rows.inc \
  || fail "wm_start_menu_rows.inc must filter rows"
grep -q 'm_startMenuFilter' src/window/detail/wm_start_menu_rows.inc \
  || fail "wm_start_menu_rows.inc must read m_startMenuFilter"

grep -q 'SDL_TEXTINPUT' src/window/detail/wm_body_08c.inc \
  || fail "TEXTINPUT not wired for Start menu type-ahead"
grep -q 'SDLK_BACKSPACE' src/window/detail/wm_body_08c.inc \
  || fail "Backspace not wired for Start menu filter"
grep -q 'm_startMenuFilter.clear()' src/window/detail/wm_body_08c.inc \
  || fail "filter clear on Escape/close missing"

grep -q 'm_startMenuFilter' src/window/detail/wm_body_03a.inc \
  || fail "filter chrome not drawn in Start menu header"
grep -q 'headerHLogical' src/window/detail/wm_body_03a.inc \
  || fail "filter-aware header height missing in Start menu chrome"
grep -q 'headerHLogical' src/window/detail/wm_start_menu_entries.inc \
  || fail "filter-aware header height missing in hit rebuild"

ok "all Start menu type-ahead static integration checks passed"
