#ifndef CODEGEN_H_
#define CODEGEN_H_

uint8_t asm_open(const char *asmfilename) MYCC;
void asm_close(void) MYCC;

void asm_putc(char c) MYCC;

void emit_nl(void) MYCC;

uint16_t newlbl(void) MYCC;

void emit_str(const char* fmt, ...) MYCC;
void emit_strln(const char* fmt, ...) MYCC;

void emit_lbl(uint16_t lbl) MYCC;
void emit_lblref(uint16_t lbl) MYCC;

void emit_strref(uint16_t id) MYCC;

void emit_ch(char c) MYCC;

void emit_instr(const char* fmt, ...) MYCC;
void emit_instrln(const char* fmt, ...) MYCC;

void emit_n(uint16_t n) MYCC;

void emit_ld_immed(void) MYCC;
void emit_ldbc_immed(void) MYCC;
void emit_ldde_immed(void) MYCC;

void emit_push(void) MYCC;

void emit_pop(void) MYCC;

void emit_swap(void) MYCC;

void emit_add16(void) MYCC;

void emit_sub16(void) MYCC;

void emit_rtl(const char* name) MYCC;
void emit_call(const char *name) MYCC;
void emit_callsym(SYMBOL* sym) MYCC;
void emit_ret(void) MYCC;
void emit_jp(uint16_t lbl) MYCC;
void emit_jp_true(uint16_t lbl) MYCC;
void emit_jp_false(uint16_t lbl) MYCC;

void emit_sname(const char* name) MYCC;

void emit_ld_symval(SYMBOL* sym) MYCC;
void emit_ld_symaddr(SYMBOL* sym) MYCC;
void emit_store_sym(SYMBOL* sym) MYCC;
void emit_store(TYPEREC typ) MYCC;
void emit_load(TYPEREC typ) MYCC;

uint16_t emit_alloclocals(void) MYCC;
void emit_lblequ16(uint16_t lbl, uint16_t value) MYCC;

void emit_nreg_immed(uint8_t reg, uint8_t val) MYCC;
void emit_nreg_A(uint8_t reg) MYCC;

void emit_frame_prologue(void) MYCC;
void emit_frame_epilogue(void) MYCC;

void emit_neg(void) MYCC;
void emit_mul2(void) MYCC;
void emit_mulDE2(void) MYCC;

void emit_org(uint16_t address) MYCC;
void emit_bank(uint8_t bank16, uint16_t offset) MYCC;
void emit_output(const char* filename, TOKEN outputTok) MYCC;
void emit_nex(const char* filename, uint16_t start, uint16_t stack, uint16_t stacksize) MYCC;
void emit_ld_const(uint16_t value) MYCC;

void emit_zopt(void);

#endif //CODEGEN_H_