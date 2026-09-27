# printf_tiny — Small formatted output

Description

Small-footprint `printf` implementation for terminal output. It exposes the same public function name as `printf.znc` and is intended as an alternative implementation.

Types

- None

Constants

- None

Globals

- None public.

Functions

- `void printf(char *fmt, ...)` — Format and print values to the terminal.

Examples

```c
printf("Score: %05d\\r", score);
```

Notes

- Include either `printf.znc` or `printf_tiny.znc`, not both.