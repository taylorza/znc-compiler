# Limitations & Notes

This section documents current limitations and implementation notes to help you write compatible code.

- `++`/`--` are supported for scalars and simple indexed lvalues.
- `const` definitions are compile-time constants for scalar types (`char`, `int`, `fixed`) and do not allocate storage; values are folded into code.
- The `nextreg`/`readreg` intrinsics are Next-specific and assume ZX Spectrum Next hardware semantics.
- Fixed-size arrays do not support initializers. Use inferred-size arrays (`byte[] s = "Hi";`) for initialized data.
- Out-of-bounds array access is undefined behavior (as with C).
- `fixed` (Q12.4 fixed-point) arithmetic uses 16-bit signed values; overflow behaviour is the same as `int` overflow on the Z80.
- Comments inside `__asm__` blocks (`//` and `;`) are stripped by the ZNC scanner and do not appear in the generated assembly output.
