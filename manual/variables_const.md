# Variables & Constants

Declare variables using a type and name. Declarations may include initialization.

```c
int x;          // uninitialized local or global depending on scope
byte b = 42;    // initialized byte
const int C = 10; // compile-time constant
```

Scope rules
- Declarations inside functions are locals; outside are globals.
- Function arguments are declared in the function parameter list.

Constants
- `const` definitions are *compile-time constants*. Example:

```c
const int MAX = 10;
int i = MAX; // MAX is folded into the code as the immediate value 10
```

The compiler records the constant value at compile time — there is no separate storage emitted for `const` values.

Array/initializer notes are in the Arrays chapter.