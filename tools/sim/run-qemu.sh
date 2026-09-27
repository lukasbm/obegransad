#!/usr/bin/env bash
#
# Build and run the obegransad firmware in QEMU with the simulator backends:
# the 16x16 panel is rendered by tools/sim/renderer.py and button presses come
# from the renderer instead of GPIO. Networking uses QEMU's emulated Ethernet
# (openeth); "Wi-Fi" credentials are irrelevant.
#
# Uses its own build-sim/ tree and sdkconfig, so the hardware build and its
# sdkconfig are never touched.
#
# Usage: tools/sim/run-qemu.sh [options]
#   --sim-port N     renderer TCP port (default 5566)
#   --http-port N    host port forwarded to guest port 80 (default 8080,
#                    0 disables the forward)
#   --persist        keep NVS (Wi-Fi/config state) across runs
#   --fresh          regenerate build-sim/sdkconfig from the sdkconfig.defaults*
#   --no-renderer    do not start the host renderer
#   --no-monitor     run QEMU without idf.py monitor
#   -h, --help
#
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="$PROJECT_DIR/build-sim"
SIM_PORT=5566
HTTP_PORT=8080
PERSIST=0
RENDERER=1
MONITOR=1
FRESH=0
RENDERER_PYTHON="${RENDERER_PYTHON:-python3}"

usage() {
  sed -n '3,19p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
  exit "${1:-0}"
}

while [ $# -gt 0 ]; do
  case "$1" in
    --sim-port) SIM_PORT="${2:?missing value}"; shift ;;
    --http-port) HTTP_PORT="${2:?missing value}"; shift ;;
    --persist) PERSIST=1 ;;
    --fresh) FRESH=1 ;;
    --no-renderer) RENDERER=0 ;;
    --no-monitor) MONITOR=0 ;;
    -h|--help) usage 0 ;;
    *) echo "error: unknown option '$1'" >&2; usage 1 ;;
  esac
  shift
done

# --- ESP-IDF environment -----------------------------------------------------
# idf.py needs both IDF_PATH and the IDF python venv; if either is missing,
# adopt the environment from the export/activation script. The Espressif
# activation script refuses to be sourced by another script (and calls exit),
# so run it in a bash subshell where $0 makes it look sourced.
if [ -z "${IDF_PATH:-}" ] || [ -z "${IDF_PYTHON_ENV_PATH:-}" ]; then
  for export_script in \
      "$HOME/.espressif/tools/activate_idf_v6.0.1.sh" \
      "$HOME/esp/esp-idf/export.sh" \
      "$HOME/.espressif/esp-idf/export.sh"; do
    if [ -f "$export_script" ]; then
      # Adopt the whole environment the activation script would export
      # (IDF_PATH, IDF_PYTHON_ENV_PATH, ESP_IDF_VERSION, toolchain PATH, ...).
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

# --- sdkconfig layering ------------------------------------------------------
DEFAULTS="sdkconfig.defaults"
if [ -f "$PROJECT_DIR/sdkconfig.defaults.local" ]; then
  DEFAULTS="$DEFAULTS;sdkconfig.defaults.local"
fi
DEFAULTS="$DEFAULTS;sdkconfig.defaults.sim"

CMAKE_ARGS=(-B "$BUILD_DIR" "-DSDKCONFIG=$BUILD_DIR/sdkconfig"
            "-DSDKCONFIG_DEFAULTS=$DEFAULTS" -DIDF_TARGET=esp32c3)

# sdkconfig defaults only apply when the sdkconfig is (re)created; an existing
# file wins. Warn when the defaults changed, and let --fresh regenerate.
if [ "$FRESH" = 1 ]; then
  rm -f "$BUILD_DIR/sdkconfig"
elif [ -f "$BUILD_DIR/sdkconfig" ]; then
  for defaults_file in ${DEFAULTS//;/ }; do
    if [ "$PROJECT_DIR/$defaults_file" -nt "$BUILD_DIR/sdkconfig" ]; then
      echo "note: $defaults_file is newer than build-sim/sdkconfig; its changes" >&2
      echo "      are not applied until you run with --fresh." >&2
      break
    fi
  done
fi

# --- QEMU networking ---------------------------------------------------------
# Emulated OpenCores Ethernet with slirp user-mode networking (DHCP + NAT).
# hostfwd makes an app HTTP server on guest port 80 reachable as
# http://127.0.0.1:$HTTP_PORT/ from the host.
NIC="user,model=open_eth,id=lo0"
if [ "$HTTP_PORT" != "0" ]; then
  NIC="$NIC,hostfwd=tcp:127.0.0.1:${HTTP_PORT}-:80"
fi
QEMU_ARGS=(--qemu-extra-args="-nic $NIC")

# --- optional persistent NVS -------------------------------------------------
# idf.py regenerates qemu_flash.bin on every run, which would wipe NVS. With
# --persist we merge a fresh image (new code) but copy the NVS partition over
# from the previous run; the NVS offset/size come from partitions.csv.
NVS_OFFSET=0x9000
NVS_SIZE=0x6000
merge_flash() {
  local out="$1"
  ( cd "$BUILD_DIR" && "$IDF_PYTHON" -m esptool --chip=esp32c3 merge-bin \
      --output="$out" --pad-to-size=4MB @flash_args )
}

if [ "$PERSIST" = 1 ]; then
  run_idf "${CMAKE_ARGS[@]}" build
  merge_flash "$BUILD_DIR/qemu_flash_new.bin"
  if [ -f "$BUILD_DIR/qemu_flash_persist.bin" ]; then
    dd if="$BUILD_DIR/qemu_flash_persist.bin" of="$BUILD_DIR/qemu_flash_new.bin" \
       bs=1 skip=$((NVS_OFFSET)) seek=$((NVS_OFFSET)) count=$((NVS_SIZE)) \
       conv=notrunc status=none
  fi
  mv "$BUILD_DIR/qemu_flash_new.bin" "$BUILD_DIR/qemu_flash_persist.bin"
  QEMU_ARGS+=(--flash-file "$BUILD_DIR/qemu_flash_persist.bin")
fi

# --- preflight: stale instances and busy ports --------------------------------
# Leftover simulator processes are the usual reason a run fails with
# "Timed out waiting for port 5555 to be open" (QEMU exits because the host
# forward or monitor port is taken) or with an "Address already in use" bind
# error. QEMU/renderer processes started from this project are safe to stop.
stale="$(pgrep -f "[b]uild-sim/qemu_flash" || true)"
if [ -n "$stale" ]; then
  echo "warning: stopping stale simulator QEMU process(es): $stale" >&2
  # shellcheck disable=SC2086
  kill $stale 2>/dev/null || true
  sleep 1
fi
if [ "$RENDERER" = 1 ]; then
  stale="$(pgrep -f "[t]ools/sim/renderer.py" || true)"
  if [ -n "$stale" ]; then
    echo "warning: stopping stale simulator renderer process(es): $stale" >&2
    # shellcheck disable=SC2086
    kill $stale 2>/dev/null || true
    sleep 1
  fi
fi

port_in_use() {
  ss -H -ltn 2>/dev/null | awk '{print $4}' | grep -qE "[:.]${1}\$"
}
if [ "$RENDERER" = 1 ] && port_in_use "$SIM_PORT"; then
  echo "error: TCP port $SIM_PORT is already in use (another renderer?);" >&2
  echo "       use --sim-port N or stop the process holding it." >&2
  exit 1
fi
if [ "$MONITOR" = 1 ] && port_in_use 5555; then
  echo "error: QEMU monitor port 5555 is already in use (stale QEMU?);" >&2
  echo "       use --no-monitor or stop the process holding it." >&2
  exit 1
fi
if [ "$HTTP_PORT" != 0 ] && port_in_use "$HTTP_PORT"; then
  echo "error: host HTTP port $HTTP_PORT is already in use;" >&2
  echo "       use --http-port N (or 0 to disable the forward)." >&2
  exit 1
fi

# --- host renderer -----------------------------------------------------------
RENDERER_PID=""
cleanup() {
  [ -n "$RENDERER_PID" ] && kill "$RENDERER_PID" 2>/dev/null || true
}
trap cleanup EXIT INT TERM

if [ "$RENDERER" = 1 ]; then
  if [ -z "${DISPLAY:-}" ] && [ -z "${WAYLAND_DISPLAY:-}" ]; then
    echo "warning: no DISPLAY/WAYLAND_DISPLAY; not starting the GUI renderer." >&2
    echo "         For a text view run: python3 tools/sim/renderer.py --ascii" >&2
  else
    "$RENDERER_PYTHON" "$PROJECT_DIR/tools/sim/renderer.py" \
        --port "$SIM_PORT" --host 0.0.0.0 &
    RENDERER_PID=$!
  fi
fi

# --- go ----------------------------------------------------------------------
cd "$PROJECT_DIR"
if [ "$MONITOR" = 1 ]; then
  run_idf "${CMAKE_ARGS[@]}" qemu "${QEMU_ARGS[@]}" monitor
else
  run_idf "${CMAKE_ARGS[@]}" qemu "${QEMU_ARGS[@]}"
fi
