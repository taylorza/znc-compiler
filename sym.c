#include "znc.h"

SYMBOL symtab[MAX_SYMBOLS];

uint16_t lastgbl = 0;
uint16_t lastloc = MAX_SYMBOLS;
uint16_t scopecount = 0; // nested scopes

/* undefined_sym lives in the main bank (sym_stub.c) so all banked modules
 * can read it without a bank switch. */

SYMBOL* far_findglb(IDENT_ID name_id) MYCC {
    for (uint16_t i = 0; i < lastgbl; i++) {
        if (symtab[i].name_id == name_id) {
            return &symtab[i];
        }
    }
    return NULL;
}

SYMBOL* far_findloc(IDENT_ID name_id) MYCC {
    for (uint16_t i = lastloc; i < MAX_SYMBOLS; ++i) {
        if (symtab[i].name_id == name_id) {
            return &symtab[i];
        }
    }
    return NULL;
}

SYMBOL* far_lookupIdent(IDENT_ID name_id) MYCC {
    SYMBOL* sym;

    sym = far_findloc(name_id);
    if (sym) return sym;

    sym = far_findglb(name_id);
    if (sym) return sym;

    return NULL;
}

void far_updatesym(SYMBOL from) MYCC {
    SYMBOL* sym;
    if (from.scope == LOCAL)
        sym = far_findloc(from.name_id);
    else
        sym = far_findglb(from.name_id);

    if (sym) *sym = from;
}

SYMBOL* far_addglb(IDENT_ID name_id, SYM_CLASS klass, uint8_t type_id, int16_t value) MYCC {
    SYMBOL *sym = far_findglb(name_id);
    if (sym) return sym;

    if (lastgbl == MAX_SYMBOLS) {
        error(errTooManySymbols);
        return NULL;
    }

    sym = &symtab[lastgbl];
    sym->name_id = name_id;
    sym->klass = klass;
    sym->type_id = type_id;
    sym->scope = GLOBAL;
    sym->flags = 0;
    if (klass == FUNCTION || klass == FUNCTION_PROTO) {
        sym->fn.signature_id = 0xFF; // No signature by default
    }
    else {
        sym->stk.offset = value;
    }
    ++lastgbl;
    return sym;
}

SYMBOL* far_addloc(IDENT_ID name_id, SYM_CLASS klass, uint8_t type_id, int16_t value) MYCC {
    SYMBOL *sym = far_findloc(name_id);
    if (sym) return sym;

    if (lastloc-1 == lastgbl) {
        error(errTooManySymbols);
        return NULL;
    }

    sym = &symtab[--lastloc];
    sym->name_id = name_id;
    sym->klass = klass;
    sym->type_id = type_id;
    sym->scope = LOCAL;
    sym->flags = 0;
    if (klass == FUNCTION || klass == FUNCTION_PROTO) {        
        sym->fn.signature_id = 0xFF; // No signature by default
    } else {
        sym->stk.offset = value;
    }    
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

uint16_t far_get_lastgbl(void) MYCC {
    return lastgbl;
}

void far_reset_lastgbl(uint16_t to) MYCC {
    lastgbl = to;
}

void far_dump_globals_range(uint16_t from, uint16_t to) MYCC {
    for (uint16_t i = from; i < to; ++i) {
        SYMBOL* sym = &symtab[i];
        if (sym->klass == FUNCTION_PROTO) {
            /* copy_ident_to_token lives in the main bank – always reachable */
            copy_ident_to_token(sym->name_id);
            error(errNotDefined_s, token);
        }
        if (sym->klass == FUNCTION || type_is_const(sym->type_id) || (sym->flags & SYM_FLAG_INITIALIZED)) continue;
        emit_sname_id(sym->name_id);
        emit_ch(' ');
        uint16_t size = type_size(sym->type_id);
        switch(size) {
            case 1:  emit_str("db 0"); break;
            case 2:  emit_str("dw 0"); break;
            default: emit_str("ds "); emit_n(size); break;
        }
        emit_nl();
        sym->flags |= SYM_FLAG_INITIALIZED;
    }
}

void far_dump_globals(void) MYCC {
    for (uint16_t i = 0; i < lastgbl; ++i) {
        SYMBOL* sym = &symtab[i];
        if (sym->klass == FUNCTION_PROTO) {
            /* copy_ident_to_token lives in the main bank – always reachable */
            copy_ident_to_token(sym->name_id);
            error(errNotDefined_s, token);
        }
        /* If symbol was emitted with an initializer earlier (marked via
         * `SYM_FLAG_INITIALIZED`), skip auto allocation here. Also skip
         * functions and consts as before.
         */
        if (sym->klass == FUNCTION || type_is_const(sym->type_id) || (sym->flags & SYM_FLAG_INITIALIZED)) continue; // skip functions, consts, inited
        emit_sname_id(sym->name_id);
        emit_ch(' ');
        uint16_t size = type_size(sym->type_id);
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

