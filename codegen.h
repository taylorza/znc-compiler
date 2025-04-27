#ifndef CODEGEN_H_
#define CODEGEN_H_

uint8_t asm_open(const char *asmfilename);
void asm_close(void);

void asm_putc(char c);

void emit_nl(void);

uint16_t newlbl(void);

void emit_str(const char* fmt, ...);
void emit_strln(const char* fmt, ...);

void emit_lbl(uint16_t lbl);
void emit_lblref(uint16_t lbl);

void emit_strref(uint16_t id);

void emit_ch(char c);

void emit_instr(const char* fmt, ...);
void emit_instrln(const char* fmt, ...);

void emit_n(uint16_t n);

void emit_ld_immed(void);
void emit_ldbc_immed(void);

void emit_push(void);

void emit_pop(void);

void emit_swap(void);

void emit_add16(void);

void emit_sub16(void);

void emit_rtl(const char* name);
void emit_call(const char *name);
void emit_callsym(SYMBOL* sym);
void emit_ret(void);
void emit_jp(uint16_t lbl);
void emit_jp_true(uint16_t lbl);
void emit_jp_false(uint16_t lbl);

void emit_sname(const char* name);

void emit_ld_symval(SYMBOL* sym);
void emit_ld_symaddr(SYMBOL* sym);
void emit_store_sym(SYMBOL* sym);
void emit_store(TYPEREC typ);
void emit_load(TYPEREC typ);

uint16_t emit_alloclocals(void);
void emit_lblequ16(uint16_t lbl, uint16_t value);

void emit_nreg_immed(uint8_t reg, uint8_t val);
void emit_nreg_A(uint8_t reg);

void emit_frame_prologue(void);
void emit_frame_epilogue(void);

void emit_neg(void);
void emit_mul2(void);

void emit_org(uint16_t address);
void emit_bank(uint8_t bank16, uint16_t offset);
void emit_output(const char* filename, TOKEN outputTok);
void emit_nex(const char* filename, uint16_t start, uint16_t stack, uint16_t stacksize);

#endif //CODEGEN_H_