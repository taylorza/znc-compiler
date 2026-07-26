#ifndef SYM_H_
#define SYM_H_

//typedef enum SYM_CLASS { CLASS_UNDEFINED, VARIABLE, ARGUMENT, FUNCTION, FUNCTION_PROTO } SYM_CLASS;
//typedef enum SYM_SCOPE { SCOPE_UNDEFINED, GLOBAL, LOCAL } SYM_SCOPE;

#define IS_DEFINED(sym) ((sym).class_scope)
#define IS_UNDEFINED(sym) ((sym).class_scope == CLASS_UNDEFINED)
#define IS_VARIABLE(sym) (((sym).class_scope & VARIABLE) == VARIABLE)
#define IS_ARGUMENT(sym) (((sym).class_scope & ARGUMENT) == ARGUMENT)
#define IS_FUNCTION(sym) (((sym).class_scope & FUNCTION) == FUNCTION)
#define IS_FUNCTION_PROTO(sym) (((sym).class_scope & FUNCTION_PROTO) == FUNCTION_PROTO)
#define IS_GLOBAL(sym) (((sym).class_scope & GLOBAL) == GLOBAL)
#define IS_LOCAL(sym) (((sym).class_scope & LOCAL) == LOCAL)

typedef enum SYM_CLASS_SCOPE {
    CLASS_UNDEFINED = 0,
    VARIABLE = 1,
    ARGUMENT = 2,
    FUNCTION = 4,
    FUNCTION_PROTO = 8,
    GLOBAL = 32,
    LOCAL = 64
} SYM_CLASS_SCOPE;

typedef struct SYMBOL {
    IDENT_ID name_id;
    
    union {
        struct {
            uint8_t arg_count;
            uint8_t signature_id;
        } fn;
        struct {
            uint16_t offset;
        } stk;
    };
    uint8_t type_id;
    SYM_CLASS_SCOPE class_scope;
    uint8_t flags;
    uint8_t bank;     // bank number this symbol (function) was declared in (0 = main bank)    
} SYMBOL;

/* Symbol flags */
#define SYM_FLAG_INITIALIZED 0x01
#define SYM_FLAG_USED        0x02

extern SYMBOL undefined_sym;

// far implementations (in banked modules) – work with IDENT_ID, not raw strings
SYMBOL* far_findglb(IDENT_ID name_id) MYCC;
SYMBOL* far_findloc(IDENT_ID name_id) MYCC;
SYMBOL* far_lookupIdent(IDENT_ID name_id) MYCC;

SYMBOL* far_addglb(IDENT_ID name_id, SYM_CLASS_SCOPE klass, uint8_t type_id, int16_t value) MYCC;
SYMBOL* far_addloc(IDENT_ID name_id, SYM_CLASS_SCOPE klass, uint8_t type_id, int16_t value) MYCC;
void far_updatesym(SYMBOL from) MYCC;

uint16_t far_push_frame(void) MYCC;
void far_pop_frame(uint16_t frame) MYCC;
uint8_t far_is_scoped(void) MYCC;

uint16_t far_get_lastgbl(void) MYCC;
void far_reset_lastgbl(uint16_t to) MYCC;
void far_dump_globals_range(uint16_t from, uint16_t to) MYCC;
void far_dump_function_dependencies(void) MYCC;
void far_check_undefined(void) MYCC;

// Non-far wrappers (provided in stubs)
SYMBOL findglb(const char* name) MYCC;
SYMBOL findloc(const char* name) MYCC;
SYMBOL lookupIdent(const char* name) MYCC;

SYMBOL addglb(const char* name, SYM_CLASS_SCOPE klass, uint8_t type_id, int16_t value) MYCC;
SYMBOL addloc(const char* name, SYM_CLASS_SCOPE klass, uint8_t type_id, int16_t value) MYCC;
void updatesym(SYMBOL* from) MYCC;

uint16_t push_frame(void) MYCC;
void pop_frame(uint16_t frame) MYCC;
uint8_t is_scoped(void) MYCC;

uint16_t get_lastgbl(void) MYCC;
void reset_lastgbl(uint16_t to) MYCC;
void dump_globals_range(uint16_t from, uint16_t to) MYCC;
void dump_globals(void) MYCC;
void dump_function_dependencies(void) MYCC;
void check_undefined(void) MYCC;

inline uint8_t is_func_or_proto(const SYMBOL* sym) MYCC { return IS_FUNCTION(*sym) || IS_FUNCTION_PROTO(*sym); }
inline uint8_t is_defined(const SYMBOL* sym) MYCC { return sym->class_scope; /*IS_DEFINED(*sym);*/ }
inline uint8_t not_defined(const SYMBOL* sym) MYCC { return sym->class_scope == 0; /*IS_UNDEFINED(*sym);*/ }
#endif //SYM_H_