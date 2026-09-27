# zxn — ZX Spectrum Next register helpers

Description

Helpers for reading and updating ZX Spectrum Next registers.

Types

- None

Constants

- None

Globals

- None

Functions

- `byte updatereg(byte reg, byte value)` — Write `value` to Next register `reg` and return the register's previous value.

Examples

```c
byte old = updatereg(0x43, 0x30);
// restore the previous palette control value later
nextreg(0x43, old);
```