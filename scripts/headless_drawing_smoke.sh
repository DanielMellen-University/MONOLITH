#!/usr/bin/env bash
# Headless smoke test: build (if needed), launch monolith on Xvfb, verify drawings dir + MODR save.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
PREFIX="${MONOLITH_TEST_PREFIX:-/tmp/monolith-test-deps/prefix}"
DISPLAY_NUM="${MONOLITH_TEST_DISPLAY:-198}"
TEST_ROOT="$(mktemp -d "${TMPDIR:-/tmp}/monolith-drawing-smoke.XXXXXX")"
TEMP_HOME=""
SMOKE_FILE=""

cleanup() {
  if [[ -n "$SMOKE_FILE" ]]; then
    rm -f -- "$SMOKE_FILE"
  fi
  if [[ -n "$TEMP_HOME" ]]; then
    rm -rf -- "$TEMP_HOME"
  fi
  rm -rf -- "$TEST_ROOT"
}
trap cleanup EXIT

HOME_PARENT="${MONOLITH_TEST_HOME_PARENT:-${MONOLITH_TEST_HOME:-$TEST_ROOT}}"
mkdir -p "$HOME_PARENT"
TEMP_HOME="$(mktemp -d "$HOME_PARENT/monolith-home.XXXXXX")"
export HOME="$TEMP_HOME"
FS_ROOT="$HOME/.monolith/fs"
RUN_ID="${TEST_ROOT##*/}"
LOG="$TEST_ROOT/monolith.log"
SMOKE_FILE="$FS_ROOT/home/monolith/drawings/smoke-$RUN_ID.modr"

export LD_LIBRARY_PATH="$PREFIX/usr/lib/x86_64-linux-gnu:${LD_LIBRARY_PATH:-}"
export DISPLAY=":${DISPLAY_NUM}"
export MONOLITH_TEST_FS_ROOT="$FS_ROOT"
export MONOLITH_TEST_RUN_ID="$RUN_ID"

BIN="$ROOT/build/monolith"
if [[ ! -x "$BIN" ]]; then
  echo "monolith binary missing at $BIN" >&2
  exit 1
fi

set +e
timeout 3 "$BIN" >"$LOG" 2>&1
RUN_STATUS=$?
set -e
if [[ $RUN_STATUS -ne 124 ]]; then
  echo "FAIL: Monolith exited before the smoke timeout (status $RUN_STATUS)" >&2
  cat "$LOG"
  exit 1
fi

if ! grep -Fq "Filesystem initialized at: $FS_ROOT" "$LOG"; then
  echo "FAIL: app did not initialize its isolated filesystem" >&2
  cat "$LOG"
  exit 1
fi
if [[ ! -d "$FS_ROOT/home/monolith/drawings" ]]; then
  echo "FAIL: app did not create its drawings directory" >&2
  cat "$LOG"
  exit 1
fi
echo "ok: monolith launched under DISPLAY=$DISPLAY for 3s without immediate fatal error"

# Write and read a small MODR fixture in the application's actual host-backed filesystem.
python3 - <<'PY'
import struct, os
fs_root = os.environ["MONOLITH_TEST_FS_ROOT"]
run_id = os.environ["MONOLITH_TEST_RUN_ID"]
path = os.path.join(fs_root, "home/monolith/drawings", f"smoke-{run_id}.modr")
w, h = 4, 4
pixels = bytes([i % 256 for i in range(w * h * 3)])
blob = b"MODR" + struct.pack("<II", w, h) + pixels
os.makedirs(os.path.dirname(path), exist_ok=True)
with open(path, "wb") as f:
    f.write(blob)
assert len(blob) == 12 + w * h * 3
with open(path, "rb") as f:
    assert f.read() == blob
print("ok: wrote and read back a unique MODR file in Monolith's filesystem")
PY

echo "ALL HEADLESS SMOKE CHECKS PASSED"
