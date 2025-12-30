# Expressions & Operators

ZNC provides a C-like expression language:

- Arithmetic: `+`, `-`, `*`, `/`, `%`
- Bitwise: `&`, `|`, `^`, `~`
- Shift: `<<`, `>>`
- Comparison: `==`, `!=`, `<`, `<=`, `>`, `>=`
- Logical: `&&`, `||`, `!`
- Ternary: `cond ? a : b`
- Pre/post increment and decrement: `++x`, `x++`, `--x`, `x--` (supported for scalars and indexed lvalues)

Operator precedence follows traditional C-like rules.

Notes
- The compiler supports prefix and postfix `++/--` for identifiers and array/indexed elements.
- When using pointer types, arithmetic scales by element size.
- String literals adjacent in source are concatenated at compile time (e.g. "Hi" "!" -> "Hi!").
