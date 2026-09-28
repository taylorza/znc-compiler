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

- Explicit-size arrays may have initializers. Nested initializers are supported for arrays of arrays and arrays of structs.

```c
byte[5] b = {1, 2, 3, 4, 5};
int[2][2] matrix = {{1, 2}, {3, 4}};
```

Arrays of structs and unions use the same indexing and member access rules:

```c
Point[2] points = {{1, 2}, {3, 4}};
points[1].x = 9;

Value[2] values = {{13}, {14}};
values[0].low = 5;
```

Notes and limits
- `void*` is supported as a generic pointer type; it is compatible with any other pointer base type in assignments and calls. Pointer arithmetic on `void*` is not meaningful since element size is unknown.
- Out-of-bounds access is undefined (as with C).
- The compiler supports `char`, `byte`, `int`, `uint`, `fixed`, struct, and nested arrays.
- Struct and union declarations, including anonymous aggregate members, are documented in [Structs & Unions](structs_unions.md).
