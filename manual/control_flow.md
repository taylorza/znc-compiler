# Control Flow

Conditionals

```c
if (cond) {
  // ...
} else {
  // ...
}
```

Loops

- `while` and `for` are supported:

```c
while (cond) { ... }
for (init; cond; post) { ... }
```

Switch

`switch` with `case` and `default` is supported. Cases must be compile-time constants (including enum members such as `Color.GREEN`).

Example:

```c
switch (val) {
  case 0:
    puts("zero");
    break;
  case 1:
  case 2:
    puts("one or two");
    break;
  default:
    puts("other");
}
```

Notes: adjacent `case` labels can fall through if there is no `break`; use this intentionally to group cases.

Enum example:

```c
enum Color { RED, GREEN, BLUE };

switch (Color.BLUE) {
  case Color.RED:
    puts("red");
    break;
  case Color.GREEN:
    puts("green");
    break;
  case Color.BLUE:
    puts("blue");
    break;
}
```

Break/Continue

`break` exits the innermost loop/case block; `continue` jumps to the next iteration.

Return / Exit

- `return expr;` inside functions returns a value.
- At top-level (no `make` directive) `return` exits execution (the DOT command behavior is: returning `int` yields that integer as standard error; returning a string indicates a custom DOT error — see Limitations & Notes for details).
