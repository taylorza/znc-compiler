# Command-line Usage & Build Workflow

This page describes the typical command-line workflow for compiling, optimizing, and assembling ZNC programs. It focuses on the DOT commands used on the NextZXOS environment.

## Overview

- The compiler (`.znc`) translates `.znc` source into a generated assembly file (`.asm`) and supporting `.rtl` includes.
- `zopt` is a rule-driven peephole optimizer that operates on the generated assembly and can optimize some common code patterns.
- The assembler (`.asm`) converts the assembly to the final binary (DOT command or other formats).

## 1) Compile

To compile a `.znc` source file and enable dead-function-elimination marker emission, pass `-dfe` to the compiler:

```
.znc -dfe demo.znc
```

Output:
- `demo.asm` — generated assembly
- `*.rtl` — any runtime helper includes

With `-dfe` the compiler will do some basic tracking of function usage and flag unsed functions for elimination during assembly. The assembler will remove any function blocks that are marked as unused by the compiler.

## 2) Optimize with ZOPT

Run the peephole optimizer against the generated assembly. By default `zopt` uses `rules.opt` as the rule file:

```
.zopt demo.asm            # use default rules.opt
.zopt myrules.opt demo.asm  # use a custom rules file
```

`zopt` operates in-place (it writes a temporary file then replaces the input).

The optimizer supports `OPT_OFF`/`OPT_ON` directives and a rich rule language in `rules.opt` for matching and replacing instruction sequences.

## 3) Assemble

After optimizing, assemble the final `.asm` to the binary using the assembler DOT command:

```
.asm demo.asm
```

## Example pipeline

```
.znc -dfe demo.znc
.zopt demo.asm
.asm demo.asm
```

## Notes

- Default ZOPT rule file: `rules.opt` (in current/search path). Provide a custom rule file to change or extend optimizer behaviour.
- `zopt` replaces the input file; if you need to keep the original generated assembly, copy `demo.asm` before running `.zopt`.
- `-dfe` requires enabling the option when invoking the compiler to enable the dead code elimination optimization.
