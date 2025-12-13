#ifndef TYPE_H__
#define TYPE_H__

typedef enum BASE_TYPE { VOID, CHAR, INT, STRING } BASE_TYPE;
typedef enum MOD_TYPE {CONST=128} MOD_TYPE;

typedef struct  {
    BASE_TYPE basetype;
    int16_t dim; // 0-pointer, 1-scalar, <0-array
} TYPEREC;

extern TYPEREC const void_type;
extern TYPEREC const char_type;
extern TYPEREC const int_type;
extern TYPEREC const string_type;

uint16_t type_size(const TYPEREC* type) MYCC;
uint8_t is_array(const TYPEREC* type) MYCC;
uint8_t is_ptr(const TYPEREC* type) MYCC;
uint8_t is_void(const TYPEREC* type) MYCC;
uint8_t is_char(const TYPEREC* type) MYCC;
uint8_t is_int(const TYPEREC* type) MYCC;
uint8_t is_string(const TYPEREC* type) MYCC;

void make_ptr(TYPEREC* type) MYCC;
void make_scalar(TYPEREC* type) MYCC;
void make_array(TYPEREC* type, uint16_t size) MYCC;
void make_const(TYPEREC* type) MYCC;

uint8_t is_const(const TYPEREC* type) MYCC;
BASE_TYPE base_type(TYPEREC* type) MYCC;

#endif //TYPE_H__