# io — Terminal I/O functions

Description

Cursor positioning, color control, and basic character I/O.

Types

- None

Constants

- None

Globals

- None

Functions

- `void ink(byte c)` — Set foreground color.
- `void paper(byte c)` — Set background color.
- `void gotoxy(byte x, byte y)` — Move cursor to `(x,y)`.
- `void printnum(int n)` — Print a signed integer.
- `char inkey()` — Non-blocking key read (returns `0` if none).
- `char waitkey()` — Blocking key read.

Examples

```c
gotoxy(10,5);
ink(2); paper(0);
puts("Hello at (10,5)\r\n");
```
