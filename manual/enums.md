# Enums

Enums define a named integral type with named members.

Declaration

```c
enum Color { RED, GREEN, BLUE };
enum State { IDLE = 0, RUN = 10, DONE };
```

Rules

- Enum declarations are top-level items.
- Member values are compile-time constants.
- If a member omits `= value`, it uses the previous value plus one.
- The first member defaults to `0` when no explicit value is given.
- A trailing comma before `}` is allowed.

Using enums

```c
enum Color { RED, GREEN, BLUE };

Color c = Color.GREEN;
int n = Color.BLUE;

if (c == Color.GREEN) {
  puts("green");
}
```

Member access

- Enum members are referenced with `TypeName.MemberName`.
- Unqualified member names are not used; use the enum type name as a prefix.

Type behavior

- Enum values are integer-like and occupy 2 bytes (same storage size as `int`).
- Same-enum assignment is allowed.
- Cross-enum assignment is rejected (for example, assigning `enum A` to `enum B`).
- Enum-to-scalar and scalar-to-enum assignment are allowed.

Switch with enums

```c
enum Mode { OFF, ON, AUTO };

switch (Mode.AUTO) {
  case Mode.OFF:
    puts("off");
    break;
  case Mode.ON:
    puts("on");
    break;
  case Mode.AUTO:
    puts("auto");
    break;
}
```
