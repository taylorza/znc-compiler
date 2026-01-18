# Delegates (Named Function Pointers)

Delegates define a named function-pointer type with a fixed signature, making it easy to declare variables and fields that can hold callable pointers.

Declaration

```c
delegate int Callback(int, int);
```

Usage

```c
int add(int a, int b) { return a + b; }
int sub(int a, int b) { return a - b; }

Callback op;         // variable holding a function pointer
op = add;            // assignment requires matching signature

int r = op(3, 4);    // calls through the function pointer
```

Rules

- A `delegate` creates a fixed signature: return type and ordered argument types.
- Assignment to a delegate variable requires an exact signature match.
- Calls through a delegate are validated at compile time against the stored signature.
- Delegates are not variadic; use a concrete signature (e.g., `int f(int, int)`), not `...`.

Struct fields

```c
struct Ops {
  Callback fn;
};

Ops o;
o.fn = sub;
int r = o.fn(10, 3);
```

Interoperability

- A `delegate` name is a named type. You can use it anywhere a type is expected (variables, fields, parameters).
- Internally, delegates are `T*` where `T` is a function type carrying the signature.

Notes

- If you need runtime selection among different operations, store several `delegate` variables and assign them as needed.
- Signatures are checked for both assignment and calls, helping catch mismatches at compile time.
