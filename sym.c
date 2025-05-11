#include "znc.h"

TYPEREC const void_type = { .basetype = VOID, .dim = 1 };
TYPEREC const char_type = { .basetype = CHAR, .dim = 1 };
TYPEREC const int_type = { .basetype = INT, .dim = 1 };
TYPEREC const string_type = { .basetype = STRING, .dim = 1 };

SYMBOL symtab[MAX_SYMBOLS];
uint16_t lastgbl = 0;
uint16_t lastloc = MAX_SYMBOLS;
uint16_t scopecount = 0; // nested scopes

uint16_t type_size(const TYPEREC* type) MYCC {
    if (is_array(type)) {
        return (-type->dim) * (type->basetype == INT ? 2 : 1);
    }
    return 2;
}

uint8_t is_array(const TYPEREC* type) MYCC { return type->dim < 0; }
uint8_t is_ptr(const TYPEREC* type) MYCC { return type->dim == 0; }
uint8_t is_void(const TYPEREC* type) MYCC { return (type->basetype & 0xff) == VOID; }
uint8_t is_char(const TYPEREC* type) MYCC { return (type->basetype & 0xff) == CHAR; }
uint8_t is_int(const TYPEREC* type) MYCC { return (type->basetype & 0xff) == INT; }
uint8_t is_string(const TYPEREC* type) MYCC { return (type->basetype & 0xff) == STRING; }

uint8_t is_const(const TYPEREC* type) MYCC { return (type->basetype & CONST); }

uint8_t is_func_or_proto(const SYMBOL* sym) MYCC { return sym->klass == FUNCTION || sym->klass == FUNCTION_PROTO; }

void make_ptr(TYPEREC* type) MYCC { type->basetype &= 0xff;  type->dim = 0; }
void make_scalar(TYPEREC* type) MYCC { type->basetype &= 0xff; type->dim = 1; }
void make_array(TYPEREC* type, uint16_t size) MYCC { type->basetype &= 0xff;  type->dim = -size; }
void make_const(TYPEREC* type) MYCC { type->basetype |= CONST; }

BASE_TYPE base_type(TYPEREC* type) MYCC { return type->basetype & 0x7f; }

SYMBOL* findglb(const char *name) MYCC {
    for (uint16_t i=0; i < lastgbl; i++) {
        if (strcmp(name, symtab[i].name) == 0) {
            return &symtab[i];
        }
    }
    return NULL;
}

SYMBOL* findloc(const char *name) MYCC {
    for (uint16_t i=lastloc; i < MAX_SYMBOLS; ++i) {
        if (strcmp(name, symtab[i].name) == 0) {
            return &symtab[i];
        }
    }
    return NULL;
}

SYMBOL* lookupIdent(const char* name) MYCC {
    SYMBOL* sym;

    sym = findloc(name);
    if (sym) return sym;

    sym = findglb(name);
    if (sym) return sym;

    return NULL;
}

SYMBOL* addglb(const char* name, SYM_CLASS klass, TYPEREC type, int16_t value) MYCC {
    SYMBOL *sym = findglb(name);
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
    ++lastgbl;
    return sym;
}

SYMBOL* addloc(const char* name, SYM_CLASS klass, TYPEREC type, int16_t value) MYCC {
    SYMBOL *sym = findloc(name);
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

uint16_t push_frame(void) MYCC {
    ++scopecount;
    return lastloc;
}

void pop_frame(uint16_t frame) MYCC {
    --scopecount;
    lastloc = frame;
}

uint8_t is_scoped(void) MYCC {
    return scopecount != 0;
}

void dump_globals(void) MYCC {
    for (uint16_t i = 0; i < lastgbl; ++i) {
        SYMBOL* sym = &symtab[i];
        if (sym->klass == FUNCTION_PROTO) error(errNotDefined_s, sym->name);
        if (sym->klass == FUNCTION || is_const(&sym->type)) continue; // skip functions and consts
        TYPEREC* ptype = &sym->type;
        emit_sname(sym->name);
        emit_ch(' ');
        emit_str("ds ");
        uint16_t size = is_int(ptype) || is_ptr(ptype) ? 2 : 1;
        if (is_array(ptype)) size *= -ptype->dim;
        emit_n(size);
        emit_nl();        
    }
}

