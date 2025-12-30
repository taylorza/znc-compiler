OUTPUT_DIR = output
RTL_DIR = RTL/generated

TARGET = +zxn

ZCC     = zcc
ASM     = z80asm

MAX_ALLOCS = 200000
CFLAGS = -m -c -clib=sdcc_iy -SO3 -opt-code-size --max-allocs-per-node$(MAX_ALLOCS)
AFLAGS =
LFLAGS = -m -startup=30 -clib=sdcc_iy -subtype=dotn -SO3 -opt-code-size --max-allocs-per-node$(MAX_ALLOCS) -pragma-include:zpragma.inc -create-app

SOURCES = strtbl_stub.c sym_stub.c rtl_stub.c type.c strtbl.c codegen.c compiler.c dataarea.c error.c expr.c main.c rtl.c scanner.c sym.c util.c 

OBJFILES = $(patsubst %.c,$(OUTPUT_DIR)/%.o,$(SOURCES))

# Generate RTL .inc headers from plain RTL .asm files
PY = python

RTL_ASMS = $(wildcard RTL/*.asm)
RTL_INCS = $(patsubst RTL/%.asm,RTL/generated/%.inc,$(RTL_ASMS))

TARGET_BIN = znc

.PHONY: all rtlincs compile assemble clean link

all: rtlincs compile link

rtlincs: $(RTL_DIR) $(RTL_INCS)

$(RTL_DIR):
	mkdir RTL\generated

$(OUTPUT_DIR):
	mkdir $(OUTPUT_DIR)

$(RTL_DIR)/%.inc: RTL/%.asm tools/rtlgenerate.py
	$(PY) tools/rtlgenerate.py $< $@

$(OUTPUT_DIR)/strtbl.o: strtbl.c | $(OUTPUT_DIR)
	@echo "Compiling BANK 40"
	$(ZCC) $(TARGET) $(CFLAGS) $< -o $@ --datasegBANK_40 --codesegBANK_40 --constsegBANK_40 --bsssegBANK_40

$(OUTPUT_DIR)/sym.o: sym.c | $(OUTPUT_DIR)
	@echo "Compiling BANK 41"
	$(ZCC) $(TARGET) $(CFLAGS) $< -o $@ --datasegBANK_41 --codesegBANK_41 --constsegBANK_41 --bsssegBANK_41

$(OUTPUT_DIR)/rtl.o: rtl.c $(RTL_INCS) | $(OUTPUT_DIR)
	@echo "Compiling BANK 42"
	$(ZCC) $(TARGET) $(CFLAGS) $< -o $@ --datasegBANK_42 --codesegBANK_42 --constsegBANK_42 --bsssegBANK_42
	@echo "-> Generated $@"

$(OUTPUT_DIR)/dataarea.o: dataarea.c | $(OUTPUT_DIR)
	@echo "Compiling (YES) $<"
	$(ZCC) $(TARGET) $(CFLAGS) $< -o $@ --datasegcode_l --codesegcode_l --constsegcode_l --bsssegcode_l
	@echo "-> Generated $@"

$(OUTPUT_DIR)/error.o: error.c | $(OUTPUT_DIR)
	@echo "Compiling (YES) $<"
	$(ZCC) $(TARGET) $(CFLAGS) $< -o $@ --datasegcode_l --codesegcode_l --constsegcode_l --bsssegcode_l
	@echo "-> Generated $@"

$(OUTPUT_DIR)/%.o: %.c | $(OUTPUT_DIR)
	@echo "Compiling $<"
	$(ZCC) $(TARGET) $(CFLAGS) $< -o $@
	@echo "-> Generated $@"

compile: $(OBJFILES)
	@echo "Compilation complete."

$(TARGET_BIN): $(OBJFILES)
	@echo "Linking into $(TARGET_BIN)..."
	$(ZCC) $(TARGET) $(LFLAGS) -o$(TARGET_BIN) $(OBJFILES)
	@echo "-> Created $(TARGET_BIN)"

link: $(TARGET_BIN)
	@echo "Linker complete."

clean:
	@echo "Cleaning generated files..."
	rm -rf $(OUTPUT_DIR) $(TARGET_BIN)
	rm -rf *.bin
	@echo "Clean complete."
