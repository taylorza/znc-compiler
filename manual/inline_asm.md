# Inline Assembly & __asm__

ZNC supports inline assembly blocks and `__asm__` annotated functions.

Inline `asm` statements:

```c
__asm__ {
  ld hl, .data
  ; any text is emitted literally as assembler lines
}
```

`__asm__` functions

You can define a function in ZNC that is implemented entirely in assembler:

```c
void foo() __asm__ {
_
  ; asm body
  ret
}
```

The compiler accepts tokens inside an `asm` block and emits them as written, with simple indentation rules.

Notes
- Use inline assembly for platform-specific or highly-optimized sequences.
- Take care with calling conventions and stack/frame setup when mixing asm and C-style functions.