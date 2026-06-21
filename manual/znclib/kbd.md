# kbd — Keyboard input utilities

Description

Keyboard scanning and simple key state helpers.

Types

- None

Constants

- None

Globals

- None

Functions

- `void updatekeys()` — Scan keyboard and update internal state.
- `byte is_pressed(char c)` — Check if key `c` is currently pressed.
- `byte getch()` — Pop the next key from the buffer (0 if none).

Examples

```c
updatekeys();
if (is_pressed('A')) puts("A pressed\r");
```
