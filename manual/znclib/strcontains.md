# strcontains — Substring test

Description

Return whether a search string exists inside a haystack string.

Types

- None

Constants

- None

Globals

- None

Functions

- `byte strcontains(char *haystack, char *search)` — Return `1` if found, otherwise `0`.

Examples

```c
if (strcontains("Hello world", "world")) {
  puts("Found\r");
}
```
