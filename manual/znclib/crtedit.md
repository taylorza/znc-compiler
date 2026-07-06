# crtedit — Single-line text editor using the CRT

Description

Simple in-place single-line editor that runs on top of the CRT tilemap console. Edits a NUL-terminated string buffer with optional character whitelist and returns the key used to exit (enter or escape).

Types

- None

Constants

- None (key constants are provided by the `crt` library, e.g. `KEY_ENTER`, `KEY_ESC`).

Globals

- None

Functions

- `char crt_edit(char *text, char *alphabet, int maxlen, int editwidth)` — Edit the string in `text` in-place. `alphabet` may be `0` to allow any printable ASCII (32–127); otherwise it should point to a NUL-terminated list of allowed characters. `maxlen` is the maximum buffer length (space for the terminating NUL); `editwidth` is the visible field width (use `0` to set it equal to `maxlen`). Returns the exit key (`KEY_ENTER` on success, or `KEY_ESC` if cancelled).

Examples

```c
char[32] buf;
crt_gotoxy(10,5);
crt_puts("Enter name: ");
char k = crt_edit(buf, 0, 31, 16);
if (k == KEY_ENTER) {
    crt_gotoxy(10,6);
    crt_puts("Hello ");
    crt_puts(buf);
}
```
