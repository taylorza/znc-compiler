# I/O & Hardware Registers

Built-in I/O helpers

- `putc(expr)` — emit a single character (uses runtime RTL `putc`).
- `puts(expr)` — emit a NUL-terminated string (uses runtime RTL `puts`).
- `in(expr)` — read a byte from an I/O port: the argument is an address expression; returns a `byte`.
- `out(expr1, expr2)` — write a byte to an I/O port: `expr1` is port/address, `expr2` is value.

Next-specific register access

ZNC exposes two Next register intrinsics:

- `nextreg(reg, value)` — write `value` to Next hardware register `reg` (used by the standard libraries for graphics, AY, etc.).
- `readreg(reg)` — read a Next hardware register value (returns `byte`).

Examples

```c
// write value to Next register 0x15
nextreg(0x15, 0b01000011);

// read and set bits
nextreg(6, (readreg(6) & 0b11111100) | 0b00000001);
```

Notes
- These intrinsics map to specific I/O sequences and are platform-specific to ZX Spectrum Next hardware.
