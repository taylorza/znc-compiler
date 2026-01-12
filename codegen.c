#include <stdarg.h>

#include "znc.h"

uint16_t nxtlbl = 0;    // label counter
uint16_t argcntlbl = 0; // label for the argcount for the current function

char buf[64];

#ifdef __ZXNEXT
uint8_t asm_fh = 0;
#else
FILE *asm_fh = NULL;
#endif

#ifndef __ZXNEXT
#undef MAX_WRITE_BUF
#define MAX_WRITE_BUF 1
#endif

char asmbuf[MAX_WRITE_BUF];
uint8_t pos;

uint8_t asm_open(const char *asmfilename) MYCC {
    pos = 0;
#ifdef __ZXNEXT
    errno = 0;
    asm_fh = esx_f_open(asmfilename, ESX_MODE_OPEN_CREAT_TRUNC | ESX_MODE_W);
    if (errno) return 0;
#else
    if (!asmfilename) {
        asm_fh = stdout;     
    }
    else {
        asm_fh = fopen(asmfilename, "wb");
        if (!asm_fh) return 0;
    }
#endif
    return 1;
}

void asm_close(void) MYCC {
    
    if (pos) {
#ifdef __ZXNEXT
        zx_border(asmbuf[0] & 1);
        esx_f_write(asm_fh, asmbuf, pos);
#else
        fwrite(asmbuf, 1, pos, asm_fh);
#endif
        pos = 0;
    }

    if (asm_fh) {
#ifdef __ZXNEXT
        esx_f_close(asm_fh);
#else        
        if (asm_fh != stdout) fclose(asm_fh);
#endif
    }
}

void asm_putc(char c) MYCC {
    if (pos == MAX_WRITE_BUF) {
#ifdef __ZXNEXT
        zx_border(asmbuf[0] & 1);
        esx_f_write(asm_fh, asmbuf, pos);        
#else
        fwrite(asmbuf, 1, pos, asm_fh);
#endif
        pos = 0;
    }
    asmbuf[pos++] = c;
}

static void asm_puts(const char *s) MYCC {
    while (*s) asm_putc(*s++);
}

uint16_t newlbl(void) MYCC {
    return ++nxtlbl;
}


void emit_nl(void) MYCC {
    asm_putc(NL);
}

void emit_ch(char c) MYCC {
    asm_putc(c);
}

void emit_lbl(uint16_t lbl) MYCC {
    emit_strln("lbl%d", lbl);    
}

void emit_lblref(uint16_t lbl) MYCC {
    emit_str("lbl%d", lbl);
}

void emit_str(const char *fmt, ...) MYCC {
    va_list v;
    va_start(v, fmt);
    vsnprintf(buf, sizeof(buf), (char *)fmt, v);
    va_end(v);
    asm_puts(buf);
}

void emit_strln(const char* fmt, ...) MYCC {
    va_list v;
    va_start(v, fmt);
    vsnprintf(buf, sizeof(buf), (char *)fmt, v);
    va_end(v);
    asm_puts(buf);
    emit_nl();
}

void emit_sname(const char *name) MYCC {
    emit_str("_%s", name);    
}

void emit_strref(uint16_t id) MYCC {
    emit_str("str+%d", id);
}

void emit_instr(const char * fmt, ...) MYCC {
    va_list v;
    va_start(v, fmt);
    vsnprintf(buf, sizeof(buf), (char *)fmt, v);
    va_end(v);
    asm_puts("  "); asm_puts(buf);
}

void emit_instrln(const char* fmt, ...) MYCC {
    va_list v;
    va_start(v, fmt);
    vsnprintf(buf, sizeof(buf), (char *)fmt, v);
    va_end(v);
    asm_puts("  "); asm_puts(buf);
    emit_nl();
}

void emit_n(uint16_t n) MYCC {
    emit_str("%d", n);
}

void emit_ld_immed(void) MYCC {
    emit_instr("ld hl,");
}

void emit_ldbc_immed(void) MYCC {
    emit_instr("ld bc,");
}

void emit_ldde_immed(void) MYCC {
    emit_instr("ld de,");
}

void emit_ld_immed_n(uint16_t n) MYCC {
    emit_ld_immed();
    emit_n(n);
    emit_nl();
}

void emit_ldbc_immed_n(uint16_t n) MYCC {
    emit_ldbc_immed();
    emit_n(n);
    emit_nl();
}

void emit_ldde_immed_n(uint16_t n) MYCC {
    emit_ldde_immed();
    emit_n(n);
    emit_nl();
}

/* Optimized addition of small constants to HL */
void emit_add_hl_small(int16_t n) MYCC {
    if (n == 0) {
        return;
    }
    
    const char *instr = (n > 0) ? "inc hl" : "dec hl";
    int8_t count = (n > 0) ? n : -n;
    
    if (count <= 3) {
        while (count--) {
            emit_instrln(instr);
        }
    } else {
        emit_ldde_immed_n((uint16_t)n);
        emit_add16();
    }
}

/* Optimized multiplication by small constants - using BC for temps instead of stack */
void emit_mul_const_optimized(uint16_t factor) MYCC {
    switch (factor) {
        case 0:
            emit_ld_immed_n(0);
            break;
        case 1:
            /* HL unchanged */
            break;
        case 3:
            /* a * 3 = a * 2 + a - use BC to save original */
            emit_copy_hl_to_bc();     /* BC = original */
            emit_mul2();              /* HL = HL * 2 */
            emit_instrln("add hl,bc"); /* HL = HL*2 + original */
            break;
        case 5:
            /* a * 5 = a * 4 + a - use BC to save original */
            emit_copy_hl_to_bc();     /* BC = original */
            emit_mul2();              /* HL = HL * 2 */
            emit_mul2();              /* HL = HL * 4 */
            emit_instrln("add hl,bc"); /* HL = HL*4 + original */
            break;
        case 6:
            /* a * 6 = a * 2 + a * 4 - use BC to save HL*2 */
            emit_mul2();              /* HL = HL * 2 */
            emit_copy_hl_to_bc();     /* BC = HL * 2 */
            emit_mul2();              /* HL = HL * 4 */
            emit_instrln("add hl,bc"); /* HL = HL*4 + HL*2 = HL*6 */
            break;
        case 7:
            /* a * 7 = a * 8 - a - use BC to save original */
            emit_copy_hl_to_bc();     /* BC = original */
            emit_mul2();              /* HL * 2 */
            emit_mul2();              /* HL * 4 */
            emit_mul2();              /* HL * 8 */
            emit_instrln("xor a");    /* Clear carry */
            emit_instrln("sbc hl,bc"); /* HL = HL*8 - original */
            break;
        case 9:
            /* a * 9 = a * 8 + a - use BC to save original */
            emit_copy_hl_to_bc();     /* BC = original */
            emit_mul2();              /* HL * 2 */
            emit_mul2();              /* HL * 4 */
            emit_mul2();              /* HL * 8 */
            emit_instrln("add hl,bc"); /* HL = HL*8 + original */
            break;
        case 10:
            /* a * 10 = a * 8 + a * 2 - use BC to save HL*2 */
            emit_mul2();              /* HL = HL * 2 */
            emit_copy_hl_to_bc();     /* BC = HL * 2 */
            emit_mul2();              /* HL = HL * 4 */
            emit_mul2();              /* HL = HL * 8 */
            emit_instrln("add hl,bc"); /* HL = HL*8 + HL*2 = HL*10 */
            break;
        case 12:
            /* a * 12 = a * 4 * 3 = (a*4)*2 + a*4 */
            emit_mul2();              /* HL * 2 */
            emit_mul2();              /* HL * 4 */
            emit_copy_hl_to_bc();     /* BC = HL * 4 */
            emit_mul2();              /* HL * 8 */
            emit_instrln("add hl,bc"); /* HL = HL*8 + HL*4 = HL*12 */
            break;
        default:
            /* Check if power of two */
            if ((factor & (factor - 1)) == 0) {
                uint16_t temp = factor;
                while (temp > 1) {
                    emit_mul2();
                    temp >>= 1;
                }
            } else {
                /* Use generic multiplication */
                emit_ldde_immed_n(factor);
                emit_rtl("ccmult");
            }
            break;
    }
}

void emit_load_word_from_hl(void) MYCC {
    emit_instrln("ld e,(hl)");
    emit_instrln("inc hl");
    emit_instrln("ld d,(hl)");
    emit_instrln("ex de,hl");
}

void emit_store_word_at_de(void) MYCC {
    emit_instrln("ld a,l");
    emit_instrln("ld (de),a");
    emit_instrln("inc de");
    emit_instrln("ld a,h");
    emit_instrln("ld (de),a");
}

void emit_store_byte_at_de(void) MYCC {
    emit_instrln("ld a,l");
    emit_instrln("ld (de),a");
}

void emit_copy_hl_to_bc(void) MYCC {
    emit_instrln("ld b,h");
    emit_instrln("ld c,l");
}

void emit_copy_bc_to_hl(void) MYCC {
    emit_instrln("ld h,b");
    emit_instrln("ld l,c");
}

/* Helper: Copy IX register to HL */
void emit_copy_ix_to_hl(void) MYCC {
    emit_instrln("push ix");
    emit_instrln("pop hl");
}

void emit_push(void) MYCC {
    emit_instrln("push hl");
}

void emit_pop_de(void) MYCC {
    emit_instrln("pop de");
}

void emit_pop_hl(void) MYCC {
    emit_instrln("pop hl");
}

void emit_swap(void) MYCC {
    emit_instrln("ex de,hl");
}

void emit_add16(void) MYCC {
    emit_instrln("add hl,de");
}

void emit_sub16(void) MYCC {
    emit_swap();
    emit_instrln("xor a");
    emit_instrln("sbc hl,de");
}

void emit_rtl(const char* name) MYCC {
    /* Copy name into a small main-bank buffer so callers from other banks
     * can pass string literals that live in their bank without causing
     * device MMU deref issues when inc_rtl switches bank.
     */
    char tmp[32];
    strncpy(tmp, name, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = '\0';

    if ((inc_rtl(tmp) & FLAG_RTL_INLINE) == 0) {
        emit_call(tmp);
    }
}

void emit_call(const char *name) MYCC {
    emit_instrln("call %s", name);    
}

void emit_callsym(SYMBOL* sym) MYCC {
    if (is_func_or_proto(sym)) {
        emit_instr("call ");
        emit_sname(sym->name);
        emit_nl();
    } else {
        uint16_t retlbl = newlbl(); 
        emit_ld_symval(sym);
        emit_instrln("ld bc,lbl%d", retlbl);
        emit_instrln("push bc");
        emit_push();
        emit_ret();
        emit_lbl(retlbl);
    }
}

void emit_ret(void) MYCC {
    emit_instrln("ret");
}

void emit_jp(uint16_t lbl) MYCC {
    emit_instr("jp ");
    emit_lblref(lbl); emit_nl();
}

void emit_jp_true(uint16_t lbl) MYCC {
    emit_instrln("ld a,h");
    emit_instrln("or l");
    emit_instr("jp nz,");
    emit_lblref(lbl); emit_nl();
}

void emit_jp_false(uint16_t lbl) MYCC {
    emit_instrln("ld a,h");
    emit_instrln("or l");
    emit_instr("jp z,");
    emit_lblref(lbl); emit_nl();
}

uint16_t emit_alloclocals(void) MYCC {
    uint16_t lbl = newlbl();
    emit_ld_immed(); emit_str("-"); emit_lblref(lbl); emit_nl();
    emit_instrln("add hl,sp");
    emit_instrln("ld sp,hl");
    return lbl;
}
void emit_lblequ16(uint16_t lbl, uint16_t value) MYCC {
    emit_lblref(lbl); emit_str(" equ "); emit_n(value); emit_nl();
}

/* Helper: Check if offset pair is in IX+d range */
static inline uint8_t offsets_in_ix_range(int16_t low, int16_t high) MYCC {
    return (low >= -128 && low <= 127 && high >= -128 && high <= 127);
}

/* Helper: Compute local variable offsets for load/store operations */
static void compute_local_offsets(SYMBOL *sym, int16_t *low_off, int16_t *high_off) MYCC {
    int8_t bp_offset;
    
    if (sym->klass == VARIABLE) {
        bp_offset = (uint8_t)sym->offset;
        uint16_t var_size = type_size(&sym->type);
        *low_off = -(bp_offset + var_size);
        *high_off = -(bp_offset + var_size - 1);
    } else { /* ARGUMENT */
        bp_offset = 2 + (func_argcount - sym->offset) * 2;
        *low_off = bp_offset;
        *high_off = bp_offset + 1;
    }
}

/* Helper: Emit code to compute address in HL when offsets out of range */
static void emit_compute_ix_address(int16_t offset) MYCC {
    emit_instrln("ld hl,%d", offset);
    emit_instrln("ld d,ixh");
    emit_instrln("ld e,ixl");
    emit_instrln("add hl,de");
}

void emit_ld_symval(SYMBOL* sym) MYCC {
    TYPEREC* ptype = &sym->type;

    if (sym->scope == GLOBAL) {
        if (is_array(ptype)) {
            emit_ld_immed();
            emit_sname(sym->name);
            emit_nl();
        }
        else if (!is_ptr(ptype) && is_char(ptype)) {
            emit_instr("ld a,(");
            emit_sname(sym->name);
            emit_ch(')');
            emit_nl();
            emit_rtl("ccsxt");
        }
        else {
            emit_instr("ld hl,(");
            emit_sname(sym->name);
            emit_ch(')');
            emit_nl();
        }
    }
    else if (sym->scope == LOCAL) {
        if (sym->klass == VARIABLE) {
            int8_t bp_offset = (uint8_t)sym->offset;
            
            if (is_array(ptype) || is_struct(ptype)) {
                /* Arrays and structs: load their address */
                int16_t addr_offset = bp_offset - type_size(ptype);
                
                if (addr_offset >= -128 && addr_offset <= 127) {
                    emit_copy_ix_to_hl();
                    if (addr_offset != 0) {
                        emit_add_hl_small(addr_offset);
                    }
                } else {
                    emit_compute_ix_address(addr_offset);
                }
            } else if (!is_ptr(ptype) && is_char(ptype)) {
                /* Char/byte scalar: load single byte and sign-extend */
                int16_t low_off = -(bp_offset + 1);
                
                if (low_off >= -128 && low_off <= 127) {
                    emit_instrln("ld a,(ix%+d)", low_off);
                    emit_rtl("ccsxt");
                } else {
                    emit_compute_ix_address(low_off);
                    emit_instrln("ld a,(hl)");
                    emit_rtl("ccsxt");
                }
            } else {
                /* Int scalar/pointer: load 2-byte value */
                int16_t low_off, high_off;
                compute_local_offsets(sym, &low_off, &high_off);
                
                if (offsets_in_ix_range(low_off, high_off)) {
                    emit_instrln("ld l,(ix%+d)", low_off);
                    emit_instrln("ld h,(ix%+d)", high_off);
                } else {
                    emit_compute_ix_address(low_off);
                    emit_load_word_from_hl();
                }
            }
        } else { /* ARGUMENT */
            int16_t low_off, high_off;
            compute_local_offsets(sym, &low_off, &high_off);
            
            if (offsets_in_ix_range(low_off, high_off)) {
                emit_instrln("ld l,(ix+%d)", low_off);
                emit_instrln("ld h,(ix+%d)", high_off);
            } else {
                emit_compute_ix_address(low_off);
                emit_load_word_from_hl();
            }
        }
    }
}

void emit_ld_symaddr(SYMBOL* sym) MYCC {
    emit_ld_symaddr_offset(sym, 0);
}

void emit_ld_symaddr_offset(SYMBOL* sym, uint16_t offset) MYCC {
    TYPEREC* ptype = &sym->type;

    if (sym->scope == GLOBAL) {
        emit_ld_immed();
        emit_sname(sym->name);
        if (offset) { emit_ch('+'); emit_n(offset); }
        emit_nl();
    }
    else if (sym->scope == LOCAL) {
        int16_t bp_offset = 0;
        if (sym->klass == VARIABLE) {
            bp_offset = (uint8_t)sym->offset;
            /* Arrays and structs: address is at base - size */
            if (is_array(ptype) || is_struct(ptype))
                bp_offset = (bp_offset - type_size(ptype));
            else {
                /* Scalars and pointers: stored value, compute address to low byte */
                uint16_t var_size = type_size(ptype);
                bp_offset = (bp_offset - var_size) + 1;
            }
        }
        else if (sym->klass == ARGUMENT) {
            bp_offset = 2 + (func_argcount - sym->offset) * 2;            
        }
        
        /* Fold in any additional offset BEFORE range checking */
        bp_offset += (int16_t)offset;
        
        /* OPTIMIZATION: If final offset is in IX+d range, compute optimally */
        if (bp_offset >= -128 && bp_offset <= 127) {
            if (bp_offset == 0) {
                /* Special case: offset 0 - just copy IX */
                emit_copy_ix_to_hl();
            } else if (bp_offset >= -3 && bp_offset <= 3) {
                /* Small offset: copy IX and use inc/dec */
                emit_copy_ix_to_hl();
                emit_add_hl_small(bp_offset);
            } else {
                /* Medium offset: still in range, use add */
                emit_copy_ix_to_hl();
                emit_ldde_immed_n((uint16_t)bp_offset);
                emit_add16();
            }
        } else {
            /* Out of range: compute manually */
            emit_instrln("ld hl,%d", bp_offset);
            emit_instrln("ld d,ixh");
            emit_instrln("ld e,ixl");
            emit_instrln("add hl,de");
        }
    }
}

void emit_ld_const(uint16_t value) MYCC {
    emit_ld_immed_n(value);
}

void emit_store_sym(SYMBOL* sym) MYCC {
    TYPEREC* ptype = &sym->type;
    if (is_array(ptype)) error(errNotlvalue);
    
    if (sym->scope == GLOBAL) {
        if (!is_ptr(ptype) && is_char(ptype)) {
            emit_instrln("ld a,l");
            emit_instr("ld ("); emit_sname(sym->name); emit_strln("),a");
        }
        else {
            emit_instr("ld ("); emit_sname(sym->name); emit_strln("),hl");
        }
    }
    else if (sym->scope == LOCAL) {
        if (sym->klass == VARIABLE && !is_ptr(ptype) && is_char(ptype)) {
            /* Char/byte scalar: store single byte */
            int8_t bp_offset = (uint8_t)sym->offset;
            int16_t low_off = -(bp_offset + 1);
            
            if (low_off >= -128 && low_off <= 127) {
                emit_instrln("ld (ix%+d),l", low_off);
            } else {
                emit_push();
                emit_compute_ix_address(low_off);
                emit_swap();
                emit_pop_de();
                emit_store_byte_at_de();
            }
        } else {
            /* Int scalar/pointer or argument: store 2-byte value */
            int16_t low_off, high_off;
            compute_local_offsets(sym, &low_off, &high_off);
            
            if (offsets_in_ix_range(low_off, high_off)) {
                if (sym->klass == VARIABLE) {
                    emit_instrln("ld (ix%+d),l", low_off);
                    emit_instrln("ld (ix%+d),h", high_off);
                } else { /* ARGUMENT */
                    emit_instrln("ld (ix+%d),l", low_off);
                    emit_instrln("ld (ix+%d),h", high_off);
                }
            } else {
                emit_push();
                emit_compute_ix_address(low_off);
                emit_swap();
                emit_pop_de();
                emit_store_word_at_de();
            }
        }
    }
}

void emit_store(TYPEREC type) MYCC {
    emit_pop_de();      // target address
    if (is_void(&type) || is_char(&type)) {
        emit_store_byte_at_de();
    } else {
        emit_store_word_at_de();
    }
} 

void emit_load(TYPEREC type) MYCC {
    /* Load the value pointed to by HL. If the base type is char (including
     * pointers to char), load a byte and sign-extend. Otherwise load a 16-bit
     * value (word) from memory.
     */
    if (is_char(&type)) {
        emit_instrln("ld a,(hl)");
        emit_rtl("ccsxt");
    } else {
        emit_load_word_from_hl();
    }
}

void emit_nreg_immed(uint8_t reg, uint8_t val) MYCC {
    emit_instrln("nreg %d,%d", reg, val);
}

void emit_nreg_A(uint8_t reg) MYCC {
    emit_instrln("ld a,l");
    emit_instrln("nreg %d,a", reg);
}

void emit_frame_prologue(uint8_t toplevel, uint16_t exit_lbl) MYCC {
    emit_instrln("push ix");
    
    if (toplevel) {
        emit_instrln("ld (lbl%d+1), sp", exit_lbl);
        emit_instrln("ld ix,(lbl%d+1)", exit_lbl);
    }
    else {
        emit_instrln("ld ix,0");
        emit_instrln("add ix,sp");
    }
}

void emit_frame_epilogue(uint8_t toplevel, uint16_t exit_lbl) MYCC {
    emit_lbl(exit_lbl);
    if (toplevel) {
        emit_instrln("ld sp,0");
    } else {
        emit_instrln("ld sp,ix");
    }    
    emit_instrln("pop ix");
    emit_ret();
}

void emit_neg(void) MYCC {
    emit_rtl("ccneg");
}

void emit_mul2(void) MYCC {
    emit_instrln("add hl,hl");
}

void emit_mulDE2(void) MYCC {
    emit_instrln("ld b,2");
    emit_instrln("bsla de,b");
}

void emit_org(uint16_t address) MYCC {
    emit_instrln("org %d", address);
}

void emit_bank(uint8_t bank, uint16_t offset) MYCC {
    emit_instrln("bank %d,%d", bank, offset);
}

void emit_output(const char* filename, TOKEN outputTok) MYCC {
    emit_strln("  output \"%s\"", filename);
    if (outputTok == tokDot) {
        emit_org(0x2000);
        TYPEREC str = { .basetype = CHAR, .dim = -80 };
        addglb("args", VARIABLE, str, 0);
        emit_rtl("ldcmdln");
    }
}

void emit_nex(const char* filename, uint16_t start, uint16_t stack, uint16_t stacksize) MYCC {
    emit_instrln("ds %d", stacksize);
    emit_lbl(stack);
    emit_instrln("equ $");
    emit_instr("savenex \"%s\",", filename);
    emit_lblref(start); emit_ch(',');
    emit_lblref(stack);
}

void emit_zopt(void) {
    emit_strln(";#ZOPT");
}