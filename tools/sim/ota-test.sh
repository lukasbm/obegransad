#!/usr/bin/env bash
#
# QEMU OTA test: boot the simulator, push the current build-sim/obegransad.bin
# to POST /api/ota, wait for the restart and verify that the device booted the
# other slot.
#
# QEMU's user-mode host forwarding occasionally fails to deliver inbound
# connections (the host socket connects but slirp never forwards to the guest),
# so this script retries the whole run a few times. See tools/sim/README.md.
#
# Usage: tools/sim/ota-test.sh [options]
#   --http-port N   host port forwarded to the guest (default 8080)
#   --attempts N    whole-run retries (default 3)
#   --image PATH    image to upload (default build-sim/obegransad.bin)
#   -h, --help
#
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
HTTP_PORT=8080
ATTEMPTS=3
IMAGE="$PROJECT_DIR/build-sim/obegransad.bin"

usage() {
  sed -n '3,16p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
  exit "${1:-0}"
}

while [ $# -gt 0 ]; do
  case "$1" in
    --http-port) HTTP_PORT="${2:?missing value}"; shift ;;
    --attempts) ATTEMPTS="${2:?missing value}"; shift ;;
    --image) IMAGE="${2:?missing value}"; shift ;;
    -h|--help) usage 0 ;;
    *) echo "error: unknown option '$1'" >&2; usage 1 ;;
  esac
  shift
done

[ -f "$IMAGE" ] || { echo "error: image '$IMAGE' not found; run tools/sim/run-qemu.sh once" >&2; exit 1; }

API="http://127.0.0.1:${HTTP_PORT}/api/ota"

kill_qemu() {
  ps -eo pid=,comm= | awk '$2=="qemu-system-ris"' | awk '{print $1}' | xargs -r kill 2>/dev/null || true
}
poll_api() {
  for _ in $(seq 1 "${1:-30}"); do
    curl -s --max-time 2 "$API" >/dev/null 2>&1 && return 0
    sleep 1
  done
  return 1
}
slot_of() {
  curl -s --max-time 3 "$API" |
    python3 -c 'import json,sys; d=json.load(sys.stdin); print(d["running"]["label"])' 2>/dev/null
}

RUNNER_PID=""
stop_runner() {
  [ -n "$RUNNER_PID" ] && kill "$RUNNER_PID" 2>/dev/null || true
  # The runner traps TERM but does not kill its QEMU child, so stop that too.
  kill_qemu
  [ -n "$RUNNER_PID" ] && wait "$RUNNER_PID" 2>/dev/null || true
  RUNNER_PID=""
}
cleanup() {
  stop_runner
}
trap cleanup EXIT INT TERM

for attempt in $(seq 1 "$ATTEMPTS"); do
  echo "=== attempt $attempt/$ATTEMPTS ==="
  kill_qemu
  sleep 2
  "$PROJECT_DIR/tools/sim/run-qemu.sh" --no-monitor --no-renderer \
      --http-port "$HTTP_PORT" >/dev/null 2>&1 &
  RUNNER_PID=$!

  if ! poll_api 35; then
    echo "  host forwarding not usable in this run; retrying"
    stop_runner
    continue
  fi

  before="$(slot_of)"
  echo "  running $before, uploading $(basename "$IMAGE") ($(stat -c%s "$IMAGE") bytes)"
  response="$(curl -s --max-time 90 -w '\n%{http_code}' \
      -X POST -H 'Content-Type: application/octet-stream' -H 'Expect:' \
      --data-binary "@$IMAGE" "$API")"
  code="$(printf '%s' "$response" | tail -1)"
  body="$(printf '%s' "$response" | head -1)"
  echo "  HTTP $code: $body"
  if [ "$code" != "202" ]; then
    echo "  upload rejected; retrying"
    stop_runner
    continue
  fi

  sleep 8
  if ! poll_api 25; then
    echo "  device did not come back after the restart; retrying"
    stop_runner
    continue
  fi

  after="$(slot_of)"
  echo "  running $after"
  if [ -n "$before" ] && [ -n "$after" ] && [ "$before" != "$after" ]; then
    echo "OTA verified: $before -> $after"
    exit 0
  fi
  echo "  slot did not change; retrying"
  stop_runner
done

echo "error: OTA could not be verified in $ATTEMPTS attempts" >&2
exit 1
