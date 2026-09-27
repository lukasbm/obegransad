#!/usr/bin/env python3
"""Convert a sprite sheet exported from LibreSprite to the .bwb format.

.bwb ("image black and white bitmap") is a 2-byte header (cell width, cell
height) followed by one byte per pixel, no compression and no palette. A sheet
is exactly one cell wide and stacked vertically, one cell per frame (see
main/sprites.hpp), so the header's width is the image width and the header's
height divides the image height.

Usage:
    python3 img2bwb.py INPUT.bmp SPRITE_WIDTH SPRITE_HEIGHT > OUTPUT.bwb
"""

import argparse
import sys

import numpy as np
from PIL import Image


def load_image(input_file: str) -> np.ndarray:
    with Image.open(input_file) as img:
        return np.array(img.convert("L"), dtype=np.uint8)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Convert a LibreSprite BMP export to .bwb (stdout)."
    )
    parser.add_argument("input_file", help="8-bit BMP exported from LibreSprite")
    parser.add_argument("sprite_width", type=int, help="cell width in pixels")
    parser.add_argument("sprite_height", type=int, help="cell height in pixels")
    args = parser.parse_args(argv)

    if not 1 <= args.sprite_width <= 255:
        parser.error(f"sprite width must be 1..255, got {args.sprite_width}")
    if not 1 <= args.sprite_height <= 255:
        parser.error(f"sprite height must be 1..255, got {args.sprite_height}")

    img = load_image(args.input_file)
    height, width = img.shape

    # Catch a mismatch between the art and the size given here. Without this a
    # wrong size silently produces a valid-looking .bwb that the firmware then
    # reads as garbage.
    if width != args.sprite_width:
        sys.exit(
            f"error: {args.input_file} is {width}px wide but the sprite width "
            f"is {args.sprite_width}; a sheet must be exactly one cell wide"
        )
    if height % args.sprite_height != 0:
        sys.exit(
            f"error: {args.input_file} is {height}px tall, not a whole number "
            f"of {args.sprite_height}px cells"
        )

    out = sys.stdout.buffer
    out.write(bytes([args.sprite_width, args.sprite_height]))
    out.write(img.tobytes())
    out.flush()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
