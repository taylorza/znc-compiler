# Inline Assembly & __asm__

ZNC supports inline assembly blocks and `__asm__` annotated functions.

Inline `asm` statements:

```c
__asm__ {
  ld hl, .data
  // any text is emitted literally as assembler lines
}
```

`__asm__` functions

You can define a function in ZNC that is implemented entirely in assembler:

```c
void foo() __asm__ {
_
  // asm body
  ret
}
```

The compiler accepts tokens inside an `asm` block and emits them verbatim as assembler lines.

Comments in `__asm__` blocks

Both `//` (C-style) and `;` (standard Z80 assembler style) are valid line comments inside `__asm__` blocks. Both are processed by the ZNC scanner and **stripped before the assembly output is written**. They serve as source-level comments visible only in the `.znc` file. No comment text appears in the generated `.asm` file.

Notes
- Use inline assembly for highly-optimized sequences.
- Take care with calling conventions and stack/frame setup when mixing asm and C-style functions. If you implement an assembler function that uses the callee‑cleanup calling convention, mark the function with `__znccall(1)` and ensure the asm body adjusts/pops argument words before returning so the stack is left consistent for the caller.