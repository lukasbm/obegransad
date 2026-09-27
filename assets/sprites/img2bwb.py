#!/usr/bin/env python3
"""Convert sprite art to the .bwb format used by the firmware.

The input is either a LibreSprite ``.ase`` file (preferred; read by the
:mod:`aseprite` module) or a flattened ``.bmp``/``.png`` (fallback, read with
Pillow). The output is written to stdout:

    <cell width:1> <cell height:1> <one gray byte per pixel>

A sheet is one cell wide with frames stacked vertically, so a multi-frame
``.ase`` has its composited frames concatenated top to bottom. The ``.ase``
frame size is used as the cell size unless one is given; fonts and flattened
bitmaps must pass one.

Usage:
    python3 img2bwb.py INPUT CELL_WIDTH CELL_HEIGHT > OUTPUT.bwb
    python3 img2bwb.py INPUT.ase > OUTPUT.bwb
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

import aseprite

_ASE_SUFFIXES = {".ase", ".aseprite"}
_IMAGE_SUFFIXES = {".bmp", ".png"}


def _rgba_to_gray(rgba: bytes) -> bytes:
    """Luminance of each RGBA pixel, composited onto black via its alpha."""
    out = bytearray(len(rgba) // 4)
    for i in range(len(out)):
        r = rgba[i * 4]
        g = rgba[i * 4 + 1]
        b = rgba[i * 4 + 2]
        a = rgba[i * 4 + 3]
        out[i] = ((r * 299 + g * 587 + b * 114) // 1000) * a // 255
    return bytes(out)


def read_sheet(source: Path):
    """Return ``(pixels, width, height, default_cell)`` for ``source``.

    ``pixels`` is row-major 8-bit gray, ``default_cell`` the cell size the
    source implies (the .ase frame size) or ``None`` if it has to be given.
    """
    suffix = source.suffix.lower()
    if suffix in _ASE_SUFFIXES:
        doc = aseprite.read(source)
        pixels = b"".join(_rgba_to_gray(doc.frame_rgba(i)) for i in range(doc.num_frames))
        height = doc.height * doc.num_frames
        return pixels, doc.width, height, (doc.width, doc.height)
    if suffix in _IMAGE_SUFFIXES:
        from PIL import Image  # imported lazily; only the fallback needs Pillow

        with Image.open(source) as image:
            gray = image.convert("L")
            return gray.tobytes(), gray.width, gray.height, None
    raise ValueError(f"unsupported source '{source}': expected .ase, .bmp or .png")


def encode(source, cell_width: int | None = None, cell_height: int | None = None) -> bytes:
    """Return the complete .bwb blob for ``source``."""
    pixels, width, height, default_cell = read_sheet(Path(source))
    if cell_width is None or cell_height is None:
        if default_cell is None:
            raise ValueError(f"{source}: cell size is required for this source")
        cell_width, cell_height = default_cell

    if not 1 <= cell_width <= 255:
        raise ValueError(f"{source}: cell width must be 1..255, got {cell_width}")
    if not 1 <= cell_height <= 255:
        raise ValueError(f"{source}: cell height must be 1..255, got {cell_height}")
    if width != cell_width:
        raise ValueError(
            f"{source}: sheet is {width}px wide but the cell width is {cell_width}; "
            "a sheet must be exactly one cell wide"
        )
    if height % cell_height != 0:
        raise ValueError(
            f"{source}: sheet is {height}px tall, not a whole number of {cell_height}px cells"
        )

    return bytes([cell_width, cell_height]) + pixels


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("input_file", help=".ase (preferred), .bmp or .png")
    parser.add_argument("cell_width", type=int, nargs="?", default=None)
    parser.add_argument("cell_height", type=int, nargs="?", default=None)
    args = parser.parse_args(argv)

    try:
        blob = encode(args.input_file, args.cell_width, args.cell_height)
    except ValueError as exc:
        sys.exit(f"error: {exc}")

    sys.stdout.buffer.write(blob)
    sys.stdout.buffer.flush()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
