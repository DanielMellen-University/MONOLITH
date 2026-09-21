# Shell polish

Unify Start menu hit-testing with `AppRegistry` and retire the duplicate hardcoded entries table.

## Change

- `wm_start_menu_entries.inc` no longer lists apps by hand. It rebuilds click targets from `buildStartMenuRows()` via `wm_start_menu_rows.inc` (same source as Start menu rendering).
- Render (`wm_body_03a.inc`) and pre-render hit testing (`ensureStartMenuHitTargets`) therefore share one AppRegistry-driven menu.
- Integration verifies assert the hit include uses registry rows and that the old hardcoded `{"Terminal", 0, 0}` table stays gone.

## Verify

```bash
bash scripts/verify_desktop_icons.sh
bash scripts/verify_games_integration.sh
bash scripts/verify_drawing_integration.sh
c++ -std=c++23 -I src scripts/test_app_registry.cpp $(pkg-config --cflags --libs sdl2) -o /tmp/test_app_registry && /tmp/test_app_registry
```
