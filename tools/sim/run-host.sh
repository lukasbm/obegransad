#!/usr/bin/env bash
#
# Build and run the firmware natively for the Linux/POSIX host target with the
# simulator backends. No QEMU: the app is a normal process, the renderer shows
# the panel, and the host network stack is used directly (the config server,
# HA/MQTT and weather all talk to the real network without forwarding).
#
# Requires ESP-IDF with linux target support and the libbsd development headers
# (Fedora: libbsd-devel, Debian/Ubuntu: libbsd-dev). The target is
# experimental in ESP-IDF; see tools/sim/README.md for details.
#
# Uses its own build-host/ tree and sdkconfig, so the hardware and QEMU builds
# are never touched.
#
# Usage: tools/sim/run-host.sh [options]
#   --sim-port N     renderer TCP port (default 5566)
#   --fresh          regenerate build-host/sdkconfig from the sdkconfig.defaults*
#   --no-renderer    do not start the host renderer
#   -h, --help
#
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BUILD_DIR="$PROJECT_DIR/build-host"
SIM_PORT=5566
FRESH=0
RENDERER=1
RENDERER_PYTHON="${RENDERER_PYTHON:-python3}"

usage() {
  sed -n '3,18p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
  exit "${1:-0}"
}

while [ $# -gt 0 ]; do
  case "$1" in
    --sim-port) SIM_PORT="${2:?missing value}"; shift ;;
    --fresh) FRESH=1 ;;
    --no-renderer) RENDERER=0 ;;
    -h|--help) usage 0 ;;
    *) echo "error: unknown option '$1'" >&2; usage 1 ;;
  esac
  shift
done

# --- ESP-IDF environment -----------------------------------------------------
# Same logic as run-qemu.sh: adopt the environment of the export/activation
# script, running it in a bash subshell where $0 makes it look sourced.
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

# --- host toolchain ----------------------------------------------------------
# The Espressif toolchains put a RISC-V/xtensa 'as' on PATH; the host compiler
# would pick that up and fail. Drop only those directories.
PATH="$(echo "$PATH" | tr ':' '\n' |
        grep -vE 'riscv32-esp-elf|xtensa-esp-elf|esp-clang|esp-rom-elfs' |
        paste -sd:)"
export PATH

# --- libbsd development headers ----------------------------------------------
# IDF's linux target needs bsd/sys/cdefs.h and friends. OBG_LIBBSD_PREFIX can
# point at an extracted prefix (used for testing without root).
CMAKE_EXTRA=()
if [ ! -e /usr/include/bsd/sys/cdefs.h ] && [ ! -e /usr/local/include/bsd/sys/cdefs.h ]; then
  if [ -n "${OBG_LIBBSD_PREFIX:-}" ]; then
    CMAKE_EXTRA=(
      "-DCMAKE_PREFIX_PATH=$OBG_LIBBSD_PREFIX"
      "-DCMAKE_C_FLAGS=-I$OBG_LIBBSD_PREFIX/include"
      "-DCMAKE_CXX_FLAGS=-I$OBG_LIBBSD_PREFIX/include"
    )
  else
    echo "error: libbsd development headers not found (bsd/sys/cdefs.h)." >&2
    echo "       Install them, e.g. 'sudo dnf install libbsd-devel' (Fedora)" >&2
    echo "       or 'sudo apt install libbsd-dev' (Debian/Ubuntu), or set" >&2
    echo "       OBG_LIBBSD_PREFIX to an extracted libbsd prefix." >&2
    exit 1
  fi
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
DEFAULTS="$DEFAULTS;sdkconfig.defaults.host"

CMAKE_ARGS=(-B "$BUILD_DIR" "-DSDKCONFIG=$BUILD_DIR/sdkconfig"
            "-DSDKCONFIG_DEFAULTS=$DEFAULTS" "${CMAKE_EXTRA[@]}")

if [ "$FRESH" = 1 ]; then
  rm -f "$BUILD_DIR/sdkconfig"
elif [ -f "$BUILD_DIR/sdkconfig" ]; then
  for defaults_file in ${DEFAULTS//;/ }; do
    if [ "$PROJECT_DIR/$defaults_file" -nt "$BUILD_DIR/sdkconfig" ]; then
      echo "note: $defaults_file is newer than build-host/sdkconfig; its changes" >&2
      echo "      are not applied until you run with --fresh." >&2
      break
    fi
  done
fi

# --- target selection --------------------------------------------------------
# The linux target is a preview target; set it once per build tree. IDF_TARGET
# is set explicitly because idf.py's set-target does not inject it when a
# custom build directory is used.
if [ ! -f "$BUILD_DIR/sdkconfig" ] ||
   ! grep -q 'CONFIG_IDF_TARGET="linux"' "$BUILD_DIR/sdkconfig"; then
  ( cd "$PROJECT_DIR" &&
    IDF_TARGET=linux run_idf --preview "${CMAKE_ARGS[@]}" set-target linux )
fi

# --- host renderer -----------------------------------------------------------
RENDERER_PID=""
cleanup() {
  [ -n "$RENDERER_PID" ] && kill "$RENDERER_PID" 2>/dev/null || true
}
trap cleanup EXIT INT TERM

port_in_use() {
  ss -H -ltn 2>/dev/null | awk '{print $4}' | grep -qE "[:.]${1}\$"
}

if [ "$RENDERER" = 1 ]; then
  stale="$(pgrep -f "[t]ools/sim/renderer.py" || true)"
  if [ -n "$stale" ]; then
    echo "warning: stopping stale simulator renderer process(es): $stale" >&2
    # shellcheck disable=SC2086
    kill $stale 2>/dev/null || true
    sleep 1
  fi
  if port_in_use "$SIM_PORT"; then
    echo "error: TCP port $SIM_PORT is already in use;" >&2
    echo "       use --sim-port N or stop the process holding it." >&2
    exit 1
  fi
  if [ -z "${DISPLAY:-}" ] && [ -z "${WAYLAND_DISPLAY:-}" ]; then
    echo "warning: no DISPLAY/WAYLAND_DISPLAY; not starting the GUI renderer." >&2
    echo "         For a text view run: python3 tools/sim/renderer.py --ascii" >&2
  else
    "$RENDERER_PYTHON" "$PROJECT_DIR/tools/sim/renderer.py" \
        --port "$SIM_PORT" &
    RENDERER_PID=$!
  fi
fi

# --- build and run -----------------------------------------------------------
cd "$PROJECT_DIR"
run_idf "${CMAKE_ARGS[@]}" build
exec "$BUILD_DIR/obegransad.elf"
