# string — Core string functions

Description

Low-level string manipulation helpers: copy, length, compare, find, and concatenation.

Types

- None

Constants

- None

Globals

- None

Functions

- `char* strcpy(char *dst, char *src)` — Copy null-terminated string from `src` to `dst`.
- `int strlen(char *s)` — Return length of string (excluding null terminator).
- `int strcmp(char *s1, char *s2)` — Compare two strings, return negative/zero/positive.
- `char* strcat(char *dst, char *src)` — Append `src` to `dst`.
- `char* strstr(char *haystack, char *needle)` — Return pointer to first occurrence of `needle` or `0`.

Examples

```c
char[32] a;
strcpy(a, "Hello");
strcat(a, " World");
printf("%s\r", a);
```
