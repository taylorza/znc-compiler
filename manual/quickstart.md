# Quickstart


```
// top of file
make dot "/dot/hello";

// code...
```

If `make` is not specified, code is assembled into memory (useful for embedding code into NextBASIC).

Output types supported by `make`:

- `dot` — DOT command binary (placed in `/dot/` folder on an SD card)
- `nex` — NEX binary
- `raw` — raw binary, default load address 0xc000

See the [Directives & Conditional Compilation](directives.md) chapter for more details about `make`, `org`, and `bank`.