# Pointers & Arrays

Pointers

- Use `*` to form a pointer type: `byte *p = 0x4000;`
- Dereference with `*p` to read/write the pointed memory.
- Pointer arithmetic follows element-size scaling: adding `1` to an `int*` advances by 2 bytes; adding `1` to a `byte*` advances by 1 byte.
- `a[b]` is equivalent to `*(a + b)`.

Arrays

- Declare arrays with `type[size]`:

```c
int[10] nums;
byte[6] s;
```

- If you omit the size in an array declaration and provide an initializer, the compiler infers the array length from the initializer list or string literal:

```c
int[] nums = {1000, 2000}; // nums has length 2
byte[] s = "Hi";          // s has length 3 == {'H','i',0}
```

- If you omit the size and there is no initializer, the declaration is treated as a pointer, not an array type:

```c
int[] p;   // treated as `int *p` (a pointer) when no initializer is given
```

- Arrays with an explicit size do **not** support an initializer. Attempting to write `byte[6] x = {..};` or `byte[6] s = "Hi";` is not supported by the compiler (use `byte s[] = "Hi";` to infer the size instead). If you need a fixed-size zero-filled buffer, declare it without an initializer and fill it at runtime.

```c
byte[5] b; // declare fixed-size array; no initializer allowed in source
```

Notes and limits
- There is no `void*` type; pointers are typed and arithmetic is done w.r.t element size.
- Out-of-bounds access is undefined (as with C).
- The compiler supports both `byte` and `int` arrays (emitted as DB and DW respectively).
