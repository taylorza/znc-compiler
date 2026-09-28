# Structs & Unions

ZNC supports named structures, named unions, nested aggregates, and anonymous structure or union members.

Struct declarations

A `struct` contains fields laid out in declaration order. Declare a named structure at top level, then use its name as a type:

```c
struct Point {
  int x;
  int y;
};

Point p;
p.x = 10;
p.y = 20;
```

Fields may be scalar values, pointers, arrays, delegates, or other struct and union types. Arrays in fields use the same syntax as other declarations:

```c
struct Sprite {
  byte x;
  byte y;
  int velocity;
  byte[3] pixels;
};

Sprite s = {5, 6, 42, {1, 2, 3}};
```

Nested named aggregates

A named aggregate can be used as a field of another aggregate. Access follows the member path:

```c
struct Rect {
  Point top_left;
  Point bottom_right;
};

Rect r;
r.top_left.x = 0;
r.bottom_right.y = 100;
```

The same access syntax works through a pointer. A pointer to a struct uses `.` for member access:

```c
Point *p = &r.top_left;
p.x = 4;
```

Unions

A `union` declares alternative views of the same storage. Every member starts at offset zero, and the union size is the size of its largest member:

```c
union Value {
  byte low;
  uint word;
};

Value v;
v.low = 7;
v.word = 0x1234;
```

Writing one union member changes the bytes observed through the other members. The compiler does not track which member is currently active.

Anonymous structs and unions

An unnamed `struct` or `union` can be declared directly inside another aggregate without a field name. Its fields are promoted into the containing aggregate and can be accessed directly:

```c
union Mix {
  Point named;
  struct {
    byte left;
    byte right;
  };
};

Mix m;
m.named.x = 4;
m.left = 6;
m.right = 7;
```

Anonymous unions work the same way inside a struct:

```c
struct Holding {
  Value named;
  union {
    byte low;
    uint wide;
  };
};

Holding h;
h.named.low = 10;
h.wide = 0x3456;
```

An anonymous struct nested in a struct contributes its fields at the containing struct's current offset. An anonymous struct nested in a union starts at the union's offset zero. Anonymous union members follow the same enclosing layout rules.

Initialization

Structs and unions support brace initializers. Nested aggregates use nested braces, and arrays of aggregates can be initialized element by element:

```c
Value first = {12};
Value[2] values = {{13}, {14}};
Rect rect = {{1, 2}, {3, 4}};
Mix[2] mixes = {{{7, 8}}, {{9, 10}}};
```

For a union initializer, the first member is the member represented by the initializer. Missing struct or array values are zero-filled. Array members can use nested initializers or string literals when the element type permits it.

Layout and size

- Struct fields are stored in declaration order.
- A struct's size is the sum of its field sizes, including nested aggregate sizes.
- All union members have offset zero.
- A union's size is its largest member's size.
- `sizeof(struct_type)` and `sizeof(union_type)` follow those layout rules.

Compatibility

Struct and union types are nominal: two separately declared aggregates with identical fields are still different types. Assignments require the same declared aggregate type; there is no implicit conversion between different structs or unions.

Related topics

- [Data Types](types.md)
- [Expressions & Operators](expressions.md)
- [Pointers & Arrays](pointers_arrays.md)