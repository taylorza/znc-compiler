#include "znc.h"

TYPEREC const void_type = { .basetype = VOID, .dim = 1 };
TYPEREC const char_type = { .basetype = CHAR, .dim = 1 };
TYPEREC const int_type = { .basetype = INT, .dim = 1 };
TYPEREC const string_type = { .basetype = STRING, .dim = 1 };

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

void make_ptr(TYPEREC* type) MYCC { type->basetype &= 0xff;  type->dim = 0; }
void make_scalar(TYPEREC* type) MYCC { type->basetype &= 0xff; type->dim = 1; }
void make_array(TYPEREC* type, uint16_t size) MYCC { type->basetype &= 0xff;  type->dim = -size; }
void make_const(TYPEREC* type) MYCC { type->basetype |= CONST; }

uint8_t is_const(const TYPEREC* type) MYCC { return (type->basetype & CONST); }
BASE_TYPE base_type(TYPEREC* type) MYCC { return type->basetype & 0x7f; }