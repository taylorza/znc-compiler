# Limitations & Notes

This section documents current limitations and implementation notes to help you write compatible code.

- Compound assignment operators (`+=`, `-=` etc.) are not implemented (use `x = x + y` instead).
 - `++`/`--` are supported for scalars and simple indexed lvalues. 
 - `const` definitions are treated as compile‑time constants and do not allocate storage; they are folded into code when possible.
 - The `nextreg`/`readreg` intrinsics are Next‑specific and assume ZX Spectrum Next hardware semantics.
 - Fixed‑size arrays do not support initializers. Use inferred‑size arrays for initialized data.
 - Out‑of‑bounds array access is undefined behavior (as with C).
