#include "znc.h"
#include "farcall.h"
#include "sym.h"

// Wrappers for symbol table banked calls (BANK 41)
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
