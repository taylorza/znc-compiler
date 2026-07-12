# dirinfo — Directory access

Description

Simple directory listing helpers built on ESXDOS directory calls. Use these to open a directory search, apply wildcard filters, and iterate short (8.3) or long filenames.
Types

- `struct DIRENT` — Compact directory entry returned when using short (8.3) names.
  - `byte attr` — File attributes.
  - `char[12+8+1] name` — 8.3 name plus null terminator.

- `struct DIRENTLFN` — Long-filename directory entry.
  - `byte attr` — File attributes.
  - `char[255+8+1] name` — Long name (up to 255) plus null terminator.

Types

- `struct DIRENT` — Compact directory entry returned when using short (8.3) names.
  - `byte attr` — File attributes.
  - `char[12+8+1] name` — 8.3 name plus null terminator.

- `struct DIRENTLFN` — Long-filename directory entry.
  - `byte attr` — File attributes.
  - `char[255+8+1] name` — Long name (up to 255) plus null terminator.

Constants

- `DIR_SFN` (byte) — 0x00 — Use short (8.3) filename form.
- `DIR_LFN` (byte) — 0x10 — Use long filename form.
- `DIR_WILDCARD` (byte) — 0x20 — Use `filter` as file pattern to search for.
- `DIR_EN_SF` (byte) — 0x80 — Enable sorting and filtering using sf_flags.

Filter flags (pass as `sf_flags` to `fopendir`):

- `F_EX_FILES` (byte) — 0x80 — Exclude regular files.
- `F_EX_DIRS` (byte) — 0x40 — Exclude directories.
- `F_EX_DOTS` (byte) — 0x20 — Exclude `.`/`..` entries.
- `F_EX_SYS` (byte) — 0x10 — Exclude system files.

Sort flags (add to `sf_flags` to sort results):

- `S_LFN` (byte) — 0x00
- `S_SFN` (byte) — 0x01
- `S_DATE` (byte) — 0x02
- `S_SIZE` (byte) — 0x03
- `S_REVERSED` (byte) — 0x04

Globals

- None

Functions

- `byte fopendir(char *dir, char *filter, byte mode, byte sf_flags)` — Open a directory search on `dir` (e.g. `"."`). `filter` is a filename or wildcard pattern. `mode` selects filename mode and behaviour (see Constants). Returns a handle (non-zero) or `0` on error; check `errno` for details.
- `byte freaddir(byte handle, char *filter, void* dirent)` — Read the next matching entry into the buffer pointed to by `dirent` (use either `DIRENT` or `DIRENTLFN`). Returns non-zero while entries are returned, and `0` when there are no more matches.

Examples

```c
DIRENTLFN dirent;
byte hdir = fopendir(
  ".",
  "*.znc",
  DIR_LFN | DIR_WILDCARD | DIR_EN_SF,
  F_EX_DIRS | F_EX_DOTS | F_EX_SYS);

if (!hdir) return errno;

while (freaddir(hdir, "*.znc", &dirent)) {
  puts(dirent.name);
}

fclose(hdir);
```

Notes

- `fopendir` / `freaddir` use the same ESXDOS-style handle model as the other `znclib` file APIs; close directory handles with `fclose()`.
- Check `errno` after failures to get the ESXDOS error code.
