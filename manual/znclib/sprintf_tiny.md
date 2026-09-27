# sprintf_tiny — Small string formatting

Description

Small-footprint `sprintf` implementation for writing formatted text into a buffer. It is an alternative implementation of `sprintf.znc`.

Types

- None

Constants

- None

Globals

- None public.

Functions

- `void sprintf(char *dst, char *fmt, ...)` — Format values and write a terminating NUL-terminated string to `dst`.

Examples

```c
char[32] text;
sprintf(text, "Score: %d", score);
```

Notes

- Include either `sprintf.znc` or `sprintf_tiny.znc`, not both.