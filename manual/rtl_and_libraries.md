# Libraries

ZNC ships with a set of convenience libraries in `znclib/`. These are plain ZNC source files that provide useful helper functions implemented for the ZX Spectrum Next environment and may call platform-specific I/O (e.g., `nextreg`, `readreg`) or use `__asm__` functions for performance.

Include a library file in your source with the `include` directive, for example:

```c
include "string.znc";
```

Available libraries (brief descriptions):

- `string.znc` — string helpers: `strcpy`, `strlen`, `strcmp`, `strcat`, `strstr`, and `strstr` variants implemented in optimized code.
- `rnd.znc` — pseudo-random number generator and seed helper (`rnd`, `rndseed`).
- `sprite.znc` — sprite helpers: load sprites into VRAM, enable/disable sprites, and update sprite attributes via Next registers.
- `io.znc` — console helpers: `printnum`, `gotoxy`, `ink`, `paper`, and `ink/paper` style primitives; includes `inkey`/`waitkey` helpers.
- `fileio.znc` — file I/O wrappers: `fopen`, `fread`, `fwrite`, `fclose`, and `errno` handling.
- `memory.znc` — memory helpers: `memcpy` and `memset`.
- `ay.znc` — AY audio helpers: register access wrappers, `ay_tone`, `ay_noise`, `ay_effect`, and convenience functions for tone/envelopes.
- `joystick.znc` — joystick helpers: read joystick states (`readJoy1`, `readJoy2`) and convenience bit masks and button helpers.
- `mouse.znc` — mouse helpers: read X/Y, buttons, and wheel (where hardware supports it).
- `datetime.znc` — date/time helpers: query RTC/datetime and extract components into buffers.
- `intToMonth.znc` — small helper to convert an integer month number to a month name (`intToMonth` and abbreviated form `intToShMonth`).
- `strcontains.znc` — helper to test whether a substring occurs in a string (`strcontains`).
- `strtoint.znc` — ASCII to integer conversion helper (`strtoint`).
- `version.znc` — helpers to read esxDOS/core version numbers and the Next core version (`exdosVersion`, `coreVersion`).

Notes
- Some libraries expose `__asm__` functions for efficiency and may depend on Next-specific services (e.g., esxDOS, Next registers). They are intended as convenience code; feel free to adapt or extend them for your needs.
- The RTL (runtime) used by the compiler is internal — this page documents only the `znclib/` helper libraries.
