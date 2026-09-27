#!/usr/bin/env bash
#
# Wired flash for the hardware build.
#
# Same as `idf.py -p PORT flash`, but also erases the otadata partition
# (0x10000, 8 KiB) so the device boots the freshly flashed image instead of a
# previously OTA-updated slot. NVS (Wi-Fi/config) is untouched.
#
# Usage: tools/flash.sh --port /dev/ttyACM0 [--erase-nvs]
#   --port PORT    serial port (find it with `idf.py -p ... monitor`)
#   --erase-nvs    also erase the NVS partition (wipes Wi-Fi/config state)
#   -h, --help
#
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
PORT=""
ERASE_NVS=0

OTADATA_OFFSET=0x10000
OTADATA_SIZE=0x2000
NVS_OFFSET=0x9000
NVS_SIZE=0x6000

usage() {
  sed -n '3,13p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
  exit "${1:-0}"
}

while [ $# -gt 0 ]; do
  case "$1" in
    --port) PORT="${2:?missing value}"; shift ;;
    --erase-nvs) ERASE_NVS=1 ;;
    -h|--help) usage 0 ;;
    *) echo "error: unknown option '$1'" >&2; usage 1 ;;
  esac
  shift
done

if [ -z "$PORT" ]; then
  echo "error: --port is required (e.g. --port /dev/ttyACM0)" >&2
  exit 1
fi

# --- ESP-IDF environment -----------------------------------------------------
if [ -z "${IDF_PATH:-}" ] || [ -z "${IDF_PYTHON_ENV_PATH:-}" ]; then
  for export_script in \
      "$HOME/.espressif/tools/activate_idf_v6.0.1.sh" \
      "$HOME/esp/esp-idf/export.sh" \
      "$HOME/.espressif/esp-idf/export.sh"; do
    if [ -f "$export_script" ]; then
      _idf_env="$(bash -c 'set +u; . "$1" >/dev/null 2>&1; export -p' \
                      bash "$export_script" 2>/dev/null)" || true
      eval "$_idf_env"
      unset _idf_env
      break
    fi
  done
fi
if [ -z "${IDF_PATH:-}" ]; then
  echo "error: ESP-IDF not found. Source your export script first (e.g. the 'esp' alias)." >&2
  exit 1
fi

IDF_PYTHON="${IDF_PYTHON_ENV_PATH:+$IDF_PYTHON_ENV_PATH/bin/python}"
IDF_PYTHON="${IDF_PYTHON:-python3}"
IDF_PY="$IDF_PATH/tools/idf.py"
[ -f "$IDF_PY" ] || { echo "error: $IDF_PY not found" >&2; exit 1; }

run_idf() { "$IDF_PYTHON" "$IDF_PY" "$@"; }

# --- flash -------------------------------------------------------------------
cd "$PROJECT_DIR"
echo "Flashing bootloader, partition table and app (ota_0) to $PORT ..."
run_idf -p "$PORT" flash

if [ "$ERASE_NVS" = 1 ]; then
  echo "Erasing NVS (Wi-Fi credentials and configuration) ..."
  "$IDF_PYTHON" -m esptool --chip=esp32c3 -p "$PORT" erase-region \
      "$NVS_OFFSET" "$NVS_SIZE"
fi

echo "Erasing otadata so the freshly flashed ota_0 boots ..."
"$IDF_PYTHON" -m esptool --chip=esp32c3 -p "$PORT" erase-region \
    "$OTADATA_OFFSET" "$OTADATA_SIZE"

echo "Done. The device now boots the image written to ota_0."
