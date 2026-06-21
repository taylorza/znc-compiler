# mouse — Mouse position and buttons

Description

Helpers to read mouse X/Y coordinates and button/wheel state.

Types

- None

Constants

- None

Globals

- None

Functions

- `byte readMouseX()` — Return X coordinate.
- `byte readMouseY()` — Return Y coordinate.
- `byte readMouseB()` — Return buttons bitmask.
- `byte readMouseW()` — Return wheel state.

Examples

```c
byte x = readMouseX();
byte y = readMouseY();
if (readMouseB() & 1) puts("Left click\r");
```
