# Expressions & Operators

ZNC provides a C-like expression language:

- Arithmetic: `+`, `-`, `*`, `/`, `%`
- Bitwise: `&`, `|`, `^`, `~`
- Shift: `<<`, `>>`
- Comparison: `==`, `!=`, `<`, `<=`, `>`, `>=`
- Logical: `&&`, `||`, `!`
- Ternary: `cond ? a : b`
- Pre/post increment and decrement: `++x`, `x++`, `--x`, `x--` (supported for scalars and indexed lvalues)
- Compound assignment: `+=`, `-=`, `*=`, `/=`, `%=`, `<<=`, `>>=`, `&=`, `|=`, `^=`
- Member access: `obj.field` (supports nested structs, unions, and anonymous aggregate fields; `obj` can be an aggregate variable or a pointer to aggregate)
- Enum member reference: `TypeName.Member` (for example, `Color.GREEN`)
- Address-of and dereference: `&name` yields the address; `*ptr` dereferences a pointer

Fixed-point arithmetic

- Expressions mixing `fixed` with `int`/`char` automatically convert the integer operand to Q4 format before the operation. The result type is `fixed`.
- Multiplication and division of two `fixed` values use runtime helpers (`ccfxmul`, `ccfxdiv`) that preserve the Q4 scale.
- Increment/decrement on a `fixed` variable steps by **1.0** (0x10 in Q4).
- Constant `fixed` expressions are folded at compile time.
- Folded and runtime Q4 multiplication truncate toward zero; Q4 division rounds to the nearest representable value, with ties away from zero.

Operator precedence follows traditional C-like rules.

Notes
- The compiler supports prefix and postfix `++/--` for identifiers and array/indexed elements.
- Compound assignment operators (`+=`, `-=`, `*=`, `/=`, `%=`, `<<=`, `>>=`, `&=`, `|=`, `^=`) are fully supported.
- When using pointer types, arithmetic scales by element size.
- String literals adjacent in source are concatenated at compile time (e.g. `"Hi" "!"` → `"Hi!"`).
- Struct variables evaluate to their address when used in expressions; member access computes the correct byte offset automatically.
- Anonymous struct and union fields are promoted, so their members are accessed directly through the containing value (for example, `value.left` rather than `value.anonymous.left`).
- Enum members evaluate as compile-time constants and can be used in constant-expression contexts.
- Assignment uses `=`.
- Assignment expressions yield the assigned value, including indexed and indirect stores.
- `&&` and `||` short-circuit and yield exactly `0` or `1`.
- Signed integer division truncates toward zero; the remainder has the dividend's sign. Division or remainder by a constant zero is a compile-time error; a runtime zero divisor is not checked.
- Shifts follow the Z80N barrel shifter: only the low five bits of the count are used. Counts 16 through 31 produce zero, except that signed right shifts sign-fill.

## Code-generation regression tests

Run `python tests\run_codegen_tests.py` from the repository root with a modern
GCC, z88dk `z80asm`, and `z88dk-ticks` on `PATH`. The `CC`, `Z80ASM`, and
`TICKS` environment variables can override the tool names or executable paths.

The runner builds a host compiler with no-op bank switching, assembles its
generated instructions without ZOPT, and executes them on the Z80N emulator.
It checks arithmetic boundaries, folded/runtime agreement, short-circuit side
effects, indexed addressing, assignment values, register/stack preservation,
and constant-multiply byte budgets. Native bank switching and NextZXOS I/O
still require target testing. Build the native compiler with `make all`.
