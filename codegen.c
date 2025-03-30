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

void nl(void) {
    asm_putc(NL);
}

static void ot(const char *s) {
    emit_strf("  %s", s);
}

static void ol(const char *s) {
    ot(s);
    nl();
}

void emit_ch(char c) {
    asm_putc(c);
}

void emit_strf(const char* fmt, ...) {
    char buf[128];
    va_list v;
    va_start(v, fmt);
    vsnprintf(buf, sizeof(buf), fmt, v);
    asm_puts(buf);
}

void emit_lbl(uint16_t lbl) {
    emit_strf("lbl%d%c", lbl, NL);    
}

void emit_lblref(uint16_t lbl) {
    emit_strf("lbl%d", lbl);
}

void emit_str(const char *s) {
    asm_puts(s);
}

void emit_sname(const char *name) {
    asm_putc('_'); asm_puts(name);
}

void emit_strref(uint16_t id) {
    emit_strf("str%d", id);
}

void emit_instr(const char* s) {
    ol(s);
}

void emit_n16(uint16_t n) {
    uint16_t k = 10000;
    uint8_t c, zs = 0;

    while (k > 0) {
        c = n / k + '0';
        if ((c != '0') || (k == 1) || (zs)) {
            zs = 1;
            asm_putc(c);
        }
        n %= k;
        k /= 10;
    }
}

void emit_ld_immed(void) {
    ot("ld hl,");
}

void emit_ldbc_immed(void) {
    ot("ld bc,");
}

void emit_push(void) {
    ol("push hl");
    sp += 2;
}

void emit_pop(void) {
    ol("pop de");
    sp -= 2;
}

void emit_swap(void) {
    ol("ex de,hl");
}

void emit_add16(void) {
    ol("add hl,de");
}

void emit_sub16(void) {
    emit_swap();
    ol("xor a");
    ol("sbc hl,de");
}

void emit_rtl(const char* name) {
    if ((inc_rtl(name) & FLAG_RTL_INLINE) == 0) {
        emit_call(name);
    }
}

void emit_call(const char *name) {
    ot("call ");
    emit_str(name);
    nl();
}

void emit_callsym(SYMBOL* sym) {
    if (sym->klass == FUNCTION) {
        ot("call ");
        emit_sname(sym->name);
        nl();
    } else {
        uint16_t retlbl = newlbl(); 
        emit_ld_symval(sym);
        emit_strf("  ld bc,lbl%d%c", retlbl, NL);
        emit_instr("push bc");
        emit_push();
        emit_ret();
        emit_lbl(retlbl);
    }
}

void emit_ret(void) {
    emit_instr("ret");
}

void emit_jp(uint16_t lbl) {
    ot("jp ");
    emit_lblref(lbl); nl();
}

void emit_jp_true(uint16_t lbl) {
    ol("ld a, h");
    ol("or l");
    ot("jp nz, ");
    emit_lblref(lbl); nl();
}

void emit_jp_false(uint16_t lbl) {
    ol("ld a, h");
    ol("or l");
    ot("jp z, ");
    emit_lblref(lbl); nl();
}

uint16_t emit_alloclocals(void) {
    uint16_t lbl = newlbl();
    emit_ld_immed(); emit_str("-"); emit_lblref(lbl); nl();
    ot("add hl,sp"); nl();
    ot("ld sp,hl"); nl();
    return lbl;
}
void emit_lblequ16(uint16_t lbl, uint16_t value) {
    emit_lblref(lbl); emit_str(" equ "); emit_n16(value); nl();
}

void emit_ld_symval(SYMBOL* sym) {
    TYPEREC* ptype = &sym->type;

    if (sym->scope == GLOBAL) {
        if (is_array(ptype)) {
            emit_ld_immed();
            emit_sname(sym->name);
            nl();
        }
        else if (!is_ptr(ptype) && is_char(ptype)) {
            ot("ld a,(");
            emit_sname(sym->name);
            emit_ch(')');
            nl();
            emit_rtl("ccsxt");
        }
        else {
            ot("ld hl,(");
            emit_sname(sym->name);
            emit_ch(')');
            nl();
        }
    }
    else if (sym->scope == LOCAL) {
        uint8_t offset = 0;
        switch (sym->klass) {
            case ARGUMENT: offset = bp_lastarg+4; break;
            case VARIABLE: offset = 0; break;
        }

        if (sym->klass == VARIABLE)
            emit_strf("  ld hl,%d+lbl%d-%d-%d%c", sp, locals_lbl, type_size(&sym->type), sym->offset, NL);
        else if (sym->klass == ARGUMENT)
            emit_strf("  ld hl,%d+lbl%d+%d-%d%c", sp, locals_lbl, 2 + bp_lastarg, sym->offset, NL);
        ot("add hl,sp"); nl(); 

        if (!is_array(ptype)) {
            emit_load(sym->type);
        }
    }
}

void emit_ld_symaddr(SYMBOL* sym) {
    emit_ld_immed();
    emit_sname(sym->name); nl();
}

void emit_store_sym(SYMBOL* sym) {
    TYPEREC* ptype = &sym->type;
    if (is_array(ptype)) error(errNotlvalue);
    if (sym->scope == GLOBAL) {
        if (!is_ptr(ptype) && is_char(ptype)) {
            ol("ld a,l");
            ot("ld ("); emit_sname(sym->name); emit_str("),a"); nl();
        }
        else {
            ot("ld ("); emit_sname(sym->name); emit_str("),hl"); nl();
        }
    }
    else if (sym->scope == LOCAL) {
        emit_push();
        if (sym->klass == VARIABLE)
            emit_strf("  ld hl,%d+lbl%d-%d-%d%c", sp, locals_lbl, type_size(&sym->type), sym->offset, NL);
        else if (sym->klass == ARGUMENT)
            emit_strf("  ld hl,%d+lbl%d+%d-%d%c", sp, locals_lbl, 2 + bp_lastarg, sym->offset, NL);
        ot("add hl, sp"); nl();
        emit_pop();
        if (!is_ptr(ptype) && is_char(ptype)) {
            ol("ld (hl),e");
        } else {
            ol("ld (hl),e");
            ol("inc hl");
            ol("ld (hl),d");
        }        
    }
}

void emit_store(TYPEREC type) {
    emit_pop();         // target address
    if (is_void(&type) || is_char(&type)) {
        ol("ld a,l");
        ol("ld (de),a");
    } else {
        ol("ld a,l");
        ol("ld (de),a");
        ol("inc de");
        ol("ld a,h");
        ol("ld (de),a");
    }
}

void emit_load(TYPEREC type) {
    if (!is_ptr(&type) && is_char(&type)) {
        ol("ld a,(hl)");
        emit_rtl("ccsxt");
    }
    else {
        ol("ld a,(hl)");
        ol("inc hl");
        ol("ld h,(hl)");
        ol("ld l, a");        
    }
}

void emit_neg(void) {
    emit_rtl("ccneg");
}

void emit_mul2(void) {
    ol("add hl,hl");
}

