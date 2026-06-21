# printf — Formatted output

Description

`printf` provides printf-style formatted printing to the terminal. Supports common specifiers like `%d`, `%x`, `%s`, `%c`, and width/padding modifiers.

Types

- None

Constants

- None

Globals

- None

Functions

- `void printf(char *fmt, ...)` — Format and print to terminal.

Examples

```c
printf("Score: %05d\r", score);
printf("Hex: 0x%04X\r", value);
```
