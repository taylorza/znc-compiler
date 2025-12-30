# Basic Syntax

A ZNC source file is split into directives and code. The language uses a C-like syntax with semicolons and braces.

- Statements end with `;`
- Blocks use `{ ... }`
- Identifiers are case-sensitive

Example:

```c
int main() {
  puts("Hello");
  return 0;
}
```

Note: ZNC integrates inline Z80N assembly and provides several built-in primitives described in later chapters.