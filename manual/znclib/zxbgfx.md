# zxbgfx — ZX BASIC graphics wrappers

Description

Wrapper helpers for ZX BASIC ROM graphics functions such as `PLOT`, `POINT`, and `DRAW`.

Types

- None

Constants

- None

Globals

- None

Functions

- `void zxb_plot(int x, int y)` — Plot pixel at `(x,y)`.
- `int zxb_point(int x, int y)` — Return pixel color at `(x,y)`.
- `void zxb_draw(int x, int y)` — Draw a line to `(x,y)`.

Examples

```c
zxb_plot(10, 10);
if (zxb_point(10,10)) puts("Pixel set\r");
```
