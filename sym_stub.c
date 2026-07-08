#include "znc.h"
#include "farcall.h"
#include "identtbl.h"
#include "sym.h"
#include "shared.h"

// Wrappers for symbol table banked calls (BANK 41)
// Names are interned into IDENT_IDs (via BANK 45) before switching to BANK 41.

/* undefined_sym is kept in the main bank so every banked module (expr.c in
 * BANK_43, initializer.c in BANK_43, etc.) can read it without a bank switch.
 * It must NOT live in sym.c (BANK_41). */
SYMBOL undefined_sym = {.name_id = IDENT_ID_NONE};

SYMBOL findglb(const char* name) MYCC {
    IDENT_ID nid = intern_ident(name);

    SYMBOL *sym;
    SYMBOL lsym;
    PROLOG(41)
    sym = far_findglb(nid);
    if (!sym) sym = &undefined_sym;
    lsym = *sym;
    EPILOG

    return lsym;
}

SYMBOL findloc(const char* name) MYCC {
    IDENT_ID nid = intern_ident(name);

    SYMBOL *sym;
    SYMBOL lsym;
    PROLOG(41)
    sym = far_findloc(nid);
    if (!sym) sym = &undefined_sym;
    lsym = *sym;
    EPILOG

    return lsym;
}

SYMBOL lookupIdent(const char* name) MYCC {
    IDENT_ID nid = intern_ident(name);

    SYMBOL *sym;
    SYMBOL lsym;
    PROLOG(41)
    sym = far_lookupIdent(nid);
    if (!sym) sym = &undefined_sym;
    lsym = *sym;
    EPILOG

    return lsym;
}

void updatesym(SYMBOL* from) MYCC {
    /* Copy the SYMBOL into a local stack variable (in main bank) before switching
     * banks. Passing the structure by value is cheaper and avoids arena overhead.
     */
    SYMBOL copy = *from;

    PROLOG(41)
    far_updatesym(copy);
    EPILOG;
}

SYMBOL addglb(const char* name, SYM_CLASS klass, uint8_t type_id, int16_t value) MYCC {
    IDENT_ID nid = intern_ident(name);

    SYMBOL *sym;
    SYMBOL lsym;
    PROLOG(41)
    sym = far_addglb(nid, klass, type_id, value);
    if (!sym) sym = &undefined_sym;
    lsym = *sym;
    EPILOG

    return lsym;
}

SYMBOL addloc(const char* name, SYM_CLASS klass, uint8_t type_id, int16_t value) MYCC {
    IDENT_ID nid = intern_ident(name);

    SYMBOL *sym;
    SYMBOL lsym;
    PROLOG(41)
    sym = far_addloc(nid, klass, type_id, value);
    if (!sym) sym = &undefined_sym;
    lsym = *sym;
    EPILOG

    return lsym;
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

uint16_t get_lastgbl(void) MYCC {
    PROLOG(41)
    uint16_t i = far_get_lastgbl();
    EPILOG_RETURN(i)
}

void reset_lastgbl(uint16_t to) MYCC {
    PROLOG(41)
    far_reset_lastgbl(to);
    EPILOG
}

void dump_globals_range(uint16_t from, uint16_t to) MYCC {
    PROLOG(41)
    far_dump_globals_range(from, to);
    EPILOG
}

void check_undefined(void) MYCC {
    PROLOG(41)
    far_check_undefined();
    EPILOG
}
