# Expressions & Operators

ZNC provides a C-like expression language:

- Arithmetic: `+`, `-`, `*`, `/`, `%`
- Bitwise: `&`, `|`, `^`, `~`
- Shift: `<<`, `>>`
- Comparison: `==`, `!=`, `<`, `<=`, `>`, `>=`
- Logical: `&&`, `||`, `!`
- Ternary: `cond ? a : b`
- Pre/post increment and decrement: `++x`, `x++`, `--x`, `x--` (supported for scalars and indexed lvalues)
- Member access: `obj.field` (supports nested structs; `obj` can be a struct variable or a pointer to struct)
- Address-of and dereference: `&name` yields the address; `*ptr` dereferences a pointer

Fixed-point arithmetic

- Expressions mixing `fixed` with `int`/`char` automatically convert the integer operand to Q4 format before the operation. The result type is `fixed`.
- Multiplication and division of two `fixed` values use runtime helpers (`ccfxmul`, `ccfxdiv`) that preserve the Q4 scale.
- Increment/decrement on a `fixed` variable steps by **1.0** (0x10 in Q4).
- Constant `fixed` expressions are folded at compile time.

Operator precedence follows traditional C-like rules.

Notes
- The compiler supports prefix and postfix `++/--` for identifiers and array/indexed elements.
- Compound assignment operators (`+=`, `-=`, `*=`, `/=`, `%=`, `<<=`, `>>=`, `&=`, `|=`, `^=`) are not supported. Use expanded forms (e.g., `x = x + y`).
- When using pointer types, arithmetic scales by element size.
- String literals adjacent in source are concatenated at compile time (e.g. `"Hi" "!"` → `"Hi!"`).
- Struct variables evaluate to their address when used in expressions; member access computes the correct byte offset automatically.
- Assignment uses `=`.
