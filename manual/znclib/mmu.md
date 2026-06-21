# mmu — Banked memory allocation

Description

Simple helpers to allocate and free banked memory pages.

Types

- None

Constants

- None

Globals

- None

Functions

- `byte mmualloc()` — Allocate a banked page, return page number or `0xFF` on failure.
- `void mmufree(byte page)` — Free a previously allocated page.

Examples

```c
byte page = mmualloc();
if (page != 0xFF) {
  // use banked memory
  mmufree(page);
}
```
