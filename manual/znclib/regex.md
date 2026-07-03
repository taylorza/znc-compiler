# regex — Regular expression matching (match)

Description

Tiny, low-memory regular expression matcher. The library exposes a single public matcher function `match` that evaluates a null-terminated pattern against a text string and returns whether the pattern matches.

Supported features

- `.` any single character (except `\0`)
- `^` match at start of text
- `$` match at end of text
- Quantifiers: `*`, `+`, `?`
- Alternation: `|`
- Character classes: `[...]` and inverted `[^...]` with ranges like `a-z`
- Escapes: `\` to match literal metacharacters (e.g., `\.` to match a dot)
- Shorthand classes: `\d` (digit), `\w` (word/underscore), `\s` (whitespace)
- Inline case modifiers: `(?i)` to enable case-insensitive matching, `(?-i)` to disable it

Types

- None

Constants

- None

Globals

- `_re_icase` — internal case-insensitive flag (not normally used directly).

Functions

- `int match(char *pattern, char *text)` — Return `1` if `pattern` matches `text`, otherwise `0`.

Notes

- An empty pattern matches every string.
- `^` and `$` anchor the pattern to the start and end of the entire text respectively.
- Inline modifiers like `(?i)` affect matching for the remainder of the pattern or until a restoring `(?-i)` is encountered.

Examples

```c
if (match("^[A-Z]xxx", "Bxxx is at the start.")) {
  puts("Matched\r");
}

/* Case-insensitive inline modifier */
if (match("(?i)system_alert", "LOG: SYSTEM_ALERT triggered.")) {
  puts("Matched (case-insensitive)\r");
}
```
