# cmdline — Command-line parsing

Description

Split a command-line string into `argv`-style tokens handling quotes.

Types

- None

Constants

- None

Globals

- None

Functions

- `byte parse_cmdline(char *cmdline, char **argv, byte maxargs)` — Parse `cmdline`, populate `argv`, return `argc`.

Examples

```c
char*[8] argv;
byte argc = parse_cmdline(args, argv, 8);
for (byte i = 0; i < argc; i++) printf("arg %d: %s\r", i, argv[i]);
```
