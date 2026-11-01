#!/usr/bin/env python3
"""Compile ZNC expressions, assemble them, and check results on a Z80N emulator."""

import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile


ROOT = Path(__file__).resolve().parents[1]


def command(args, cwd):
    result = subprocess.run(args, cwd=cwd, text=True, capture_output=True, timeout=120)
    return result


def run(args, cwd):
    result = command(args, cwd)
    if result.returncode:
        raise RuntimeError(f"{args[0]} failed:\n{result.stdout}\n{result.stderr}")
    return result.stdout


def assemble_text(text):
    """Translate only assembler syntax; do not optimize the emitted instructions."""
    text = re.sub(r"^\s*(map|options)\s.*\n", "", text, flags=re.MULTILINE)
    text = re.sub(r"^(?!ldir\b)([A-Za-z_]\w*)\s*$", r"\1:", text, flags=re.MULTILINE)
    text = re.sub(r"^(\w+)\s+(db|dw|ds)\b", r"\1: \2", text, flags=re.MULTILINE)
    text = re.sub(r"\bds\b", "defs", text)
    scope = "entry"
    lines = []
    for line in text.splitlines():
        label = re.match(r"^(\w+):", line)
        if label:
            scope = label[1]
        line = re.sub(r"(?<!\w)\.(\w+)", lambda m: f"{scope}_{m[1]}", line)
        line = re.sub(r"^(\w+_\w+)\s*$", r"\1:", line)
        lines.append(line)
    return "\n".join(lines) + "\n"


def emulate(assembler, ticks, work, assembly):
    (work / "test.asm").write_text(assemble_text(assembly))
    run([assembler, "-b", "-m", "-mz80n", "test.asm"], work)
    if len((work / "test.bin").read_bytes()) >= 0x6000:
        raise AssertionError("Generated code/data overlaps the emulator result buffer")
    run([ticks, "-mz80n", "-pc", "8000", "-l", "0x8000", "-end", "ffff",
         "-counter", "10000000", "-output", "ram.bin", "test.bin"], work)
    ram = (work / "ram.bin").read_bytes()
    # ticks appends registers after RAM; ensure a timeout cannot look like success.
    if len(ram) < 65544 or ram[0x10006:0x10008] != b"\xff\xff":
        raise AssertionError("Emulated program did not reach its completion address")
    return ram


def execute(cases, compiler, assembler, ticks, work):
    source = [
        "int identity(int x) { return x; }",
        "uint unsigned_identity(uint x) { return x; }",
        "fixed fixed_identity(fixed x) { return x; }",
        "int calls = 0;",
        "int tick() { calls++; return 7; }",
        "int[3] items = {11, 22, 33};",
        "int* p = &items[0];",
        "struct Box { int pad; int[3] values; };",
        "Box box = {99, {11, 22, 33}};",
        "struct Links { int* numbers; Box* child; };",
        "Links links;",
        "links.numbers = p;",
        "links.child = &box;",
        "struct Wide { byte[13] data; };",
        "Wide[3] wide;",
        "Wide* wp = &wide[0];",
        "int address_delta(Wide* q) { return (int)q - (int)wp; }",
        "int local_index(int i) { int[80] a; a[79] = 1234; return *(&a[i]); }",
        "int local_byte(int i) { byte[140] a; a[139] = 201; return *(&a[i]); }",
        "int assign_index() { return (items[1] = 45); }",
        "int prefix_index() { return ++items[0]; }",
        "int local_assign() { byte[140] a; return (a[139] = 201); }",
        "int far_local() { byte[140] a; int x; return (x = 1234); }",
        "int zero_product() { int x = 1; int y = x++ * 0; return x + y; }",
        # Stores must leave the stored value in HL for the enclosing expression.
        "int[3] cells = {1, 2, 3};",
        "int compound_index() { int r = (cells[1] *= 7); return r * 100 + cells[1]; }",
        "int postfix_cell() { int v = cells[2]++; return v * 100 + cells[2]; }",
        "int chained() { int a; int b; a = b = 9; return a + b; }",
        "char[2] chars;",
        "int char_store() { chars[1] = -5; return chars[1]; }",
        "int shift_assign() { int x = -3; x <<= 2; x >>= 1; return x; }",
        "int mod_assign() { int x = -17; x %= 5; return x; }",
        "int far_compound() { byte[140] a; int x = 30; x -= 4; return x; }",
        "int sum_va(int cnt, ...) { va_list args; int total = 0; va_start(args, cnt);"
        " while (cnt--) total = total + va_arg(args, int); va_end(args); return total; }",
        "int va_assign(int n, ...) { return (n = n + 5); }",
        # Assignment values reused by the enclosing expression (peephole hazards).
        "byte gb; byte gb2; char gc; int gi;",
        "int zero_byte_chain() { int a = 9; a = gb = 0; return a + 1; }",
        "int zero_local_chain() { int a = 9; int b = 9; a = b = 0; return a + b + 1; }",
        "int byte_assign_value() { return (gb = 1) + 1; }",
        "int byte_copy_value() { gb = 77; return (gb2 = gb); }",
        "int char_ptr_value() { char* q = &gc; gc = -3; return (gb = *q); }",
        "int mod_byte_value() { int x = 47; return (gb = x % 10); }",
        "int return_global_postinc() { gi = 5; return gi++; }",
        "int return_local_postinc() { int i = 5; return i++; }",
        "int early_global_postinc(int c) { gi = 5; if (c) return gi++; return 99; }",
        "int early_local_postinc(int c) { int i = 5; if (c) return i++; return 99; }",
        "int shifted_arg() { int x = -64; return identity(x >> 2); }",
        # out() must still hold the port in BC after evaluating a BC-clobbering value.
        "int out_port_shift(int x) { out(4660, x >> 1);",
        "__asm__ {", "ld l,c", "ld h,b", "}", "}",
        "int out_port_call(int x) { out(4660, identity(x) + 4096);",
        "__asm__ {", "ld l,c", "ld h,b", "}", "}",
        f"int[{len(cases)}] results;",
    ]
    source.extend(f"results[{i}] = {expression};"
                  for i, (expression, _) in enumerate(cases))
    source.extend(("__asm__ {", "ld hl,_results", "ld de,$e000",
                   f"ld bc,{2 * len(cases)}", "ldir", "jp $ffff", "}"))
    (work / "test.znc").write_text("\n".join(source) + "\n")
    run([str(compiler), "test.znc", "test.asm"], work)
    assembly = (work / "test.asm").read_text().replace(
        '  include "test.rtl"', (work / "test.rtl").read_text())
    ram = emulate(assembler, ticks, work, 'org $8000\nld sp,$ff00\n' + assembly)
    failures = []
    for i, (expression, expected) in enumerate(cases):
        actual = int.from_bytes(ram[0xE000 + 2 * i:0xE002 + 2 * i], "little")
        if actual != expected:
            failures.append(f"{expression}: got {actual:#06x}, expected {expected:#06x}")
    return failures


def check_emitters(cc, sources, assembler, ticks, work):
    emitter = work / "emitters.exe"
    run([cc, "-std=c99", "-O2", "-I", str(ROOT / "tests" / "host"),
         "-I", str(ROOT), *[p for p in sources if Path(p).name != "main.c"],
         str(ROOT / "tests" / "host" / "platform.c"),
         str(ROOT / "tests" / "host" / "emitters.c"), "-o", str(emitter)], ROOT)
    assembly = run([str(emitter)], work)
    assembly = assembly.replace('include "test.rtl"', (work / "test.rtl").read_text())
    ram = emulate(assembler, ticks, work, assembly)
    expected_values = (0x8001, 0x2468, 0xFF01, 0x5432 * 13 & 0xFFFF, 0x2468,
                       0x5432 * 13 * 256 & 0xFFFF, 0x2468)
    for i, expected in enumerate(expected_values):
        actual = int.from_bytes(ram[0xE000 + 2 * i:0xE002 + 2 * i], "little")
        if actual != expected:
            raise AssertionError(f"Emitter check {i}: got {actual:#x}, expected {expected:#x}")
    symbols = dict(re.findall(r"^(\w+)\s*=\s*\$([0-9A-Fa-f]+)", (work / "test.map").read_text(),
                              flags=re.MULTILINE))
    for name, budget in (("mul3", 4), ("mul5", 5), ("mul256", 7)):
        size = int(symbols[name + "_end"], 16) - int(symbols[name], 16)
        if size > budget:
            raise AssertionError(f"{name} uses {size} bytes, budget is {budget}")
    print("PASS: emitter register/stack checks; multiply-by-3/5/256 budgets: 4/5/7 bytes")


def main():
    cc = os.environ.get("CC", "gcc")
    assembler = os.environ.get("Z80ASM", "z80asm")
    ticks = os.environ.get("TICKS", "z88dk-ticks")
    for tool in (cc, assembler, ticks):
        if not shutil.which(tool):
            raise RuntimeError(f"Required tool not found: {tool}")

    cases = []

    def case(expression, expected):
        cases.append((expression, expected & 0xFFFF))

    for a in (-32768, -30000, -257, -7, -1, 0, 1, 7, 255, 30000, 32767):
        for b in (-32768, -1, 0, 1, 32767):
            for op, expected in (
                ("<", a < b), ("<=", a <= b), (">", a > b),
                (">=", a >= b), ("==", a == b), ("!=", a != b),
            ):
                case(f"identity({a}) {op} identity({b})", expected)
        for b in (1, 2, 8, 256, -1, -8, -32768):
            quotient = abs(a) // abs(b) * (-1 if (a < 0) != (b < 0) else 1)
            for expression in (f"{a} / {b}", f"identity({a}) / {b}",
                               f"identity({a}) / identity({b})"):
                case(expression, quotient)
            case(f"{a} % {b}", a - quotient * b)
            case(f"identity({a}) % {b}", a - quotient * b)
        for count in (0, 1, 4, 15, 16, 31, 32, 255):
            case(f"({a}) >> {count}", a >> (count & 31))
            case(f"identity({a}) >> {count}", a >> (count & 31))
            case(f"({a}) << {count}", (a & 0xFFFF) << (count & 31))
            case(f"identity({a}) << {count}", a << (count & 31))
        for factor in (0, 1, 2, 3, 5, 6, 7, 8, 9, 10, 12, 13, 256, 257):
            case(f"identity({a}) * {factor}", a * factor)
            case(f"{factor} * identity({a})", a * factor)
            case(f"identity({a}) * identity({factor})", a * factor)

    for a in (0, 1, 32767, 32768, 65535):
        for divisor in (1, 2, 8, 256, 32768, 32769, 65535):
            case(f"unsigned_identity({a}) / {divisor}", a // divisor)
            case(f"unsigned_identity({a}) % {divisor}", a % divisor)
            case(f"unsigned_identity({a}) / unsigned_identity({divisor})", a // divisor)
            case(f"unsigned_identity({a}) % unsigned_identity({divisor})", a % divisor)

    for a, b in ((0, 0), (0, 7), (3, 0), (3, 7)):
        case(f"identity({a}) && {b}", bool(a and b))
        case(f"identity({a}) || {b}", bool(a or b))
        case(f"{a} && identity({b})", bool(a and b))
        case(f"{a} || identity({b})", bool(a or b))
    case("0 && tick()", 0)
    case("1 || tick()", 1)
    case("calls", 0)
    case("1 && tick()", 1)
    case("0 || tick()", 1)
    case("calls", 2)

    # Literals can start an expression which still emits code.
    case("p[1 + identity(1)]", 33)
    case("*(&items[identity(1)])", 22)
    case("*(&box.values[2])", 33)
    case("*(&box.values[identity(1)])", 22)
    case("*(&links.numbers[identity(2)])", 33)
    case("*(&links.child.values[2])", 33)
    case("local_index(79)", 1234)
    case("local_byte(139)", 201)
    case("address_delta(wp + identity(2))", 26)
    case("address_delta(identity(2) + wp)", 26)
    case("assign_index()", 45)
    case("prefix_index()", 12)
    case("local_assign()", 201)
    case("far_local()", 1234)
    case("(int)&box.values[16384] - (int)&box.values[0]", 32768)
    case("zero_product()", 2)
    case("compound_index()", 1414)
    case("postfix_cell()", 304)
    case("chained()", 18)
    case("char_store()", -5)
    case("shift_assign()", -6)
    case("mod_assign()", -2)
    case("far_compound()", 26)
    case("sum_va(3, 10, 20, 30)", 60)
    case("va_assign(1, 2)", 6)
    case("zero_byte_chain()", 1)
    case("zero_local_chain()", 1)
    case("byte_assign_value()", 2)
    case("byte_copy_value()", 77)
    case("char_ptr_value()", -3)
    case("mod_byte_value()", 7)
    case("return_global_postinc()", 5)
    case("return_local_postinc()", 5)
    case("early_global_postinc(1)", 5)
    case("early_local_postinc(1)", 5)
    case("shifted_arg()", -16)
    case("out_port_shift(6)", 4660)
    case("out_port_call(6)", 4660)

    for a in (-32768, -257, -17, -8, -1, 0, 1, 8, 17, 257, 32767):
        for b in (-17, -3, 3, 17):
            product = abs(a * b) // 16 * (-1 if a * b < 0 else 1)
            numerator = abs(a) * 16
            quotient = (numerator // abs(b) +
                        (2 * (numerator % abs(b)) >= abs(b)))
            quotient *= -1 if (a < 0) != (b < 0) else 1
            for left in (f"(fixed)({a})", f"fixed_identity((fixed)({a}))"):
                case(f"(int)({left} * (fixed)({b}))", product)
                case(f"(int)({left} / (fixed)({b}))", quotient)

    # Compares with a constant operand (the compiler mirrors x>c and x<=c) and
    # compares used as conditions (ZOPT branches on the routine's flags).
    compare_ops = (
        ("<", lambda x, y: x < y), ("<=", lambda x, y: x <= y),
        (">", lambda x, y: x > y), (">=", lambda x, y: x >= y),
        ("==", lambda x, y: x == y), ("!=", lambda x, y: x != y),
    )
    for a in (-32768, -30000, -257, -7, -1, 0, 1, 7, 255, 30000, 32767):
        for b in (-32768, -1, 0, 1, 32767):
            for op, fn in compare_ops:
                case(f"identity({a}) {op} {b}", fn(a, b))
                case(f"{a} {op} identity({b})", fn(a, b))
                if a in (-7, 0, 7):
                    case(f"identity({a}) {op} {b} ? 5 : 9", 5 if fn(a, b) else 9)
    for a in (0, 1, 32767, 32768, 65535):
        for b in (0, 1, 32767, 32768, 65535):
            for op, fn in compare_ops:
                case(f"unsigned_identity({a}) {op} {b}", fn(a, b))
                case(f"{a} {op} unsigned_identity({b})", fn(a, b))
                case(f"unsigned_identity({a}) {op} unsigned_identity({b})", fn(a, b))
                if a in (0, 32768):
                    case(f"unsigned_identity({a}) {op} {b} ? 5 : 9", 5 if fn(a, b) else 9)
    # (fixed)(n) reinterprets n as raw Q12.4 bits; plain ints are scaled by 16.
    for a in (-8, -1, 0, 1, 8, 48):
        for b in (-3, 0, 3, 48):
            for op, fn in compare_ops:
                case(f"fixed_identity((fixed)({a})) {op} (fixed)({b})", fn(a, b))
                case(f"fixed_identity((fixed)({a})) {op} {b}", fn(a, b * 16))
                case(f"identity({a}) {op} (fixed)({b})", fn(a * 16, b))

    with tempfile.TemporaryDirectory(prefix="znc-codegen-") as directory:
        work = Path(directory)
        # Generate includes from the actual RTL, not potentially stale build output.
        generated = ROOT / "RTL" / "generated"
        generated.mkdir(exist_ok=True)
        for source in (ROOT / "RTL").glob("*.asm"):
            output = work / (source.stem + ".inc")
            run([os.sys.executable, str(ROOT / "tools" / "rtlgenerate.py"),
                 str(source), str(output)], ROOT)
            target = generated / output.name
            if not target.exists() or target.read_bytes() != output.read_bytes():
                shutil.copyfile(output, target)
        sources = [str(p) for p in ROOT.glob("*.c") if p.name != "farcall.c"]
        compiler = work / "znc-host.exe"
        run([cc, "-std=c99", "-O2", "-I", str(ROOT / "tests" / "host"),
             "-I", str(ROOT), *sources, str(ROOT / "tests" / "host" / "platform.c"),
             "-o", str(compiler)], ROOT)

        failures = []
        # Bound code/data size and keep the result buffer away from generated code.
        for start in range(0, len(cases), 100):
            failures.extend(execute(cases[start:start + 100], compiler, assembler, ticks, work))
        if failures:
            print("\n".join(failures[:40]))
            raise AssertionError(f"{len(failures)} / {len(cases)} execution checks failed")
        print(f"PASS: {len(cases)} generated-code execution checks")
        for expression in ("1 / 0", "identity(1) / 0", "1 % 0", "1.0 / 0.0"):
            (work / "zero.znc").write_text(
                f"int identity(int x) {{ return x; }}\nint x = {expression};\n")
            result = command([str(compiler), "zero.znc", "zero.asm"], work)
            if result.returncode == 0 or "division by zero" not in result.stdout:
                raise AssertionError(f"Expected division-by-zero diagnostic for {expression}")
        print("PASS: constant division-by-zero diagnostics")
        check_emitters(cc, sources, assembler, ticks, work)
        for library in (ROOT / "znclib").iterdir():
            if library.is_file():
                shutil.copyfile(library, work / library.name)
        for name in ("feature_test", "local_arrays", "pointer_arith", "struct_test", "incdec"):
            shutil.copyfile(ROOT / "tests" / (name + ".znc"), work / (name + ".znc"))
            run([str(compiler), name + ".znc", "smoke.asm"], work)
        print("PASS: existing feature, array, pointer, struct and inc/dec compilation tests")


if __name__ == "__main__":
    main()
