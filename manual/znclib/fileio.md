# fileio — File operations

Description

File I/O helpers for opening, reading, writing, seeking, and closing files.

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

- `byte fopen(char *filename, byte mode)` — Open a file, return handle or `0` on error.
- `byte fgethandle()` — Return the current file handle used by the program.
- `int fread(byte handle, void *buffer, int len)` — Read up to `len` bytes into `buffer`.
- `int fwrite(byte handle, void *buffer, int len)` — Write `len` bytes from `buffer`.
- `int fseek(byte handle, int pos, byte mode)` — Seek in file (set/forward/back).
- `int funlink(char *filename)` — Delete a file; return `0` on success or an error code on failure.
- `void fclose(byte handle)` — Close file.

Examples

```c
byte h = fopen("data.bin", M_READ);
if (h != 0) {
  char[128] buf;
  int n = fread(h, buf, 128);
  fclose(h);
}
```
