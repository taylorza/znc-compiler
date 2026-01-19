#include "znc.h"
#include "farcall.h"
#include "sym.h"
#include "shared.h"

// Wrappers for symbol table banked calls (BANK 41)
SYMBOL findglb(const char* name) MYCC {
    ARENA_MARKER m = arena_get_marker();
    char* ncopy = arena_strdup(name, strnlen(name, MAX_IDENT_LEN));

    SYMBOL *sym;
    SYMBOL lsym;
    PROLOG(41)
    sym = far_findglb(ncopy);
    if (!sym) sym = &undefined_sym;
    lsym = *sym;
    EPILOG

    arena_free_to_marker(m);
    return lsym;
}

SYMBOL findloc(const char* name) MYCC {
    ARENA_MARKER m = arena_get_marker();
    char* ncopy = arena_strdup(name, strnlen(name, MAX_IDENT_LEN));

    SYMBOL *sym;
    SYMBOL lsym;
    PROLOG(41)
    sym = far_findloc(ncopy);
    if (!sym) sym = &undefined_sym;
    lsym = *sym;
    EPILOG

    arena_free_to_marker(m);
    return lsym;
}

SYMBOL lookupIdent(const char* name) MYCC {
    ARENA_MARKER m = arena_get_marker();
    char* ncopy = arena_strdup(name, strnlen(name, MAX_IDENT_LEN));

    SYMBOL *sym;
    SYMBOL lsym;
    PROLOG(41)
    sym = far_lookupIdent(ncopy);
    if (!sym) sym = &undefined_sym;
    lsym = *sym;
    EPILOG

    arena_free_to_marker(m);
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
    ARENA_MARKER m = arena_get_marker();
    char* ncopy = arena_strdup(name, strnlen(name, MAX_IDENT_LEN));

    SYMBOL *sym;
    SYMBOL lsym;
    PROLOG(41)
    sym = far_addglb(ncopy, klass, type_id, value);
    if (!sym) sym = &undefined_sym;
    lsym = *sym;
    EPILOG

    arena_free_to_marker(m);
    return lsym;
}

SYMBOL addloc(const char* name, SYM_CLASS klass, uint8_t type_id, int16_t value) MYCC {
    ARENA_MARKER m = arena_get_marker();
    char* ncopy = arena_strdup(name, strnlen(name, MAX_IDENT_LEN));

    SYMBOL *sym;
    SYMBOL lsym;
    PROLOG(41)
    sym = far_addloc(ncopy, klass, type_id, value);
    if (!sym) sym = &undefined_sym;
    lsym = *sym;
    EPILOG

    arena_free_to_marker(m);
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

void dump_globals(void) MYCC {
    PROLOG(41)
    far_dump_globals();
    EPILOG
}
