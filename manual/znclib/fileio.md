# fileio — File operations

Description

File I/O helpers for opening, reading, writing, seeking, and closing files.

Functions

- `byte fopen(char *filename, byte mode)` — Open a file, return handle or `0xFF` on error.

Types

- None

Constants

- `M_READ` (byte) — 1
- `M_WRITE` (byte) — 2
- `M_EXIST` (byte) — 0
- `M_CREATE` (byte) — 8
- `M_NOEXIST` (byte) — 4
- `M_TRUNC` (byte) — 12
- `M_HDR` (byte) — 64
- `M_SEEK_SET` (byte) — 0
- `M_SEEK_FWD` (byte) — 1
- `M_SEEK_BWD` (byte) — 2

Globals

- `errno` (byte) — last error code set by file operations (0 = OK)

Functions

- `byte fopen(char *filename, byte mode)` — Open a file, return handle or `0xFF` on error.
- `int fread(byte handle, char *buffer, int len)` — Read up to `len` bytes into `buffer`.
- `int fwrite(byte handle, char *buffer, int len)` — Write `len` bytes from `buffer`.
- `int fseek(byte handle, int pos, byte mode)` — Seek in file (set/forward/back).
- `void fclose(byte handle)` — Close file.

Examples

```c
byte h = fopen("data.bin", M_READ);
if (h != 0xFF) {
  char[128] buf;
  int n = fread(h, buf, 128);
  fclose(h);
}
```
