# sprintf — Format into buffer

Description

Format data into a memory buffer using printf-style format specifiers.

Types

- None

Constants

- None

Globals

- None

Functions

- `void sprintf(char* dst, char *fmt, ...)` — Write formatted text to `dst`.

Examples

```c
char[64] buf;
sprintf(buf, "Score: %d", score);
puts(buf);
```
