# Functions

Declaration and definition

```c
int add(int a, int b);

int add(int a, int b) {
  return a + b;
}
```

- The compiler supports function prototypes (declarations) and definitions.
- The compiler records the expected argument count and checks call sites; if a call's argument count does not match a function prototype/definition, a compile-time error is produced.

Arguments

- Arguments are passed on the stack; the compiler keeps track of the number of arguments for checking.

Notes
- No varargs support is implemented.
- Functions may be implemented using inline assembly (`__asm__`) if necessary.