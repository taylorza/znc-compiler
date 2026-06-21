# intToMonth — Month number to text conversion

Description

Convert month numbers to full or short month names.

Types

- None

Constants

- None

Globals

- None

Functions

- `char* intToMonth(int month)` — Return full month name for `month` (1..12).
- `char* intToShMonth(int month)` — Return 3-letter month abbreviation.

Examples

```c
printf("Month: %s\r", intToMonth(3)); // "March"
printf("Short: %s\r", intToShMonth(12)); // "Dec"
```
