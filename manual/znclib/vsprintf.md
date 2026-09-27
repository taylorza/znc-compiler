# vsprintf — Callback-based formatted output

Description

Format values and send each generated character to a caller-provided callback. This is the shared formatting engine used by the standard `printf` and `sprintf` libraries.

Types

- `delegate void PFN_PUTC(char ch, void* ctx)` — Character sink callback and caller-owned context.

Constants

- None

Globals

- None public.

Functions

- `void vsprintf(PFN_PUTC sink, void* sink_ctx, char *fmt, va_list args)` — Format `fmt` using `args` and call `sink` for each output character.

Examples

```c
struct output_ctx { char* pos; };

void write_char(char ch, void* ctx) {
  output_ctx* out = ctx;
  *out.pos++ = ch;
}

void write_value(int value, ...) {
  char[32] text;
  output_ctx out;
  out.pos = text;
  va_list args;
  va_start(args, value);
  vsprintf(write_char, &out, "Value: %d", args);
  va_end(args);
}
```

Notes

- Supported conversions include decimal, unsigned decimal, hexadecimal, fixed-point, character, string, and `%%`; width and zero padding are supported.