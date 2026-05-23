# Directives & Conditional Compilation

ZNC provides several top-level directives that affect code generation.

`make` — choose output format

Syntax

```
make <type> ["<binary name>"];
```

`<type>`: `dot`, `nex`, `raw`.

Behavior

- `dot` produces a DOT command binary
- `nex` produces a NEX binary
- `raw` produces a raw binary (default load address 0xc000)

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

`include` — include another source file

```
include "file.znc";
```

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
include "nex_only.znc";
#else
include "dot_only.znc";
#endif

// Use a compile-time const to toggle inclusion
const int USE_FEATURE = 1;
#if USE_FEATURE
include "feature.znc";
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
