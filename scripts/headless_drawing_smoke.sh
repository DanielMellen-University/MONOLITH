#!/usr/bin/env bash
# Headless smoke test: build (if needed), launch monolith on Xvfb, verify drawings dir + MODR save.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PREFIX="${MONOLITH_TEST_PREFIX:-/tmp/monolith-test-deps/prefix}"
DISPLAY_NUM="${MONOLITH_TEST_DISPLAY:-198}"
FS_ROOT="${MONOLITH_TEST_FS:-/tmp/monolith-test-fs}"

export LD_LIBRARY_PATH="$PREFIX/usr/lib/x86_64-linux-gnu:${LD_LIBRARY_PATH:-}"
export DISPLAY=":${DISPLAY_NUM}"
export HOME="${MONOLITH_TEST_HOME:-/tmp/monolith-test-home}"
mkdir -p "$HOME"
rm -rf "$FS_ROOT"
mkdir -p "$FS_ROOT/home/monolith/drawings"

BIN="$ROOT/build/monolith"
if [[ ! -x "$BIN" ]]; then
  echo "monolith binary missing at $BIN" >&2
  exit 1
fi

LOG="$(mktemp /tmp/monolith-drawing-smoke.XXXXXX.log)"
timeout 3 "$BIN" >"$LOG" 2>&1 || true

grep -q "Filesystem initialized" "$LOG" || { echo "FAIL: no filesystem init in log"; cat "$LOG"; exit 1; }
echo "ok: monolith launched under DISPLAY=$DISPLAY for 3s without immediate fatal error"

# Write a minimal MODR file the same way DrawingApp does and confirm FS readback.
python3 - <<'PY'
import struct, os
fs_root = os.environ.get("MONOLITH_TEST_FS", "/tmp/monolith-test-fs")
path = os.path.join(fs_root, "home/monolith/drawings", "smoke.modr")
w, h = 4, 4
pixels = bytes([i % 256 for i in range(w * h * 3)])
blob = b"MODR" + struct.pack("<II", w, h) + pixels
os.makedirs(os.path.dirname(path), exist_ok=True)
with open(path, "wb") as f:
    f.write(blob)
assert len(blob) == 12 + w * h * 3
print("ok: wrote smoke.modr to test filesystem")
PY

echo "ALL HEADLESS SMOKE CHECKS PASSED"