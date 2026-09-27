"""Standalone reader for Aseprite / LibreSprite ``.ase`` sprite files.

Only what is needed to get pixels back out is implemented: the header, the
palette, layer names, and cel chunks (raw, linked and zlib-compressed images).
Tilemaps, external files and color profiles are ignored.

Use :func:`read` to load a file into an :class:`Aseprite`, whose frames are
composited on demand with :meth:`Aseprite.frame_rgba`. Rendering is done here
so callers never have to know about layers, cels or the source color depth.

Pure standard library (``struct`` + ``zlib``) so the asset tools can import it
without pulling in Pillow or numpy.
"""

from __future__ import annotations

import struct
import zlib
from dataclasses import dataclass, field
from pathlib import Path

__all__ = ["AsepriteError", "Aseprite", "read", "parse"]

HEADER_MAGIC = 0xA5E0
FRAME_MAGIC = 0xF1FA
HEADER_SIZE = 128

_CHUNK_LAYER = 0x2004
_CHUNK_CEL = 0x2005
_CHUNK_PALETTE = 0x2019

_CEL_RAW = 0
_CEL_LINKED = 1
_CEL_COMPRESSED = 2
_CEL_COMPRESSED_TILEMAP = 3

_DEPTH_BYTES = {8: 1, 16: 2, 24: 3, 32: 4}


class AsepriteError(Exception):
    """Raised when a file is not a readable .ase image."""


@dataclass
class Cel:
    layer: int
    x: int
    y: int
    opacity: int
    kind: int
    width: int = 0
    height: int = 0
    rgba: bytes | None = None
    linked_frame: int | None = None


@dataclass
class Frame:
    duration_ms: int
    cels: list[Cel] = field(default_factory=list)


class Aseprite:
    """A parsed .ase file. Pixels are exposed as composited RGBA frames."""

    def __init__(self, width, height, color_depth, transparent_index):
        self.width = width
        self.height = height
        self.color_depth = color_depth
        self.transparent_index = transparent_index
        self.layers: list[str] = []
        self.frames: list[Frame] = []
        self.palette: list[tuple[int, int, int, int]] = [(0, 0, 0, 0)] * 256

    @property
    def num_frames(self) -> int:
        return len(self.frames)

    def frame_rgba(self, index: int) -> bytes:
        """Return frame ``index`` composited onto a transparent canvas as
        ``width * height * 4`` bytes of RGBA, in row-major order."""
        if not 0 <= index < len(self.frames):
            raise IndexError(f"frame {index} out of range (0..{self.num_frames - 1})")
        canvas = bytearray(self.width * self.height * 4)
        for cel in self.frames[index].cels:
            rgba, w, h = self._resolve(index, cel)
            if rgba is not None:
                _blend(canvas, self.width, self.height, rgba, w, h, cel.x, cel.y)
        return bytes(canvas)

    def _resolve(self, frame_index: int, cel: Cel):
        if cel.kind == _CEL_LINKED:
            src = self.frames[cel.linked_frame]
            for candidate in src.cels:
                if candidate.layer == cel.layer:
                    return self._resolve(cel.linked_frame, candidate)
            raise AsepriteError(f"linked cel points at frame {cel.linked_frame} with no layer {cel.layer}")
        return cel.rgba, cel.width, cel.height


def _blend(canvas, canvas_w, canvas_h, src, src_w, src_h, ox, oy):
    """Source-over ``src`` (RGBA) onto ``canvas`` (RGBA) at ``(ox, oy)``, clipped."""
    for cy in range(src_h):
        ty = oy + cy
        if ty < 0 or ty >= canvas_h:
            continue
        srow = cy * src_w * 4
        trow = (ty * canvas_w) * 4
        for cx in range(src_w):
            tx = ox + cx
            if tx < 0 or tx >= canvas_w:
                continue
            si = srow + cx * 4
            sa = src[si + 3]
            if sa == 0:
                continue
            di = trow + tx * 4
            if sa == 255:
                canvas[di : di + 4] = src[si : si + 4]
                continue
            inv = 255 - sa
            canvas[di] = (src[si] * sa + canvas[di] * inv) // 255
            canvas[di + 1] = (src[si + 1] * sa + canvas[di + 1] * inv) // 255
            canvas[di + 2] = (src[si + 2] * sa + canvas[di + 2] * inv) // 255
            canvas[di + 3] = sa + (canvas[di + 3] * inv) // 255


def _decode_cel(doc: Aseprite, cel: Cel, raw: bytes) -> bytes:
    expected = cel.width * cel.height * _DEPTH_BYTES[doc.color_depth]
    if cel.kind == _CEL_COMPRESSED:
        try:
            raw = zlib.decompress(raw)
        except zlib.error as exc:
            raise AsepriteError(f"cel data is not valid zlib: {exc}") from exc
    if len(raw) != expected:
        raise AsepriteError(f"cel has {len(raw)} bytes, expected {expected}")

    depth = doc.color_depth
    pixels = cel.width * cel.height
    out = bytearray(pixels * 4)
    if depth == 32:
        out[:] = raw
    elif depth == 24:
        for i in range(pixels):
            out[i * 4 : i * 4 + 3] = raw[i * 3 : i * 3 + 3]
            out[i * 4 + 3] = 255
    elif depth == 16:
        for i in range(pixels):
            gray = raw[i * 2]
            out[i * 4] = out[i * 4 + 1] = out[i * 4 + 2] = gray
            out[i * 4 + 3] = raw[i * 2 + 1]
    else:  # depth == 8, indexed
        for i in range(pixels):
            index = raw[i]
            if index == doc.transparent_index:
                continue  # stays fully transparent
            out[i * 4 : i * 4 + 4] = bytes(doc.palette[index])
    return bytes(out)


def _parse_chunk(doc: Aseprite, frame: Frame, data: bytes, off: int, csize: int, ctype: int):
    body = off + 6
    if ctype == _CHUNK_LAYER:
        name_len = struct.unpack_from("<H", data, body + 16)[0]
        doc.layers.append(data[body + 18 : body + 18 + name_len].decode("utf-8", "replace"))
    elif ctype == _CHUNK_CEL:
        cel = Cel(
            layer=struct.unpack_from("<H", data, body)[0],
            x=struct.unpack_from("<h", data, body + 2)[0],
            y=struct.unpack_from("<h", data, body + 4)[0],
            opacity=data[body + 6],
            kind=struct.unpack_from("<H", data, body + 7)[0],
        )
        if cel.kind == _CEL_LINKED:
            cel.linked_frame = struct.unpack_from("<H", data, body + 16)[0]
        elif cel.kind in (_CEL_RAW, _CEL_COMPRESSED):
            cel.width = struct.unpack_from("<H", data, body + 16)[0]
            cel.height = struct.unpack_from("<H", data, body + 18)[0]
            cel.rgba = _decode_cel(doc, cel, data[body + 20 : off + csize])
        elif cel.kind == _CEL_COMPRESSED_TILEMAP:
            raise AsepriteError("tilemap cels are not supported")
        else:
            raise AsepriteError(f"unknown cel type {cel.kind}")
        frame.cels.append(cel)
    elif ctype == _CHUNK_PALETTE:
        count = struct.unpack_from("<I", data, body)[0]
        first = struct.unpack_from("<I", data, body + 4)[0]
        poff = body + 20
        for i in range(count):
            r, g, b, a = data[poff + 2 : poff + 6]
            if 0 <= first + i < len(doc.palette):
                doc.palette[first + i] = (r, g, b, a)
            poff += 6


def parse(data: bytes) -> Aseprite:
    """Parse the bytes of a .ase file."""
    if len(data) < HEADER_SIZE:
        raise AsepriteError("file is smaller than the 128-byte header")
    size, magic, num_frames, width, height, depth = struct.unpack_from("<IHHHHH", data, 0)
    if magic != HEADER_MAGIC:
        raise AsepriteError(f"bad magic {magic:#06x}, not an Aseprite file")
    if size != len(data):
        raise AsepriteError(f"header says the file is {size} bytes, but it is {len(data)}")
    if depth not in _DEPTH_BYTES:
        raise AsepriteError(f"unsupported color depth {depth}")
    if num_frames == 0 or width == 0 or height == 0:
        raise AsepriteError("image has no frames or zero size")

    doc = Aseprite(width, height, depth, transparent_index=data[28])
    off = HEADER_SIZE
    for fi in range(num_frames):
        if off + 16 > len(data):
            raise AsepriteError(f"frame {fi} header runs past the end of the file")
        frame_bytes, fmagic, old_count, duration, new_count = struct.unpack_from(
            "<IHHH2xI", data, off
        )
        if fmagic != FRAME_MAGIC:
            raise AsepriteError(f"frame {fi}: bad magic {fmagic:#06x}")
        frame_end = off + frame_bytes
        count = new_count or old_count
        frame = Frame(duration_ms=duration)
        coff = off + 16
        for ci in range(count):
            if coff + 6 > frame_end:
                raise AsepriteError(f"frame {fi}: chunk {ci} header out of bounds")
            csize, ctype = struct.unpack_from("<IH", data, coff)
            if csize < 6 or coff + csize > len(data):
                raise AsepriteError(f"frame {fi}: chunk {ci} has a bad size {csize}")
            _parse_chunk(doc, frame, data, coff, csize, ctype)
            coff += csize
        if coff != frame_end:
            raise AsepriteError(f"frame {fi}: chunks end at {coff}, header said {frame_end}")
        doc.frames.append(frame)
        off = frame_end
    return doc


def read(path) -> Aseprite:
    """Read a .ase file from ``path`` (str or :class:`pathlib.Path`)."""
    return parse(Path(path).read_bytes())
