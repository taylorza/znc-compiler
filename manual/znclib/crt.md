# crt — Tilemap text console (CRT)

Description

Tilemap-based text console built on the ZX Next tilemap/tiledef hardware. Provides an 80×32 text mode with per-character attributes, basic input handling and palette setup.

Types

- None

Constants

- `const char KEY_TRUEVID` — Enable true video mode key code.
- `const char KEY_INVVID` — Invert video key code.
- `const char KEY_CAPS` — Caps-lock toggle key code.
- `const char KEY_EDIT` — Edit key code.
- `const char KEY_LEFT` — Left arrow key code.
- `const char KEY_RIGHT` — Right arrow key code.
- `const char KEY_DOWN` — Down arrow key code.
- `const char KEY_UP` — Up arrow key code.
- `const char KEY_DELETE` — Delete/backspace key code.
- `const char KEY_ENTER` — Enter key code.
- `const char KEY_GRAPH` — Graph/graphics key code.
- `const byte CRT_WIDTH` — Character width (80).
- `const byte CRT_WIDTH2` — Width*2 (byte offset helper).
- `const byte CRT_HEIGHT` — Character height (32).
- `const int CRT_TILEMAP` — Tilemap base address.
- `const int CRT_TILEDEF` — Tile definition base address.
- `const byte CRT_TILE_OFFS` — Tile definition offset for nextreg.
- `const byte CRT_FLASHRATE` — Cursor/attribute flash rate.

Globals

- None

Functions

- `void crt_init()` — Initialize CRT mode, install font and palette.
- `void crt_restore()` — Restore saved registers and clear CRT state.
- `void crt_setpal(byte *pal)` — Set palette; pass `0` to use the built-in palette.
- `void crt_cls()` — Clear the screen and reset cursor to (0,0).
- `void crt_gotoxy(byte x, byte y)` — Move cursor to `(x,y)`.
- `byte crt_wherex()` — Return current cursor X.
- `byte crt_wherey()` — Return current cursor Y.
- `void crt_scroll(char lines)` — Scroll the screen by `lines` (positive scrolls up, negative scrolls down).
- `void crt_setbg(byte color)` — Set background color for subsequent writes.
- `void crt_setfg(byte color)` — Set foreground color for subsequent writes.
- `void crt_putc(byte ch)` — Output character `ch` at cursor (handles CR and wrapping).
- `int crt_puts(char *s)` — Output NUL-terminated string `s`, returns number of characters written.
- `byte crt_getattr(byte x, byte y)` — Read attribute byte at position `(x,y)`.
- `void crt_setattr(byte x, byte y, byte attr)` — Set attribute byte at position `(x,y)`.
- `char crt_inkey()` — Non-blocking key read (returns `0` if none).
- `char crt_getch()` — Blocking key read with cursor flashing; returns key code (e.g. `KEY_ENTER`, `KEY_ESC`).
- `void crt_cursor(byte nocaps_color, byte caps_color)` — Set the cursor colors used for normal and caps-lock modes; pass `255` to use the computed default attribute.

Examples

```c
crt_init();
crt_setbg(0); crt_setfg(7);
crt_cls();
crt_gotoxy(1,1);
crt_puts("Hello CRT world!");
crt_getch();
crt_restore();
```
