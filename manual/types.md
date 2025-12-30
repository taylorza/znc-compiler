# Data Types

ZNC supports the following base types:

- `char` — character, used for string/byte data
- `byte` — signed 8-bit value (-128..127)
- `int` — signed 16-bit integer (-32768..32767)

You can form pointer types and arrays from these base types.

Notes:
- `int` is 16 bits and `byte` is 8 bits. Pointer arithmetic scales by the pointed-to element size (see Pointers & Arrays).
- `const` can be applied to types to create compile-time constants (see Variables & Constants).
- The compiler supports `char`/`byte` vs `int` distinctions in loads and stores; be mindful of sizes when mixing types.
