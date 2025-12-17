#!/usr/bin/env python3
import sys
from pathlib import Path

if len(sys.argv) != 3:
    print("Usage: rtlgenerate.py <input.asm> <output.inc>")
    sys.exit(2)

src_path = Path(sys.argv[1])
out_path = Path(sys.argv[2])

s = src_path.read_text(encoding='utf-8').replace('\r\n', '\n')
lines = s.split('\n')
with out_path.open('w', encoding='utf-8') as f:
    for i, line in enumerate(lines):
        # escape backslashes and double quotes
        esc = line.replace('\\', '\\\\').replace('"', '\\"')
        if i == len(lines) - 1:
            # last line: emit string and trailing comma for initializer
            f.write('"' + esc + "\\n" + '",' + '\n')
        else:
            f.write('"' + esc + "\\n" + '"' + '\n')

print(f"Wrote {out_path}")
