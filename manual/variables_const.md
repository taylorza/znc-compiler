# Variables & Constants

Declare variables using a type and name. Declarations may include initialization.

```c
int x;              // uninitialized local or global depending on scope
byte b = 42;        // initialized byte
fixed f = 1.5;      // fixed-point variable
const int C = 10;   // compile-time constant integer
const fixed PI = 3.14; // compile-time constant fixed-point
```

Scope rules
- Declarations inside functions are locals; outside are globals.
- Function arguments are declared in the function parameter list.

Constants
- `const` definitions are *compile-time constants* for scalar types (`char`, `int`, `fixed`). Example:

```c
const int MAX = 10;
const fixed HALF = 0.5;
int i = MAX;    // MAX folded to immediate value 10
fixed h = HALF; // HALF folded to immediate value 0x08 (Q4 representation of 0.5)
```

The compiler records the constant value at compile time — there is no separate storage emitted for `const` values.

Array/initializer notes are in the Arrays chapter.