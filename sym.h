#ifndef SYM_H_
#define SYM_H_

typedef enum BASE_TYPE { VOID, CHAR, INT, STRING } BASE_TYPE;
typedef enum SYM_CLASS { VARIABLE, ARGUMENT, FUNCTION, FUNCTION_PROTO } SYM_CLASS;
typedef enum SYM_SCOPE { GLOBAL, LOCAL } SYM_SCOPE;

typedef struct  {
    BASE_TYPE basetype;
    int16_t dim; // 0-pointer, 1-scalar, <0-array
} TYPEREC;

extern TYPEREC const int_void;
extern TYPEREC const int_type;
extern TYPEREC const char_type;
extern TYPEREC const string_type;

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

uint16_t type_size(const TYPEREC* type);
uint8_t is_array(const TYPEREC* type);
uint8_t is_ptr(const TYPEREC* type);
uint8_t is_void(const TYPEREC* type);
uint8_t is_char(const TYPEREC* type);
uint8_t is_int(const TYPEREC* type);
uint8_t is_string(const TYPEREC* type);

uint8_t is_func_or_proto(const SYMBOL* sym);

void make_ptr(TYPEREC* type);
void make_scalar(TYPEREC* type);
void make_array(TYPEREC* type, uint16_t size);

SYMBOL* findglb(const char* name);
SYMBOL* findloc(const char* name);
SYMBOL* lookupIdent(const char* name);

SYMBOL* addglb(const char* name, SYM_CLASS klass, TYPEREC type, int16_t value);
SYMBOL* addloc(const char* name, SYM_CLASS klass, TYPEREC type, int16_t value);

uint16_t push_frame(void);
void pop_frame(uint16_t frame);
uint8_t is_scoped(void);

void dump_globals(void);

#endif //SYM_H_