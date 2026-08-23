# tilemap — Tilemap and palette support

Description

Configure and use the ZX Spectrum Next tilemap layer, load tile and map data, select palettes, and control scrolling and clipping.

Types and constants

- `enum TIL_MODE` — `TIL_40x32` and `TIL_80x32` select the tilemap width.
- `enum PALETTE` — identifies the ULA, Layer 2, sprite, or tilemap palette bank.
- `TIL_TILEMAP` — tilemap base address (`0xC000`).
- `TIL_WIDTH`, `TIL_HEIGHT`, `TIL_MAX_TILES`, and `TIL_TILEDEF` — dimensions and tile-definition storage selected by `til_init`.

Functions

- `void til_init(TIL_MODE mode)` — Configure the tilemap layer and select 40x32 or 80x32 mode.
- `void til_default()` — Reset the tilemap clip window and scroll offsets.
- `void til_restore()` — Restore the Next registers saved by `til_init`.
- `void til_cls(byte tile)` — Fill the visible map with one tile index.
- `void til_load(char *filename, byte count)` — Load `count` 32-byte tiles.
- `void til_loadmap(char *filename)` — Load a map for the selected width.
- `void til_set(byte x, byte y, byte tile)` — Set a tile if the coordinates are in range.
- `byte til_get(byte x, byte y)` — Read a tile, returning zero for out-of-range coordinates.
- `void til_at(uint x, uint y)` — Set the scroll position.
- `void til_clip(uint x1, uint y1, uint x2, uint y2)` — Set the clip window.
- `void pal_load(PALETTE palette, char *filename)` — Load a 512-byte palette into the selected palette bank.
- `void pal_select(PALETTE palette)` — Select the active ULA, Layer 2, sprite, or tilemap palette.

Example

```c
include "tilemap.znc";

til_init(TIL_MODE.TIL_40x32);
til_load("tiles.bin", 16);
til_loadmap("map.bin");
til_at(0, 0);
```
