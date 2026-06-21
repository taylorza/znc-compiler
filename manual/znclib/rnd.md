# rnd — Pseudorandom number generator

Description

XORShift-based pseudorandom generator and seeding helper.

Types

- None

Constants

- None

Globals

- None

Functions

- `int rnd()` — Return next pseudorandom integer.
- `void rndseed()` — Seed RNG from frame counter.

Examples

```c
rndseed();
int v = rnd() % 100; // 0..99
```
