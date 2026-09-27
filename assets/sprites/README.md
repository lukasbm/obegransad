# Sprites

Art is authored in LibreSprite (`.ase`, the source of truth) and turned into
the blobs and C++ wrappers the firmware embeds by `convert_all.sh`.

## Authoring

* Use the grid feature; the grid size is the cell size.
* Load `Palette.ase`. The panel renders four brightness levels
  (`PANEL_BRIGHTNESS_OFF/1/2/3` in `ikea-obegransad-panel.h`); other gray values
  are clamped to the nearest one when drawn, so stick to those four.
* A sheet is **one cell wide**, with animation frames stacked vertically. A 9x9
  four-frame animation is a 9x9 canvas with four frames, not a 9x36 canvas; the
  tooling stacks the frames for you.

## Adding or changing a sprite

1. Draw it in LibreSprite and save the `.ase` here.
2. Add or edit its entry in `sprites.toml`.
3. Run `./convert_all.sh` (from this directory). It writes, per entry:
   * `data/<name>.bwb` -- the embedded blob,
   * `main/sprites/<name>.hpp` -- externs, wrapper struct and global,
   * `main/sprites/sprites.cmake` -- the `EMBED_FILES` list.
4. Commit the `.ase`, `sprites.toml` and the generated files.

`./convert_all.sh --check` verifies the committed generated files match the
sources; use it before committing.

## Pieces

* `aseprite.py` -- standalone, standard-library-only reader for `.ase`, imported
  by the converter. Handles the RGBA / grayscale / indexed color modes, linked
  and zlib-compressed cels, and composites layers.
* `img2bwb.py` -- converts one file to `.bwb`. Prefers `.ase`; also accepts a
  flattened `.bmp`/`.png` (needs Pillow) as a fallback.
* `generate.py` -- reads `sprites.toml` and writes all generated files.
* `sprites.toml` -- the manifest: for each sprite its source, kind, C++ type and
  optional instance/cell.
* `convert_all.sh` -- thin wrapper: `python3 generate.py "$@"`.

## Fonts

`BoldGlyphs6x7` covers ASCII 32-125 and `ThinGlyphs4x6` covers 32-96 (no
lowercase); `drawGlyph()` silently draws nothing outside the range. Font sheets
are the one case that needs an explicit `cell` in the manifest, because there
the canvas is the whole glyph strip rather than a single frame.

## Format

`.bwb` is a 2-byte header (cell width, cell height) followed by one byte per
pixel, with no compression or palette.

## Unused / legacy

* `unused/` holds sources no scene renders (alternative or demo art).
* The `.bmp` exports are no longer used by the build; they are kept only as a
  flattened reference and as input to the fallback converter.
