# joystick — Joystick/gamepad input

Description

Read joystick directions and buttons from supported hardware (Kempston-style, etc.).
Types

- None

Constants

- `JOYSTK_RIGHT` — 0x0001
- `JOYSTK_LEFT`  — 0x0002
- `JOYSTK_DOWN`  — 0x0004
- `JOYSTK_UP`    — 0x0008
- `JOYSTK_B`     — 0x0010
- `JOYSTK_C`     — 0x0020
- `JOYSTK_A`     — 0x0040
- `JOYSTK_START` — 0x0080
- `JOYSTK_SELECT`— 0x0100
- `JOYSTK_Y`     — 0x0200
- `JOYSTK_Z`     — 0x0400
- `JOYSTK_X`     — 0x0800

Globals

- None

Functions

- `byte initJoystick()` — Initialize joystick subsystem.
- `int readJoy1()` — Read state for joystick 1.
- `int readJoy2()` — Read state for joystick 2.
- `byte buttonJoy(byte n, int mask)` — Test button `mask` for joystick `n`.

Examples

```c
initJoystick();
int s = readJoy1();
if (buttonJoy(1, JOYSTK_A)) shoot();
```
