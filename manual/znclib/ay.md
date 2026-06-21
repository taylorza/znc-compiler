# ay — AY-3-8912 stereo sound chip control

Description

Controls the AY sound chips for tone, noise, and envelope effects.

Types

- None

Constants

- `AY1` (byte) — 0b00000011
- `AY2` (byte) — 0b00000010
- `AY3` (byte) — 0b00000001
- `AY_LEFT` (byte) — 0b01000000
- `AY_RIGHT` (byte) — 0b00100000
- `AY_STEREO` (byte) — AY_LEFT | AY_RIGHT

Globals

- None

Functions

- `void ay_select(byte chip)` — Select AY chip (e.g., `AY1`, `AY2`, `AY3`).
- `void ay_wreg(byte reg, byte val)` — Write a value to AY register `reg`.
- `void ay_tone(byte chan, int tone, byte volume)` — Play a tone on channel `chan`.
- `void ay_noise(byte chan, int tone, byte volume)` — Play noise on channel.
- `void ay_envelope(byte chan, int tone, int period, byte shape)` — Play with envelope.
- `void ay_pgm(byte[] values)` — Program all AY registers from an array.
- `void ay_stop(byte chan)` — Stop a single channel.
- `void ay_stopall()` — Stop all channels.

Examples

Short tone example:

```c
// select chip 1 and tone on channel 0
ay_select(AY1);
ay_tone(0, 440, 10);
```
