# Functions

Declaration and definition

```c
int add(int a, int b);

int add(int a, int b) {
  return a + b;
}
```

- Prototypes and definitions are supported. The compiler records signatures and checks call sites.
- Argument count and types are validated against the declared signature.

Variadic functions

- Declare with an ellipsis at the end of the fixed parameter list:

```c
int logf(int level, const char* fmt, ...);
```

- Intrinsics for accessing variadic arguments inside the function body:
  - `__va_count()` returns the number of variadic arguments passed.
  - `__va_arg(index)` returns the `int` value at variadic position `index` (0‑based) from the caller.

Delegates (named function‑pointer types)

- Define a named function‑pointer type:

```c
delegate int Callback(int, int);
```

- Use it to declare variables or fields that hold pointers to functions with that signature:

```c
Callback cb;           // variable holding a function pointer
cb = some_function;    // assignment requires matching signature
```

Calling conventions

- Arguments are passed on the stack (16‑bit words). Return values are in `HL`.
- For variadic functions, the compiler also pushes the count of variadic arguments.
- Function calls via direct symbol or via a function pointer (delegate) are supported; pointer calls are validated against the pointer’s stored signature.

Inline assembly functions

- Functions may be implemented using inline assembly with `__asm__` after the signature:

```c
void foo() __asm__ {
_
  ; asm body
  ret
}
```