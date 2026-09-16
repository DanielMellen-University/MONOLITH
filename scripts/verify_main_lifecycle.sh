#!/usr/bin/env bash
# Static check that SDL-backed shell objects die before renderer shutdown.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

fail() { echo "FAIL: $1" >&2; exit 1; }
ok()   { echo "ok: $1"; }

main_body_dir="$(mktemp -d)"
combined="$(mktemp)"
trap 'rm -rf "$main_body_dir" "$combined"' EXIT

python3 src/decompress_main_bodies.py "$main_body_dir" >/dev/null \
    || fail "main body fragments could not be decompressed"
cat "$main_body_dir/main_body_a.inc" "$main_body_dir/main_body_b.inc" > "$combined"

line_number() {
    local pattern="$1"
    grep -n -m1 "$pattern" "$combined" | cut -d: -f1
}

fs_line="$(line_number 'Filesystem monolithFs')"
wm_line="$(line_number 'WindowManager wm')"
save_line="$(line_number 'wm.saveSession')"
stop_line="$(line_number 'SDL_StopTextInput')"
destroy_line="$(line_number 'SDL_DestroyRenderer')"

[[ "$fs_line" -lt "$wm_line" ]] || fail "filesystem must outlive the Window Manager"
[[ "$wm_line" -lt "$save_line" ]] || fail "Window Manager must remain alive through session save"
[[ "$save_line" -lt "$stop_line" ]] || fail "SDL text input shutdown must follow Window Manager destruction"
[[ "$stop_line" -lt "$destroy_line" ]] || fail "renderer shutdown must follow SDL text input shutdown"

ok "SDL-backed Window Manager resources are scoped before renderer shutdown"
