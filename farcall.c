#include "znc.h"

#define PROLOG(BANK) \
    { \
        uint8_t page0 = ZXN_READ_MMU6(); \
        uint8_t page1 = ZXN_READ_MMU7(); \
        ZXN_WRITE_MMU6(_z_page_table[BANK<<1]); \
        ZXN_WRITE_MMU7(_z_page_table[(BANK<<1)+1]);

#define EPILOG \
        ZXN_WRITE_MMU6(page0); \
        ZXN_WRITE_MMU7(page1); \
    }

#define EPILOG_RETURN(EXPR) \
        ZXN_WRITE_MMU6(page0); \
        ZXN_WRITE_MMU7(page1); \
        return EXPR; \
    }

extern unsigned char _z_page_table[];

// strtbl - BANK 40
int16_t lookupstr(const char* s) MYCC {
    PROLOG(40)
    int16_t i = far_lookupstr(s);
    EPILOG_RETURN(i);
}

void dump_strings(void) MYCC {
    PROLOG(40)
    far_dump_strings();
    EPILOG
}

// sym - BANK 41
SYMBOL undefined_sym = {.scope = SCOPE_UNDEFINED, .klass = CLASS_UNDEFINED};

SYMBOL findglb(const char* name) MYCC {
    PROLOG(41)
    SYMBOL *sym = far_findglb(name);
    if (!sym) sym = &undefined_sym;
    SYMBOL lsym = *sym;
    EPILOG_RETURN(lsym);
}

SYMBOL findloc(const char* name) MYCC {
    PROLOG(41)
    SYMBOL *sym = far_findloc(name);
    if (!sym) sym = &undefined_sym;
    SYMBOL lsym = *sym;
    EPILOG_RETURN(lsym);
}

SYMBOL lookupIdent(const char* name) MYCC {
    PROLOG(41)
    SYMBOL *sym = far_lookupIdent(name);
    if (!sym) sym = &undefined_sym;
    SYMBOL lsym = *sym;
    EPILOG_RETURN(lsym);
}

void updatesym(SYMBOL* from) MYCC {
    PROLOG(41)
    far_updatesym(from);
    EPILOG;
}

SYMBOL addglb(const char* name, SYM_CLASS klass, TYPEREC type, int16_t value) MYCC {
    PROLOG(41)
    SYMBOL *sym = far_addglb(name, klass, type, value);
    if (!sym) sym = &undefined_sym;
    SYMBOL lsym = *sym;
    EPILOG_RETURN(lsym);
}

SYMBOL addloc(const char* name, SYM_CLASS klass, TYPEREC type, int16_t value) MYCC {
    PROLOG(41)
    SYMBOL *sym = far_addloc(name, klass, type, value);
    if (!sym) sym = &undefined_sym;
    SYMBOL lsym = *sym;
    EPILOG_RETURN(lsym);
}

uint16_t push_frame(void) MYCC {
    PROLOG(41)
    uint16_t i = far_push_frame();
    EPILOG_RETURN(i);
}

void pop_frame(uint16_t frame) MYCC {
    PROLOG(41)
    far_pop_frame(frame);
    EPILOG
}

uint8_t is_scoped(void) MYCC {
    PROLOG(41)
    uint8_t i = far_is_scoped();
    EPILOG_RETURN(i)
}

void dump_globals(void) MYCC {
    PROLOG(41)
    far_dump_globals();
    EPILOG
}