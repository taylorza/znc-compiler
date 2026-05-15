#ifndef SYM_H_
#define SYM_H_


typedef enum SYM_CLASS { CLASS_UNDEFINED, VARIABLE, ARGUMENT, FUNCTION, FUNCTION_PROTO } SYM_CLASS;
typedef enum SYM_SCOPE { SCOPE_UNDEFINED, GLOBAL, LOCAL } SYM_SCOPE;

typedef struct VALUE {
    uint8_t type_id;
    uint16_t value;
} VALUE;

typedef struct SYMBOL {
    IDENT_ID name_id;
    uint8_t type_id;
    SYM_CLASS klass;
    SYM_SCOPE scope;

    union {
        struct {
            uint8_t arg_count;
            uint8_t signature_id;
        } fn;
        struct {
            uint16_t offset;
        } stk;
    };    
    uint8_t flags;
} SYMBOL;

/* Symbol flags */
#define SYM_FLAG_INITIALIZED 0x01

extern SYMBOL undefined_sym;

// far implementations (in banked modules) – work with IDENT_ID, not raw strings
SYMBOL* far_findglb(IDENT_ID name_id) MYCC;
SYMBOL* far_findloc(IDENT_ID name_id) MYCC;
SYMBOL* far_lookupIdent(IDENT_ID name_id) MYCC;

SYMBOL* far_addglb(IDENT_ID name_id, SYM_CLASS klass, uint8_t type_id, int16_t value) MYCC;
SYMBOL* far_addloc(IDENT_ID name_id, SYM_CLASS klass, uint8_t type_id, int16_t value) MYCC;
void far_updatesym(SYMBOL from) MYCC;

uint16_t far_push_frame(void) MYCC;
void far_pop_frame(uint16_t frame) MYCC;
uint8_t far_is_scoped(void) MYCC;

uint16_t far_get_lastgbl(void) MYCC;
void far_reset_lastgbl(uint16_t to) MYCC;
void far_dump_globals_range(uint16_t from, uint16_t to) MYCC;
void far_dump_globals(void) MYCC;

// Non-far wrappers (provided in stubs)
SYMBOL findglb(const char* name) MYCC;
SYMBOL findloc(const char* name) MYCC;
SYMBOL lookupIdent(const char* name) MYCC;

SYMBOL addglb(const char* name, SYM_CLASS klass, uint8_t type_id, int16_t value) MYCC;
SYMBOL addloc(const char* name, SYM_CLASS klass, uint8_t type_id, int16_t value) MYCC;
void updatesym(SYMBOL* from) MYCC;

uint16_t push_frame(void) MYCC;
void pop_frame(uint16_t frame) MYCC;
uint8_t is_scoped(void) MYCC;

uint16_t get_lastgbl(void) MYCC;
void reset_lastgbl(uint16_t to) MYCC;
void dump_globals_range(uint16_t from, uint16_t to) MYCC;
void dump_globals(void) MYCC;

inline uint8_t is_func_or_proto(const SYMBOL* sym) MYCC { return sym->klass == FUNCTION || sym->klass == FUNCTION_PROTO; }
inline uint8_t is_defined(const SYMBOL* sym) MYCC { return sym->klass != CLASS_UNDEFINED && sym->scope != SCOPE_UNDEFINED; }
inline uint8_t not_defined(const SYMBOL* sym) MYCC { return sym->klass == CLASS_UNDEFINED || sym->scope == SCOPE_UNDEFINED; }
#endif //SYM_H_