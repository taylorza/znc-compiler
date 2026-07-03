# ctype — Character classification and conversion

Description

Common character classification helpers and simple case conversion routines used across the standard libraries.

Types

- None

Constants

- None

Globals

- None

Functions

- `byte isdigit(char c)` — Return `1` if `c` is '0'..'9', otherwise `0`.
- `byte isalpha(char c)` — Return `1` if `c` is a letter (A-Z or a-z).
- `byte isalnum(char c)` — Return `1` if `c` is a letter (A-Z or a-z) or a digit.
- `byte islanum(char c)` — Return `1` if `c` is a lowercase letter (a-z) or a digit.
- `byte isupper(char c)` — Return `1` if `c` is an uppercase letter (A-Z).
- `byte islower(char c)` — Return `1` if `c` is a lowercase letter (a-z).
- `byte isxdigit(char c)` — Return `1` if `c` is a hexadecimal digit (0-9, A-F, a-f).
- `byte isspace(char c)` — Return `1` for whitespace characters: space, tab, newline, vertical tab, formfeed, carriage return.
- `byte isprint(char c)` — Return `1` for printable ASCII characters (32..126).
- `byte ispunct(char c)` — Return `1` for printable punctuation characters (printable and not alphanumeric).
- `char to_lower(char c)` — Convert an uppercase ASCII letter to lowercase; other characters are returned unchanged.
- `char to_upper(char c)` — Convert a lowercase ASCII letter to uppercase; other characters are returned unchanged.

Examples

```c
char s[] = "Hello, WORLD! 123\n";
for (int i = 0; s[i] != '\0'; ++i) {
  char c = s[i];
  if (isupper(c)) {
    putc(to_lower(c));
  } else {
    putc(c);
  }
}
```
