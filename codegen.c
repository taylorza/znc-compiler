#include <stdarg.h>

#include "znc.h"

uint16_t nxtlbl = 0;    // label counter
uint16_t argcntlbl = 0; // label for the argcount for the current function

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

uint8_t asm_open(const char *asmfilename) {
    pos = 0;
#ifdef __ZXNEXT
    errno = 0;
    asm_fh = esx_f_open(asmfilename, ESX_MODE_OPEN_CREAT_TRUNC | ESX_MODE_W);
    if (errno) return 0;
#else
    asm_fh = stdout;
#endif
    return 1;
}

void asm_close(void) {
    
    if (pos) {
#ifdef __ZXNEXT
        putc('.', stdout);
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

void asm_putc(char c) {
    if (pos == MAX_WRITE_BUF) {
#ifdef __ZXNEXT
        putc('.', stdout);
        esx_f_write(asm_fh, asmbuf, pos);        
#else
        fwrite(asmbuf, 1, pos, asm_fh);
#endif
        pos = 0;
    }
    asmbuf[pos++] = c;
}

static void asm_puts(const char *s) {
    while (*s) asm_putc(*s++);
}

uint16_t newlbl(void) {
    return ++nxtlbl;
}


void emit_nl(void) {
    asm_putc(NL);
}

void emit_ch(char c) {
    asm_putc(c);
}

void emit_lbl(uint16_t lbl) {
    emit_strln("lbl%d", lbl);    
}

void emit_lblref(uint16_t lbl) {
    emit_str("lbl%d", lbl);
}

void emit_str(const char *fmt, ...) {
    char buf[64];
    va_list v;
    va_start(v, fmt);
    vsnprintf(buf, sizeof(buf), (char *)fmt, v);
    va_end(v);
    asm_puts(buf);
}

void emit_strln(const char* fmt, ...) {
    char buf[64];
    va_list v;
    va_start(v, fmt);
    vsnprintf(buf, sizeof(buf), (char *)fmt, v);
    va_end(v);
    asm_puts(buf);
    emit_nl();
}

void emit_sname(const char *name) {
    emit_str("_%s", name);    
}

void emit_strref(uint16_t id) {
    emit_str("str%d", id);
}

void emit_instr(const char * fmt, ...) {
    char buf[64];
    va_list v;
    va_start(v, fmt);
    vsnprintf(buf, sizeof(buf), (char *)fmt, v);
    va_end(v);
    asm_puts("  "); asm_puts(buf);
}

void emit_instrln(const char* fmt, ...) {
    char buf[64];
    va_list v;
    va_start(v, fmt);
    vsnprintf(buf, sizeof(buf), (char *)fmt, v);
    va_end(v);
    asm_puts("  "); asm_puts(buf);
    emit_nl();
}

void emit_n16(uint16_t n) {
    emit_str("%d", n);
}

void emit_ld_immed(void) {
    emit_instr("ld hl,");
}

void emit_ldbc_immed(void) {
    emit_instr("ld bc,");
}

void emit_push(void) {
    emit_instrln("push hl");
}

void emit_pop(void) {
    emit_instrln("pop de");
}

void emit_swap(void) {
    emit_instrln("ex de,hl");
}

void emit_add16(void) {
    emit_instrln("add hl,de");
}

void emit_sub16(void) {
    emit_swap();
    emit_instrln("xor a");
    emit_instrln("sbc hl,de");
}

void emit_rtl(const char* name) {
    if ((inc_rtl(name) & FLAG_RTL_INLINE) == 0) {
        emit_call(name);
    }
}

void emit_call(const char *name) {
    emit_instrln("call %s", name);    
}

void emit_callsym(SYMBOL* sym) {
    if (sym->klass == FUNCTION) {
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

void emit_ret(void) {
    emit_instrln("ret");
}

void emit_jp(uint16_t lbl) {
    emit_instr("jp ");
    emit_lblref(lbl); emit_nl();
}

void emit_jp_true(uint16_t lbl) {
    emit_instrln("ld a, h");
    emit_instrln("or l");
    emit_instr("jp nz, ");
    emit_lblref(lbl); emit_nl();
}

void emit_jp_false(uint16_t lbl) {
    emit_instrln("ld a, h");
    emit_instrln("or l");
    emit_instr("jp z, ");
    emit_lblref(lbl); emit_nl();
}

uint16_t emit_alloclocals(void) {
    uint16_t lbl = newlbl();
    emit_ld_immed(); emit_str("-"); emit_lblref(lbl); emit_nl();
    emit_instrln("add hl,sp");
    emit_instrln("ld sp,hl");
    return lbl;
}
void emit_lblequ16(uint16_t lbl, uint16_t value) {
    emit_lblref(lbl); emit_str(" equ "); emit_n16(value); emit_nl();
}

void emit_ld_symval(SYMBOL* sym) {
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
        int8_t bp_offset = 0;
        
        if (sym->klass == VARIABLE) {
            bp_offset = (uint8_t)sym->offset;
            if (is_array(ptype)) {
                emit_instrln("ld hl, %d", bp_offset - type_size(ptype) + 1);
                emit_instrln("ld d, ixh");
                emit_instrln("ld e, ixl");                
                emit_instrln("add hl, de");
            } else {
                emit_instrln("ld h,(ix-%d)", bp_offset);
                emit_instrln("ld l,(ix-%d)", bp_offset +1);
            }
        } else if (sym->klass == ARGUMENT) {
            bp_offset = 2 + (func_argcount - sym->offset) * 2;            
            emit_instrln("ld h,(ix+%d)", bp_offset+1);
            emit_instrln("ld l,(ix+%d)", bp_offset);            
        }
    }
}

void emit_ld_symaddr(SYMBOL* sym) {
    emit_ld_immed();
    emit_sname(sym->name); emit_nl();
}

void emit_store_sym(SYMBOL* sym) {
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
        int8_t bp_offset = 0;
        if (sym->klass == VARIABLE) {
            bp_offset = (uint8_t)sym->offset;
            emit_instrln("ld (ix-%d),h", bp_offset);
            emit_instrln("ld (ix-%d),l", bp_offset + 1);
        }
        else if (sym->klass == ARGUMENT) {
            bp_offset = 2 + (func_argcount - sym->offset) * 2;
            emit_instrln("ld (ix+%d),h", bp_offset + 1);
            emit_instrln("ld (ix+%d),l", bp_offset);
        }
    }
}

void emit_store(TYPEREC type) {
    emit_pop();         // target address
    if (is_void(&type) || is_char(&type)) {
        emit_instrln("ld a,l");
        emit_instrln("ld (de),a");
    } else {
        emit_instrln("ld a,l");
        emit_instrln("ld (de),a");
        emit_instrln("inc de");
        emit_instrln("ld a,h");
        emit_instrln("ld (de),a");
    }
}

void emit_load(TYPEREC type) {
    if (!is_ptr(&type) && is_char(&type)) {
        emit_instrln("ld a,(hl)");
        emit_rtl("ccsxt");
    }
    else {
        emit_instrln("ld a,(hl)");
        emit_instrln("inc hl");
        emit_instrln("ld h,(hl)");
        emit_instrln("ld l, a");
    }
}

void emit_nreg_immed(uint8_t reg, uint8_t val) {
    emit_instrln("nreg %d,%d", reg, val);
}

void emit_nreg_A(uint8_t reg) {
    emit_instrln("ld a,l");
    emit_instrln("nreg %d,a", reg);
}

void emit_func_prologue(void) {
    emit_instrln("push ix");
    emit_instrln("ld ix,0");
    emit_instrln("add ix,sp");
}

void emit_func_epilogue(void) {
    emit_instrln("ld sp, ix");
    emit_instrln("pop ix");
    emit_ret();
}

void emit_neg(void) {
    emit_rtl("ccneg");
}

void emit_mul2(void) {
    emit_instrln("add hl,hl");
}

