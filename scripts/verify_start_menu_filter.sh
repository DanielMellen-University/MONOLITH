#!/usr/bin/env bash
# Static integration checks for Start menu type-ahead filter (7.6).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

fail() { echo "FAIL: $1" >&2; exit 1; }
ok() { echo "ok: $1"; }

grep -q 'filterStartMenuRows' src/window/StartMenuFilter.hpp \
  || fail "filterStartMenuRows missing from StartMenuFilter.hpp"
grep -q 'startMenuLabelMatches' src/window/StartMenuFilter.hpp \
  || fail "startMenuLabelMatches missing"

if ! grep -q 'm_startMenuFilter' src/window/WindowManager_private.inc \
   && ! grep -q 'm_startMenuFilter' src/window/WindowManager.hpp; then
  fail "m_startMenuFilter state missing"
fi
if ! grep -q 'applyStartMenuFilterEdit' src/window/WindowManager_private.inc \
   && ! grep -q 'applyStartMenuFilterEdit' src/window/WindowManager.hpp; then
  fail "applyStartMenuFilterEdit not declared"
fi
if ! grep -q 'invalidateStartMenuHitTargets' src/window/WindowManager_private.inc \
   && ! grep -q 'invalidateStartMenuHitTargets' src/window/WindowManager.hpp; then
  fail "invalidateStartMenuHitTargets not declared"
fi

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

grep -q 'StartMenuFilter.hpp' src/window/WindowManager.cpp \
  || fail "StartMenuFilter.hpp not included in WM"
ok "all Start menu type-ahead static integration checks passed"
