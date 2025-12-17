#!/usr/bin/env python3
from pathlib import Path
import subprocess

tools = Path(__file__).resolve().parent
root = Path(__file__).resolve().parent.parent
tools = Path(__file__).resolve().parent
rtl_dir = root / 'RTL'
generated_dir = rtl_dir / 'generated'
script = tools / 'rtlgenerate.py'

generated_dir.mkdir(parents=True, exist_ok=True)

for asm in sorted(rtl_dir.glob('*.asm')):
    inc = generated_dir / (asm.stem + '.inc')
    subprocess.check_call(["python", str(script), str(asm), str(inc)])
    print(f"Generated {inc}")
