# Directives & Conditional Compilation

ZNC provides several top-level directives that affect code generation.

`make` — choose output format

Syntax

```
make dot ["<binary name>"];
make raw ["<binary name>"];
make nex ["<binary name>"] [, <stack base>];
```

`make` must be the first directive, optionally followed by `bank` and then `org`.

Behavior

- If no binary name is given, the source file's base name is used. `nex` appends the `.nex` extension.
- `dot` produces a DOT command binary. The default code origin is 0x2000.
- `nex` produces a NEX binary. If no `org` is given, the default code origin is 0xc000. The optional second argument sets the initial stack pointer; the default is `0xBFFF`. Only `nex` accepts a stack argument.
- `raw` produces a raw binary. The compiler does not set a code origin, so use `org` to set one.

`org` — set origin address

```
org 0x8000;
```

`bank` — target a specific bank

```
bank(40) {
  // code/data compiled into bank 40
}

bank(40, 0x1000) {
  // code/data compiled into bank 40 at offset 0x1000
}
```

Each bank number may be used only once, and bank blocks cannot be nested. The optional offset is passed to the assembler's `bank` directive.

Variables declared inside a bank block are globals. Their initializers are not run automatically when the bank is loaded, so assign initial values from a function:

```c
bank(1) {
  int score;
  void reset_score() { score = 5; }
}
```

Inside a `dot` program, a bank block without `org` has a default origin of 0x8000 plus the offset.

A function can be placed in a bank with the `bank` modifier, which takes no parentheses:

```c
int draw() bank 41 {
  ...
}
```

Calls to a function in another bank go through the `ccfarcall` trampoline. It switches MMU slot 6 (0xC000–0xDFFF) to the target bank and restores the previous mapping on return. Calls within the same bank are direct.

`include` — include another source file

```
include "file.znc"
```

The filename is a string literal and the directive takes no terminating `;`.

Conditional compilation

`#if`, `#ifdef`, `#ifndef`, `#elif`, `#else`, `#endif` are supported. Use `const` and defines to conditionally include code.

- `#if <const_expr>` — includes the block when the constant expression is non-zero.
- `#ifdef <ident>` — includes the block when the identifier is defined.
- `#ifndef <ident>` — includes the block when the identifier is not defined.
- `#elif <const_expr>` — chained condition; evaluated only when all preceding `#if`/`#elif` branches were not taken. Multiple `#elif` branches are allowed.
- `#else` — fallback block when no prior branch was taken.
- `#endif` — closes the conditional block.

Examples:

```c
// Use built-in defines set by `make` to include platform-specific code
#ifdef __NEX
include "nex_only.znc"
#else
include "dot_only.znc"
#endif

// Use a compile-time const to toggle inclusion
const int USE_FEATURE = 1;
#if USE_FEATURE
include "feature.znc"
#endif

// Chained #elif
const int VERSION = 2;
#if VERSION == 1
  puts("v1");
#elif VERSION == 2
  puts("v2");
#else
  puts("other");
#endif
```

Notes: `#ifdef` checks whether a symbol is defined, `#if` evaluates a compile-time constant expression; case matters for identifiers.

Predefined defines

- `__NEX`, `__DOT`, `__RAW` are set based on the `make` output and can be used in `#if` tests.
