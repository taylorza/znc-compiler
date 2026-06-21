# memory — Block memory operations

Description

Simple `memcpy` and `memset` helpers for copying and filling memory blocks.

Types

- None

Constants

- None

Globals

- None

Functions

- `void memcpy(void *dst, void *src, int len)` — Copy `len` bytes from `src` to `dst`.
- `void memset(void *dst, char val, int len)` — Fill `len` bytes at `dst` with `val`.

Examples

```c
char[64] buf;
memset(buf, 0, 64);
memcpy(buf, src, 16);
```
