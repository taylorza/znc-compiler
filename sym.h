#ifndef SYM_H_
#define SYM_H_


typedef enum SYM_CLASS { CLASS_UNDEFINED, VARIABLE, ARGUMENT, FUNCTION, FUNCTION_PROTO } SYM_CLASS;
typedef enum SYM_SCOPE { SCOPE_UNDEFINED, GLOBAL, LOCAL } SYM_SCOPE;

typedef struct VALUE {
    TYPEREC type;
    uint16_t value;
} VALUE;

typedef struct SYMBOL {
    char name[MAX_IDENT_LEN+1];
    TYPEREC type;
    SYM_CLASS klass;
    SYM_SCOPE scope;
    int16_t offset;
} SYMBOL;

extern SYMBOL undefined_sym;

// far implementations (in banked modules)
SYMBOL* far_findglb(const char* name) MYCC;
SYMBOL* far_findloc(const char* name) MYCC;
SYMBOL* far_lookupIdent(const char* name) MYCC;

SYMBOL* far_addglb(const char* name, SYM_CLASS klass, TYPEREC type, int16_t value) MYCC;
SYMBOL* far_addloc(const char* name, SYM_CLASS klass, TYPEREC type, int16_t value) MYCC;
void far_updatesym(SYMBOL* from) MYCC;

uint16_t far_push_frame(void) MYCC;
void far_pop_frame(uint16_t frame) MYCC;
uint8_t far_is_scoped(void) MYCC;

void far_dump_globals(void) MYCC;

// Non-far wrappers (provided in stubs)
SYMBOL findglb(const char* name) MYCC;
SYMBOL findloc(const char* name) MYCC;
SYMBOL lookupIdent(const char* name) MYCC;

SYMBOL addglb(const char* name, SYM_CLASS klass, TYPEREC type, int16_t value) MYCC;
SYMBOL addloc(const char* name, SYM_CLASS klass, TYPEREC type, int16_t value) MYCC;
void updatesym(SYMBOL* from) MYCC;

uint16_t push_frame(void) MYCC;
void pop_frame(uint16_t frame) MYCC;
uint8_t is_scoped(void) MYCC;

void dump_globals(void) MYCC;

inline uint8_t is_func_or_proto(const SYMBOL* sym) MYCC { return sym->klass == FUNCTION || sym->klass == FUNCTION_PROTO; }
inline uint8_t is_defined(const SYMBOL* sym) MYCC { return sym->klass != CLASS_UNDEFINED && sym->scope != SCOPE_UNDEFINED; }
inline uint8_t not_defined(const SYMBOL* sym) MYCC { return sym->klass == CLASS_UNDEFINED || sym->scope == SCOPE_UNDEFINED; }
#endif //SYM_H_