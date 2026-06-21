# version — System/core version queries

Description

Helpers to query EXDOS and Next core version numbers.

Types

- None

Constants

- None

Globals

- None

Functions

- `int exdosVersion()` — Return EXDOS version in BCD format.
- `int coreVersion()` — Return Next core version in BCD format.

Examples

```c
int v = coreVersion();
printf("Next core version: %04X\r", v);
```
