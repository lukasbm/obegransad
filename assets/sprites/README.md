# Sprites

Art is authored in LibreSprite (`.ase`, the source files) and exported as
`.bmp`, which `convert_all.sh` turns into the `.bwb` blobs the firmware embeds
(see `main/sprites.hpp`).

## Authoring

* Use the grid feature in LibreSprite; the grid size is the cell size.
* Load `Palette.ase` as the palette. The panel renders four brightness levels
  (`PANEL_BRIGHTNESS_OFF/1/2/3` in `ikea-obegransad-panel.h`). Other gray values
  are clamped to the nearest one when drawn, so stick to those four.
* A sheet is **one cell wide**, with frames stacked vertically. A 9x9,
  four-frame animation therefore exports as 9x36.

## Exporting

1. Export the sprite from LibreSprite as a BMP into this directory.
2. Add a line for it to the table in `convert_all.sh` (source, cell width, cell
   height, output name) and run `./convert_all.sh` from this directory. The cell
   size can't be read from the BMP, so the table is the source of truth; the
   converter fails loudly if the BMP and the size disagree.
3. Commit both the `.ase`/`.bmp` sources and the generated `../../data/*.bwb`.
   The `.bwb` files are checked in so the firmware builds without LibreSprite.
4. Add the `.bwb` to `EMBED_FILES` in `main/CMakeLists.txt` and add a wrapper in
   `main/sprites/<name>.hpp` (the `_binary_*` symbol is named after the file).

`./convert_all.sh --check` verifies the committed `.bwb` files still match the
`.bmp` sources; use it before committing art changes.

## Fonts

Fonts are sheets of 1-character cells, starting at ASCII 32. `BoldGlyphs6x7`
covers 32-125 and `ThinGlyphs4x6` covers 32-96 (no lowercase).
`drawGlyph()` silently draws nothing outside its range.

## Format

`.bwb` is a 2-byte header (cell width, cell height) followed by one byte per
pixel, with no compression or palette, so it can be streamed straight out of
flash.

## Unused

`unused/` holds sources that no scene renders (alternative or demo art). Move a
file back out and add it to the table to bring it into the build.
