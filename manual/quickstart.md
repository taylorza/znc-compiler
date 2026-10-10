# Quickstart


```
// top of file
make dot "/dot/hello";

// code...
```

If `make` is not specified, code is assembled into memory (useful for embedding code into NextBASIC).

Output types supported by `make`:

- `dot` — DOT command binary (placed in `/dot/` folder on an SD card)
- `nex` — NEX binary (default code origin 0xc000 unless `org` is used)
- `raw` — raw binary; the compiler sets no load address, so use `org`

See the [Directives & Conditional Compilation](directives.md) chapter for more details about `make`, `org`, and `bank`.