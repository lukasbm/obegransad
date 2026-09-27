#!/usr/bin/env bash
#
# Push a firmware image to a device's OTA endpoint (POST /api/ota).
#
# The token is required: it must match CONFIG_OBG_OTA_TOKEN in the firmware.
# Resolution order: --token, $OBG_OTA_TOKEN, then CONFIG_OBG_OTA_TOKEN parsed
# from sdkconfig.defaults.local / sdkconfig.
#
# Usage: tools/ota-push.sh [--host HOST] [--port N] [--image FILE] [--token T]
#   --host HOST    device (or 127.0.0.1 for QEMU's host forward; default)
#   --port N       config server port (default 8080)
#   --image FILE   image to upload (default build/obegransad.bin, falling back
#                  to build-sim/obegransad.bin)
#   --token T      OTA token (see above)
#   -h, --help
#
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
HOST="127.0.0.1"
PORT=8080
IMAGE=""
TOKEN="${OBG_OTA_TOKEN:-}"

usage() {
  sed -n '3,17p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
  exit "${1:-0}"
}

while [ $# -gt 0 ]; do
  case "$1" in
    --host) HOST="${2:?missing value}"; shift ;;
    --port) PORT="${2:?missing value}"; shift ;;
    --image) IMAGE="${2:?missing value}"; shift ;;
    --token) TOKEN="${2:?missing value}"; shift ;;
    -h|--help) usage 0 ;;
    *) echo "error: unknown option '$1'" >&2; usage 1 ;;
  esac
  shift
done

if [ -z "$IMAGE" ]; then
  for candidate in "$PROJECT_DIR/build/obegransad.bin" \
                   "$PROJECT_DIR/build-sim/obegransad.bin"; do
    if [ -f "$candidate" ]; then
      IMAGE="$candidate"
      break
    fi
  done
fi
[ -n "$IMAGE" ] && [ -f "$IMAGE" ] || {
  echo "error: no image found (build one or pass --image)" >&2
  exit 1
}

if [ -z "$TOKEN" ]; then
  TOKEN="$(sed -n 's/^CONFIG_OBG_OTA_TOKEN="\(.*\)"$/\1/p' \
      "$PROJECT_DIR/sdkconfig.defaults.local" "$PROJECT_DIR/sdkconfig" \
      2>/dev/null | tail -1)"
fi
if [ -z "$TOKEN" ]; then
  echo "error: no OTA token. Set CONFIG_OBG_OTA_TOKEN in sdkconfig.defaults.local," >&2
  echo "       export OBG_OTA_TOKEN, or pass --token." >&2
  exit 1
fi

URL="http://${HOST}:${PORT}/api/ota"
echo "Target $URL"
echo "Image  $IMAGE ($(stat -c%s "$IMAGE") bytes)"

current="$(curl -s --max-time 5 "$URL" || true)"
[ -n "$current" ] && echo "Before: $current"

code="$(curl -sS --max-time 300 -o /tmp/ota-push-response.json -w '%{http_code}' \
    -X POST \
    -H 'Content-Type: application/octet-stream' \
    -H 'Expect:' \
    -H "X-OTA-Token: ${TOKEN}" \
    --data-binary "@$IMAGE" "$URL")"
echo "HTTP $code: $(cat /tmp/ota-push-response.json)"
rm -f /tmp/ota-push-response.json

case "$code" in
  202) echo "Image accepted; the device validates, switches slots and restarts." ;;
  401) echo "Rejected: token does not match CONFIG_OBG_OTA_TOKEN on the device." >&2; exit 1 ;;
  403) echo "Rejected: OTA disabled on the device or no station connection." >&2; exit 1 ;;
  400) echo "Rejected: transfer aborted, invalid image, or not an obegransad image." >&2; exit 1 ;;
  *)   echo "Unexpected response." >&2; exit 1 ;;
esac
