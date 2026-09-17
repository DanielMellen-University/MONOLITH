#!/usr/bin/env bash
# Static integration checks for desktop icons (7.3+) and App registry (7.4).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

fail() { echo "FAIL: $1" >&2; exit 1; }
ok() { echo "ok: $1"; }

[[ -f src/window/AppRegistry.hpp ]] || fail "AppRegistry.hpp missing"
[[ -f src/window/DesktopIcons.hpp ]] || fail "DesktopIcons.hpp missing"
[[ -f src/window/detail/wm_desktop_icons.inc ]] || fail "wm_desktop_icons.inc missing"

grep -q 'layoutDesktopIcons' src/window/DesktopIcons.hpp || fail "layoutDesktopIcons missing"
grep -q 'hitTestDesktopIcon' src/window/DesktopIcons.hpp || fail "hitTestDesktopIcon missing"
grep -q 'isDesktopIconDoubleClick' src/window/DesktopIcons.hpp || fail "isDesktopIconDoubleClick missing"
grep -q 'collectDesktopIconDefs' src/window/DesktopIcons.hpp || fail "collectDesktopIconDefs missing"
grep -q 'showDesktopIcon' src/window/AppRegistry.hpp || fail "registry desktop-icon flag missing"

grep -q 'DesktopIcons.hpp' src/window/WindowManager.cpp || fail "DesktopIcons.hpp not included in WM"
grep -q 'AppRegistry.hpp\|DesktopIcons.hpp' src/window/WindowManager.hpp || fail "registry not visible to WM"
grep -q 'wm_desktop_icons.inc' src/window/WindowManager.cpp || fail "wm_desktop_icons.inc not included in WM"
grep -q 'renderDesktopIcons' src/window/WindowManager_private.inc || fail "renderDesktopIcons not declared"
grep -q 'tryHandleDesktopIconClick' src/window/WindowManager_private.inc || fail "tryHandleDesktopIconClick not declared"
grep -Rq 'renderDesktopIcons' src/window/detail || fail "renderDesktopIcons not called from render path"
grep -Rq 'tryHandleDesktopIconClick' src/window/detail || fail "tryHandleDesktopIconClick not wired into input"
grep -Rq 'launchByAction' src/window || fail "launchByAction missing"

grep -q 'AppAction::Terminal' src/window/AppRegistry.hpp || fail "Terminal registry entry missing"
grep -q 'AppAction::Filesystem' src/window/AppRegistry.hpp || fail "Filesystem registry entry missing"
grep -q 'AppAction::TextEditor' src/window/AppRegistry.hpp || fail "Editor registry entry missing"
grep -q 'AppAction::Drawing' src/window/AppRegistry.hpp || fail "Drawing registry entry missing"
grep -q 'AppAction::Settings' src/window/AppRegistry.hpp || fail "Settings registry entry missing"

ok "all desktop icon static integration checks passed"
