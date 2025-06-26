#!/usr/bin/env bash
#
# Usage:  ./run-qemu.sh [pio_env]  [flash_size]
#         pio_env     – PlatformIO environment name      (default: xiao-esp32c3)
#         flash_size  – QEMU flash size: 2MB | 4MB | 8MB (default: 4MB)
#
set -euo pipefail

ENV="${1:-seeed_xiao_esp32c3}"
FLASH_SZ="${2:-4MB}"

BUILD_DIR=".pio/build/${ENV}"
OUT_BIN="${BUILD_DIR}/qemu_flash.bin"

# 1. compile (only if something changed)
pio run -e "${ENV}"

# 2. locate the binaries produced by PlatformIO
BOOT="${BUILD_DIR}/bootloader.bin"
PART="${BUILD_DIR}/partitions.bin"
APP="${BUILD_DIR}/firmware.bin"
# OTA="${BUILD_DIR}/ota_data_initial.bin" # may be missing on very old cores
# BOOT_APP0="${BUILD_DIR}/boot_app0.bin"  # needed for OTA layouts; exists in current cores

# 3. sanity-check
for f in "$BOOT" "$PART" "$APP"; do
    [[ -f "$f" ]] || {
        echo "Missing $f – build failed?"
        exit 1
    }
done

echo "▶️  Merging binaries ➜ $OUT_BIN"
esptool.py --chip esp32c3 merge_bin \
    --flash_mode dio --flash_freq 80m --flash_size "${FLASH_SZ}" \
    --fill-flash-size "${FLASH_SZ}" \
    -o "${OUT_BIN}" \
    0x0000 "${BOOT}" \
    0x8000 "${PART}" \
    0xe000 "${OTA:-/dev/null}" \
    0xf000 "${BOOT_APP0:-/dev/null}" \
    0x10000 "${APP}"
# (offsets match the default partition table)

# 4. run QEMU – uses QEMU in $PATH or set QEMU=/path/to/qemu-system-riscv32
export QEMU="/home/lukas/Documents/test/espressif-qemu/build/qemu-system-riscv32"
QEMU_BIN="${QEMU:-qemu-system-riscv32}"

exec "${QEMU_BIN}" \
    -nographic \
    -machine esp32c3 \
    -drive file="${OUT_BIN}",if=mtd,format=raw \
    -serial mon:stdio
