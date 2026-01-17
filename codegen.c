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
void emit_store_word_at_hl(void) MYCC {
    emit_instrln("ld (hl),e");
    emit_instrln("inc hl");
    emit_instrln("ld (hl),d");
}

void emit_store_byte_at_hl(void) MYCC {
    emit_instrln("ld (hl),e");
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

void emit_callsym(SYMBOL* sym, uint8_t ptr_in_hl) MYCC {
    if (is_func_or_proto(sym)) {
        emit_instr("call ");
        emit_sname(sym->name);
        emit_nl();
    } else {
        uint16_t retlbl = newlbl();
        /* Only load the symbol value if it's not already in HL */
        if (!ptr_in_hl) {
            emit_ld_symval(sym);
        }
        emit_instrln("ld bc,lbl%d", retlbl);
        emit_instrln("push bc");
        emit_instrln("jp (hl)");
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

/* Helper: Compute IX-relative base offset for a local/argument symbol
 * For local variables: returns -(offset + size), pointing to the low byte
 * For arguments: returns 2 + (func_argcount - offset) * 2, pointing to the low byte */
static int16_t compute_symbol_base_offset(SYMBOL *sym) MYCC {
    if (sym->klass == VARIABLE) {
        int8_t bp_offset = (uint8_t)sym->offset;
        uint16_t var_size = type_size(sym->type_id);
        return -(bp_offset + var_size);
    } else { /* ARGUMENT */
        return 2 + (func_argcount - sym->offset) * 2;
    }
}

/* Helper: Compute local variable offsets for load/store operations */
static void compute_local_offsets(SYMBOL *sym, int16_t *low_off, int16_t *high_off) MYCC {
    int16_t base = compute_symbol_base_offset(sym);
    *low_off = base;
    *high_off = base + 1;
}

/* Helper: Emit code to compute address in HL when offsets out of range */
static void emit_compute_ix_address(int16_t offset) MYCC {
    emit_instrln("ld hl,%d", offset);
    emit_instrln("ld d,ixh");
    emit_instrln("ld e,ixl");
    emit_instrln("add hl,de");
}


/* Helper: Emit code to load a variadic function argument value */
static void emit_load_variadic_arg(SYMBOL *sym) MYCC {
    uint8_t offset_factor = func_argcount - sym->offset;
    uint16_t constant_offset = 4 + 2 * offset_factor;
    emit_ldde_immed_n(constant_offset);
    emit_rtl("ccvafixed");
}

/* Helper: Emit code to load address of a variadic function argument */
static void emit_load_variadic_arg_addr(SYMBOL *sym) MYCC {
    uint8_t offset_factor = func_argcount - sym->offset;
    uint16_t constant_offset = 4 + 2 * offset_factor;
    emit_ldde_immed_n(constant_offset);
    emit_rtl("ccvafixedaddr");
}

/* Helper: Emit code to store to a variadic function argument */
static void emit_store_variadic_arg(SYMBOL *sym) MYCC {
    emit_push();
    emit_load_variadic_arg_addr(sym);
    emit_instrln("pop de");
    emit_instrln("ld (hl),e");
    emit_instrln("inc hl");
    emit_instrln("ld (hl),d");
}

void emit_ld_symval(SYMBOL* sym) MYCC {
    uint8_t type_id = sym->type_id;

    if (sym->scope == GLOBAL) {
        if (type_is_array(type_id)) {
            emit_ld_immed();
            emit_sname(sym->name);
            emit_nl();
        }
        else if (!type_is_pointer(type_id) && type_is_char(type_id)) {
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
        /* For variadic functions, ARGUMENT parameters need dynamic offset calculation */
        if (sym->klass == ARGUMENT && func_is_variadic) {
            emit_load_variadic_arg(sym);
            return;
        }
        
        if (sym->klass == VARIABLE) {
            if (type_is_array(type_id) || type_is_struct(type_id)) {
                /* Arrays and structs: load their address */
                int16_t addr_offset = compute_symbol_base_offset(sym);
                
                if (addr_offset >= -128 && addr_offset <= 127) {
                    emit_copy_ix_to_hl();
                    if (addr_offset != 0) {
                        emit_add_hl_small(addr_offset);
                    }
                } else {
                    emit_compute_ix_address(addr_offset);
                }
                return;
            }
            
            if (!type_is_pointer(type_id) && type_is_char(type_id)) {
                /* Char/byte scalar: load single byte and sign-extend */
                int16_t low_off = compute_symbol_base_offset(sym);
                
                if (low_off >= -128 && low_off <= 127) {
                    emit_instrln("ld a,(ix%+d)", low_off);
                    emit_rtl("ccsxt");
                } else {
                    emit_compute_ix_address(low_off);
                    emit_instrln("ld a,(hl)");
                    emit_rtl("ccsxt");
                }
                return;
            } 
        }

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
}

void emit_ld_symaddr(SYMBOL* sym) MYCC {
    emit_ld_symaddr_offset(sym, 0);
}

void emit_ld_symaddr_offset(SYMBOL* sym, uint16_t offset) MYCC {
    uint8_t type_id = sym->type_id;

    if (sym->scope == GLOBAL) {
        emit_ld_immed();
        emit_sname(sym->name);
        if (offset) { emit_ch('+'); emit_n(offset); }
        emit_nl();
    }
    else if (sym->scope == LOCAL) {
        /* For variadic functions, ARGUMENT parameters need dynamic offset calculation */
        if (sym->klass == ARGUMENT && func_is_variadic) {
            emit_load_variadic_arg_addr(sym);
            if (offset) {
                emit_ldde_immed_n(offset);
                emit_add16();
            }
            return;
        }
        
        /* Calculate base address of symbol (low byte) */
        int16_t bp_offset = compute_symbol_base_offset(sym);
        
        /* Fold in any additional offset BEFORE range checking */
        bp_offset += (int16_t)offset;
        
        /* OPTIMIZATION: If final offset is in IX+d range, compute optimally */
        /* IMPORTANT: Do NOT use DE register - callers may need it preserved! */
        if (bp_offset >= -128 && bp_offset <= 127) {
            if (bp_offset == 0) {
                /* Special case: offset 0 - just copy IX */
                emit_copy_ix_to_hl();
            } else if (bp_offset >= -3 && bp_offset <= 3) {
                /* Small offset: copy IX and use inc/dec */
                emit_copy_ix_to_hl();
                emit_add_hl_small(bp_offset);
            } else {
                /* Medium offset: use BC to avoid clobbering DE */
                emit_copy_ix_to_hl();
                emit_instrln("ld bc,%d", (uint16_t)bp_offset);
                emit_instrln("add hl,bc");
            }
        } else {
            /* Out of range: use BC to avoid clobbering DE */
            emit_instrln("ld hl,%d", bp_offset);
            emit_instrln("ld b,ixh");
            emit_instrln("ld c,ixl");
            emit_instrln("add hl,bc");
        }
    }
}

void emit_ld_const(uint16_t value) MYCC {
    emit_ld_immed_n(value);
}

void emit_store_sym(SYMBOL* sym) MYCC {
    uint8_t type_id = sym->type_id;
    if (type_is_array(type_id)) error(errNotlvalue);
    
    if (sym->scope == GLOBAL) {
        if (!type_is_pointer(type_id) && type_is_char(type_id)) {
            emit_instrln("ld a,l");
            emit_instr("ld ("); emit_sname(sym->name); emit_strln("),a");
        }
        else {
            emit_instr("ld ("); emit_sname(sym->name); emit_strln("),hl");
        }
    }
    else if (sym->scope == LOCAL) {
        /* For variadic functions, ARGUMENT parameters need dynamic offset calculation */
        if (sym->klass == ARGUMENT && func_is_variadic) {
            emit_store_variadic_arg(sym);
            return;
        }
        
        if (sym->klass == VARIABLE && !type_is_pointer(type_id) && type_is_char(type_id)) {
            /* Char/byte scalar: store single byte */
            int8_t bp_offset = (uint8_t)sym->offset;
            int16_t low_off = -(bp_offset + 1);
            
            if (low_off >= -128 && low_off <= 127) {
                emit_instrln("ld (ix%+d),l", low_off);
            } else {
                emit_push();
                emit_compute_ix_address(low_off);
                emit_pop_de();
                /* DE contains value, HL contains address: store directly */
                emit_store_byte_at_hl();
            }
        } else {
            /* Int scalar/pointer or argument: store 2-byte value */
            int16_t low_off, high_off;
            compute_local_offsets(sym, &low_off, &high_off);
            
            if (offsets_in_ix_range(low_off, high_off)) {
                emit_instrln("ld (ix%+d),l", low_off);
                emit_instrln("ld (ix%+d),h", high_off);
            } else {
                emit_push();
                emit_compute_ix_address(low_off);
                emit_pop_de();
                /* DE contains value, HL contains address: store directly */
                emit_store_word_at_hl();
            }
        }
    }
}

/* Store value from HL into address on top of stack */
void emit_store(uint8_t type_id) MYCC {
    emit_swap();         // value in DE
    emit_pop_hl();       // target address in HL
    if (type_is_void(type_id) || type_is_char(type_id)) {
        emit_store_byte_at_hl();
    } else {
        emit_store_word_at_hl();
    }
} 

void emit_load(uint8_t type_id) MYCC {
    /* Load the value pointed to by HL. If the effective element type is char
     * (including pointers/arrays to char), load a byte and sign-extend.
     * Otherwise load a 16-bit value (word) from memory.
     */
    uint8_t effective_type = type_id;
    if (type_is_pointer(type_id) || type_is_array(type_id)) {
        uint8_t elem = type_get_element_type_id(type_id);
        if (elem != TYPE_ID_VOID) effective_type = elem;
    }

    if (type_is_void(effective_type) || type_is_char(effective_type)) {
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

/* Optimized division by power of 2 - use arithmetic right shift
 * DE = DE / HL where HL is a power of 2
 * For signed division, need to handle negative numbers correctly */
void emit_div_pow2(uint8_t shift_count) MYCC {
    if (shift_count == 0) {
        /* Division by 1 - result is already in DE, just move to HL */
        emit_swap();
        return;
    }
    if (shift_count == 1) {
        /* Division by 2 - special case with sra */
        emit_instrln("sra d");
        emit_instrln("rr e");
        emit_swap();
        return;
    }
    /* General case: use bsra for shift right arithmetic */
    emit_instrln("ld b,%d", shift_count);
    emit_instrln("bsra de,b");
    emit_swap();
}

/* Optimized modulo by power of 2 - use bitwise AND
 * DE = DE % HL where HL is a power of 2
 * For positive numbers: x % 2^n = x & (2^n - 1) */
void emit_mod_pow2(uint16_t divisor) MYCC {
    uint16_t mask = divisor - 1;
    /* x % power_of_2 = x & (power_of_2 - 1) */
    emit_instrln("ld hl,%d", mask);
    emit_rtl("ccand");
    emit_swap();  /* Result ends up in HL for consistency */
}

/* Optimized division - detects power of 2 and uses shift, otherwise uses RTL
 * DE = DE / divisor (constant) */
void emit_div_const_optimized(uint16_t divisor) MYCC {
    /* Check if divisor is a power of 2 and positive */
    if (divisor > 0 && (divisor & (divisor - 1)) == 0) {
        /* Count trailing zeros to get shift amount */
        uint8_t shift = 0;
        uint16_t temp = divisor;
        while (temp > 1) {
            shift++;
            temp >>= 1;
        }
        emit_div_pow2(shift);
    } else {
        /* Use generic division */
        emit_rtl("ccdiv");
    }
}

/* Optimized modulo - detects power of 2 and uses AND, otherwise uses RTL
 * DE = DE % divisor (constant) */
void emit_mod_const_optimized(uint16_t divisor) MYCC {
    /* Check if divisor is a power of 2 and positive */
    if (divisor > 0 && (divisor & (divisor - 1)) == 0) {
        emit_mod_pow2(divisor);
    } else {
        /* Use generic division and take remainder */
        emit_rtl("ccdiv");
        emit_swap();  /* Remainder is in DE, move to HL */
    }
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
        /* Create char[80] array type for args */
        uint8_t args_type = type_make_array(TYPE_ID_CHAR, 80);
        addglb("args", VARIABLE, args_type, 0);
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