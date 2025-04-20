OUTPUT_DIR = output

TARGET = +zxn

ZCC     = zcc
ASM     = z80asm

MAX_ALLOCS = 200000
CFLAGS = -m -c -clib=sdcc_iy -SO3 -opt-code-size --max-allocs-per-node$(MAX_ALLOCS) -pragma-include:zpragma.inc
AFLAGS =
LFLAGS = -m -startup=30 -clib=sdcc_iy -subtype=dotn -SO3 --max-allocs-per-node$(MAX_ALLOCS) -pragma-include:zpragma.inc -create-app

SOURCES = codegen.c compiler.c dataarea.c error.c expr.c main.c rtl.c scanner.c strtbl.c sym.c util.c

OBJFILES = $(patsubst %.c,$(OUTPUT_DIR)/%.o,$(SOURCES))

TARGET_BIN = znc

.PHONY: all compile assemble clean

all: compile link

$(OUTPUT_DIR):
	mkdir $(OUTPUT_DIR)

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
	@echo "Clean complete."
