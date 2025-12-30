#include "znc.h"

SYMBOL symtab[MAX_SYMBOLS];

uint16_t lastgbl = 0;
uint16_t lastloc = MAX_SYMBOLS;
uint16_t scopecount = 0; // nested scopes

SYMBOL undefined_sym = {.scope = SCOPE_UNDEFINED, .klass = CLASS_UNDEFINED};

SYMBOL* far_findglb(const char *name) MYCC {
    for (uint16_t i=0; i < lastgbl; i++) {
        if (strcmp(name, symtab[i].name) == 0) {
            return &symtab[i];
        }
    }
    return NULL;
}

SYMBOL* far_findloc(const char *name) MYCC {
    for (uint16_t i=lastloc; i < MAX_SYMBOLS; ++i) {
        if (strcmp(name, symtab[i].name) == 0) {
            return &symtab[i];
        }
    }
    return NULL;
}

SYMBOL* far_lookupIdent(const char* name) MYCC {
    SYMBOL* sym;

    sym = far_findloc(name);
    if (sym) return sym;

    sym = far_findglb(name);
    if (sym) return sym;

    return NULL;
}

void far_updatesym(SYMBOL* from) MYCC {
    SYMBOL* sym;
    if (from->scope == LOCAL)
        sym = far_findloc(from->name);
    else
        sym = far_findglb(from->name);

    if (sym) *sym = *from;
}

SYMBOL* far_addglb(const char* name, SYM_CLASS klass, TYPEREC type, int16_t value) MYCC {
    SYMBOL *sym = far_findglb(name);
    if (sym) return sym;

    if (lastgbl == MAX_SYMBOLS) {
        error(errTooManySymbols);
        return NULL;
    }

    sym = &symtab[lastgbl];
    strncpy(sym->name, name, MAX_IDENT_LEN);
    sym->klass = klass;
    sym->type = type;
    sym->scope = GLOBAL;
    sym->offset = value;
    sym->flags = 0;
    ++lastgbl;
    return sym;
}

SYMBOL* far_addloc(const char* name, SYM_CLASS klass, TYPEREC type, int16_t value) MYCC {
    SYMBOL *sym = far_findloc(name);
    if (sym) return sym;

    if (lastloc-1 == lastgbl) {
        error(errTooManySymbols);
        return NULL;
    }

    sym = &symtab[--lastloc];
    strncpy(sym->name, name, MAX_IDENT_LEN);
    sym->klass = klass;
    sym->type = type;
    sym->scope = LOCAL;
    sym->offset = value;
    return sym;
}

uint16_t far_push_frame(void) MYCC {
    ++scopecount;
    return lastloc;
}

void far_pop_frame(uint16_t frame) MYCC {
    --scopecount;
    lastloc = frame;
}

uint8_t far_is_scoped(void) MYCC {
    return scopecount != 0;
}

void far_dump_globals(void) MYCC {
    for (uint16_t i = 0; i < lastgbl; ++i) {
        SYMBOL* sym = &symtab[i];
        if (sym->klass == FUNCTION_PROTO) error(errNotDefined_s, sym->name);
        /* If symbol was emitted with an initializer earlier (marked via
         * `SYM_FLAG_INITIALIZED`), skip auto allocation here. Also skip
         * functions and consts as before.
         */
        if (sym->klass == FUNCTION || is_const(&sym->type) || (sym->flags & SYM_FLAG_INITIALIZED)) continue; // skip functions, consts, inited
        TYPEREC* ptype = &sym->type;
        emit_sname(sym->name);
        emit_ch(' ');
        uint16_t size = is_int(ptype) || is_ptr(ptype) ? 2 : 1;
        if (is_array(ptype)) size *= -ptype->dim;
        switch(size) {
            case 1:
                emit_str("db 0");
                break;
            case 2:
                emit_str("dw 0");
                break;
            default:
                emit_str("ds ");
                emit_n(size);
                break;
        }
        emit_nl();   
        sym->flags |= SYM_FLAG_INITIALIZED; // mark as initialized
    }
}

