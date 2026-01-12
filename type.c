#include "znc.h"
#include "struct.h"

TYPEREC const void_type = { .basetype = VOID, .dim = 1, .struct_id = 0 };
TYPEREC const char_type = { .basetype = CHAR, .dim = 1, .struct_id = 0 };
TYPEREC const int_type = { .basetype = INT, .dim = 1, .struct_id = 0 };
TYPEREC const string_type = { .basetype = STRING, .dim = 1, .struct_id = 0 };

uint16_t type_size(const TYPEREC* type) MYCC {
    if (is_array(type)) {
        if (type->basetype == STRUCT && type->struct_id) {
            /* arrays of struct - use struct size */
            return (-type->dim) * get_struct_size(type->struct_id - 1);
        }
        return (-type->dim) * (type->basetype == INT ? 2 : 1);
    }
    /* Pointers are always 2 bytes regardless of what they point to */
    if (is_ptr(type)) return 2;

    if (type->basetype == STRUCT && type->struct_id) {
        return get_struct_size(type->struct_id - 1);
    }
    
    /* Scalars: char/byte is 1 byte, int is 2 bytes */
    return ((type->basetype & 0xff) == CHAR) ? 1 : 2;
}

uint8_t is_array(const TYPEREC* type) MYCC { return type->dim < 0; }
uint8_t is_ptr(const TYPEREC* type) MYCC { return type->dim == 0; }
uint8_t is_void(const TYPEREC* type) MYCC { return (type->basetype & 0xff) == VOID; }
uint8_t is_char(const TYPEREC* type) MYCC { return (type->basetype & 0xff) == CHAR; }
uint8_t is_int(const TYPEREC* type) MYCC { return (type->basetype & 0xff) == INT; }
uint8_t is_string(const TYPEREC* type) MYCC { return (type->basetype & 0xff) == STRING; }
uint8_t is_struct(const TYPEREC* type) MYCC { return (type->basetype & 0xff) == STRUCT && type->struct_id != 0; }

void make_ptr(TYPEREC* type) MYCC { type->basetype &= 0xff;  type->dim = 0; }
void make_scalar(TYPEREC* type) MYCC { type->basetype &= 0xff; type->dim = 1; }
void make_array(TYPEREC* type, uint16_t size) MYCC { type->basetype &= 0xff;  type->dim = -size; }
void make_const(TYPEREC* type) MYCC { type->basetype |= CONST; }

void set_struct(TYPEREC* type, uint16_t struct_id) MYCC { type->basetype = STRUCT; type->struct_id = struct_id; }


uint8_t is_const(const TYPEREC* type) MYCC { return (type->basetype & CONST); }
BASE_TYPE base_type(TYPEREC* type) MYCC { return type->basetype & 0x7f; }