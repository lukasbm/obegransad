#!/usr/bin/env bash
#
# Regenerate the embedded sprite data (data/*.bwb) from the LibreSprite
# exports (assets/sprites/*.bmp). Run from this directory:
#
#   ./convert_all.sh          rewrite ../../data/*.bwb
#   ./convert_all.sh --check  fail if the committed data is stale
#
# The cell size cannot be derived from the .bmp: a multi-frame sheet is one
# cell wide with the frames stacked vertically, so its height is
# frames * cell_height. Keep the table below in sync when adding a sprite.
set -euo pipefail

cd "$(dirname "$0")"

OUT_DIR="../../data"
EMBED_LIST="../../main/CMakeLists.txt"

# <source.bmp>  <cell_w> <cell_h>  <output.bwb>
entries=(
  "ThinGlyphs4x6.bmp  4  6 thin_glyphs.bwb"
  "BoldGlyphs6x7.bmp  6  7 bold_glyphs.bwb"
  "Sun.bmp            9  9 sun.bwb"
  "Rain.bmp           7  8 rain.bwb"
  "Cloud.bmp          9  5 cloud.bwb"
  "MoonSmall.bmp      9  9 moon_small.bwb"
  "WiFi.bmp          10  8 wifi.bwb"
  "Heart.bmp          9  8 heart.bwb"
  "Firework.bmp       9  9 firework.bwb"
)

check=0
if [ "${1:-}" = "--check" ]; then
  check=1
fi

mkdir -p "$OUT_DIR"
status=0
declare -A listed

for entry in "${entries[@]}"; do
  read -r src w h out <<< "$entry"
  listed[$src]=1
  if [ ! -f "$src" ]; then
    echo "error: source $src is listed but missing" >&2
    status=1
    continue
  fi

  tmp="$(mktemp)"
  if ! python3 img2bwb.py "$src" "$w" "$h" > "$tmp"; then
    rm -f "$tmp"
    status=1
    continue
  fi

  if [ "$check" = 1 ]; then
    if ! cmp -s "$tmp" "$OUT_DIR/$out"; then
      echo "stale: $OUT_DIR/$out does not match $src" >&2
      status=1
    fi
    rm -f "$tmp"
  else
    mv "$tmp" "$OUT_DIR/$out"
    echo "$src ($w x $h) -> $OUT_DIR/$out"
  fi
done

# A .bmp that is not in the table is either new (add it above) or stray.
for bmp in *.bmp; do
  [ -e "$bmp" ] || continue
  if [ -z "${listed[$bmp]:-}" ]; then
    echo "warning: $bmp is not listed in convert_all.sh" >&2
  fi
done

# Every generated .bwb must be embedded, or linking fails later with a
# cryptic '_binary_...' error.
for out in "$OUT_DIR"/*.bwb; do
  name="$(basename "$out")"
  if ! grep -q "data/$name" "$EMBED_LIST"; then
    echo "warning: $out is not in EMBED_FILES ($EMBED_LIST)" >&2
  fi
done

if [ "$check" = 1 ] && [ "$status" = 0 ]; then
  echo "committed .bwb data matches the .bmp sources"
fi

exit "$status"
