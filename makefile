OUTPUT_DIR = output

TARGET = +zxn

ZCC     = zcc
ASM     = z80asm

MAX_ALLOCS = 200000
CFLAGS = -m -c -clib=sdcc_iy -SO3 -opt-code-size --max-allocs-per-node$(MAX_ALLOCS)
AFLAGS =
LFLAGS = -m -startup=30 -clib=sdcc_iy -subtype=dotn -SO3 -opt-code-size --max-allocs-per-node$(MAX_ALLOCS) -pragma-include:zpragma.inc -create-app

SOURCES = farcall.c type.c strtbl.c strtbl_s.asm codegen.c compiler.c dataarea.c error.c expr.c main.c rtl.c scanner.c sym.c sym_s.asm util.c 

OBJFILES = $(patsubst %.c,$(OUTPUT_DIR)/%.o,$(SOURCES))

TARGET_BIN = znc

.PHONY: all compile assemble clean

all: compile link

$(OUTPUT_DIR):
	mkdir $(OUTPUT_DIR)

$(OUTPUT_DIR)/strtbl.o: strtbl.c | $(OUTPUT_DIR)
	@echo "Compiling BANK 40"
	$(ZCC) $(TARGET) $(CFLAGS) $< -o $@ --datasegBANK_40 --codesegBANK_40 --constsegBANK_40

$(OUTPUT_DIR)/sym.o: sym.c | $(OUTPUT_DIR)
	@echo "Compiling BANK 40"
	$(ZCC) $(TARGET) $(CFLAGS) $< -o $@ --datasegBANK_41 --codesegBANK_41 --constsegBANK_41

$(OUTPUT_DIR)/dataarea.o: dataarea.c | $(OUTPUT_DIR)
	@echo "Compiling (YES) $<"
	$(ZCC) $(TARGET) $(CFLAGS) $< -o $@ --datasegcode_l --codesegcode_l --constsegcode_l
	@echo "-> Generated $@"

$(OUTPUT_DIR)/error.o: error.c | $(OUTPUT_DIR)
	@echo "Compiling (YES) $<"
	$(ZCC) $(TARGET) $(CFLAGS) $< -o $@ --datasegcode_l --codesegcode_l --constsegcode_l
	@echo "-> Generated $@"

$(OUTPUT_DIR)/rtl.o: rtl.c | $(OUTPUT_DIR)
	@echo "Compiling (YES) $<"
	$(ZCC) $(TARGET) $(CFLAGS) $< -o $@ --datasegcode_l --codesegcode_l --constsegcode_l
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
