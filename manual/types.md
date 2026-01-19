# Data Types

ZNC provides a compact static type system tailored for ZX Spectrum Next.

Base types
- `char` / `byte`: signed 8‑bit scalar. `byte` is an alias for `char`.
- `int`: signed 16‑bit scalar.
- `void`: used only for function return type (not for pointers).

Composite types
- Pointers: `T*` (size: 2 bytes). Pointer arithmetic scales by `sizeof(T)`.
- Arrays: `T[n]` (size: `n * sizeof(T)`). If `[]` is used without a length in a variable declaration, it is treated as a pointer type; in member declarations inside `struct`, `[]` means pointer as well.
- Structs: `struct Name { /* field declarations */ }`.
- Function types: created implicitly for functions and explicitly via `delegate` (see below).

Named function-pointer types (delegates)
- `delegate <ret> Name(<args...> [,...]) ;` defines a named function‑pointer type.
- Example: `delegate int Callback(int, int);` then `Callback cb;` declares a variable holding a pointer to a function of that signature.

Const qualifier
- `const` can be applied only to scalar types (`char`, `int`) to produce compile-time constants. `const` is not permitted on `void`, pointers, or arrays. Const values are folded into code with no storage allocated.

Sizes
- `sizeof(char)` = 1, `sizeof(int)` = 2.
- Pointers are 2 bytes regardless of base type.
- `sizeof(struct)` is the sum of its field sizes; array sizes are computed statically where a fixed length is provided.

Type compatibility rules
- Same type: identical type IDs are compatible.
- Scalars: `char` and `int` are mutually compatible; implicit conversion is allowed.
- Arrays ↔ arrays: only compatible when both element type and length are identical.
- Arrays ↔ pointers: assigning an array to a pointer of matching element type is allowed (array decays to pointer). Assigning a pointer to an array is not allowed.
- Pointers ↔ pointers:
	- Indirection levels must match.
	- `void*` is compatible with any pointer base type.
	- Base element types must match for non‑`void*` pointers.
- Scalars ↔ pointers: assigning an integer (char/int) to a pointer is allowed (commonly used for absolute addresses). Assigning a pointer to a scalar is not allowed.
- Structs: only exact struct types (same struct ID) are compatible; no implicit conversions between different structs.
- Delegates/function pointers: assignment requires a matching signature (same return type, argument types, and variadic flag). Calls are checked against the callee’s signature.

Notes
- Arrays with explicit fixed size do not support initializers; use inferred‑size arrays (e.g., `byte[] s = "Hi";`) or copy data at runtime.
- Pointers are typed; arithmetic scales by element size. No `void*` support.
