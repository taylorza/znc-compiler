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

- Access variadic arguments using `va_list` and intrinsic functions:
  - `va_start(list, lastNamedParam)` initializes the variadic argument list.
  - `va_arg(list, type)` retrieves the next argument of the given type.
  - `va_end(list)` cleans up the argument list.

Example:

```c
int sum(int count, ...) {
  va_list args;
  int total = 0;
  va_start(args, count);
  while (count--) total = total + va_arg(args, int);
  va_end(args);
  return total;
}
```

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

- By default, arguments are passed on the stack (16‑bit words) and the caller is responsible for cleaning the stack after the call (caller‑cleanup). Return values are in `HL`.
- For variadic functions, the compiler also pushes the count of variadic arguments.
- Calling convention control: annotate a function with `__znccall(1)` immediately after the parameter list to request a callee‑cleanup calling convention. With `__znccall(1)` the callee is responsible for popping/adjusting its argument words before returning. Example:

```c
int add(int a, int b) __znccall(1) {
  return a + b;
}
```

- When using `__znccall(1)` ensure that callers and any function‑pointer or `delegate` types agree on the convention. A mismatch between caller‑cleanup and callee‑cleanup will corrupt the stack and cause crashes; the compiler validates signatures (and will check conventions where it can) for direct calls and delegate assignments.
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