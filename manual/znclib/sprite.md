# sprite — Sprite management

Description

Load and control sprite patterns using the NextReg sprite system.

Types

- None

Constants

- None

Globals

- None

Functions

- `void spr_load(byte startid, byte *data, int count)` — Load `count` bytes of sprite data into sprite slot `startid`.
- `void spr_enable()` — Enable sprite rendering.
- `void spr_disable()` — Disable sprite rendering.
- `void spr_hide(byte spriteid)` — Hide sprite with id `spriteid`.
- `void spr_update(byte spriteid, byte patternid, int x, int y, byte flags)` — Update sprite position and pattern.

Examples

```c
spr_load(0, sprite_data, 16*16);
spr_enable();
spr_update(0, 0, 100, 80, 0);
```
