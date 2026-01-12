#include "znc.h"
#include "farcall.h"
#include "type.h"

/* Far implementations in BANK 43 */
void far_type_init(void) MYCC;
uint8_t far_type_make_char(uint8_t is_const) MYCC;
uint8_t far_type_make_int(uint8_t is_const) MYCC;
uint8_t far_type_make_void(void) MYCC;
uint8_t far_type_make_pointer(uint8_t base_type_id, uint8_t extra_indir) MYCC;
uint8_t far_type_make_struct(uint8_t struct_id, uint8_t is_const) MYCC;
uint8_t far_type_make_function(uint8_t signature_id) MYCC;
uint8_t far_type_make_array(uint8_t element_type_id, uint8_t length) MYCC;
TypeKind far_type_get_kind(uint8_t type_id) MYCC;
uint8_t far_type_get_indirection(uint8_t type_id) MYCC;
uint8_t far_type_is_pointer(uint8_t type_id) MYCC;
uint8_t far_type_is_array(uint8_t type_id) MYCC;
uint8_t far_type_is_const(uint8_t type_id) MYCC;
uint8_t far_type_is_void(uint8_t type_id) MYCC;
uint8_t far_type_is_char(uint8_t type_id) MYCC;
uint8_t far_type_is_int(uint8_t type_id) MYCC;
uint8_t far_type_is_struct(uint8_t type_id) MYCC;
uint8_t far_type_is_function(uint8_t type_id) MYCC;
uint8_t far_type_get_element_type(uint8_t array_type_id) MYCC;
uint8_t far_type_get_array_length(uint8_t array_type_id) MYCC;
uint8_t far_type_get_struct_id(uint8_t struct_type_id) MYCC;
uint8_t far_type_get_function_sig(uint8_t func_type_id) MYCC;
uint16_t far_type_size(uint8_t type_id) MYCC;
uint8_t far_type_get_element_type_id(uint8_t ptr_or_array_type_id) MYCC;

/* Wrappers: switch to BANK 43, call far implementations, switch back */
void type_init(void) MYCC {
    PROLOG(43)
    far_type_init();
    EPILOG
}

uint8_t type_make_char(uint8_t is_const) MYCC {
    uint8_t r;
    PROLOG(43)
    r = far_type_make_char(is_const);
    EPILOG
    return r;
}

uint8_t type_make_int(uint8_t is_const) MYCC {
    uint8_t r;
    PROLOG(43)
    r = far_type_make_int(is_const);
    EPILOG
    return r;
}

uint8_t type_make_void(void) MYCC {
    uint8_t r;
    PROLOG(43)
    r = far_type_make_void();
    EPILOG
    return r;
}

uint8_t type_make_pointer(uint8_t base_type_id, uint8_t extra_indir) MYCC {
    uint8_t r;
    PROLOG(43)
    r = far_type_make_pointer(base_type_id, extra_indir);
    EPILOG
    return r;
}

uint8_t type_make_struct(uint8_t struct_id, uint8_t is_const) MYCC {
    uint8_t r;
    PROLOG(43)
    r = far_type_make_struct(struct_id, is_const);
    EPILOG
    return r;
}

uint8_t type_make_function(uint8_t signature_id) MYCC {
    uint8_t r;
    PROLOG(43)
    r = far_type_make_function(signature_id);
    EPILOG
    return r;
}

uint8_t type_make_array(uint8_t element_type_id, uint8_t length) MYCC {
    uint8_t r;
    PROLOG(43)
    r = far_type_make_array(element_type_id, length);
    EPILOG
    return r;
}

TypeKind type_get_kind(uint8_t type_id) MYCC {
    TypeKind r;
    PROLOG(43)
    r = far_type_get_kind(type_id);
    EPILOG
    return r;
}

uint8_t type_get_indirection(uint8_t type_id) MYCC {
    uint8_t r;
    PROLOG(43)
    r = far_type_get_indirection(type_id);
    EPILOG
    return r;
}

uint8_t type_is_pointer(uint8_t type_id) MYCC {
    uint8_t r;
    PROLOG(43)
    r = far_type_is_pointer(type_id);
    EPILOG
    return r;
}

uint8_t type_is_array(uint8_t type_id) MYCC {
    uint8_t r;
    PROLOG(43)
    r = far_type_is_array(type_id);
    EPILOG
    return r;
}

uint8_t type_is_const(uint8_t type_id) MYCC {
    uint8_t r;
    PROLOG(43)
    r = far_type_is_const(type_id);
    EPILOG
    return r;
}

uint8_t type_is_void(uint8_t type_id) MYCC {
    uint8_t r;
    PROLOG(43)
    r = far_type_is_void(type_id);
    EPILOG
    return r;
}

uint8_t type_is_char(uint8_t type_id) MYCC {
    uint8_t r;
    PROLOG(43)
    r = far_type_is_char(type_id);
    EPILOG
    return r;
}

uint8_t type_is_int(uint8_t type_id) MYCC {
    uint8_t r;
    PROLOG(43)
    r = far_type_is_int(type_id);
    EPILOG
    return r;
}

uint8_t type_is_struct(uint8_t type_id) MYCC {
    uint8_t r;
    PROLOG(43)
    r = far_type_is_struct(type_id);
    EPILOG
    return r;
}

uint8_t type_is_function(uint8_t type_id) MYCC {
    uint8_t r;
    PROLOG(43)
    r = far_type_is_function(type_id);
    EPILOG
    return r;
}

uint8_t type_get_element_type(uint8_t array_type_id) MYCC {
    uint8_t r;
    PROLOG(43)
    r = far_type_get_element_type(array_type_id);
    EPILOG
    return r;
}

uint8_t type_get_array_length(uint8_t array_type_id) MYCC {
    uint8_t r;
    PROLOG(43)
    r = far_type_get_array_length(array_type_id);
    EPILOG
    return r;
}

uint8_t type_get_struct_id(uint8_t struct_type_id) MYCC {
    uint8_t r;
    PROLOG(43)
    r = far_type_get_struct_id(struct_type_id);
    EPILOG
    return r;
}

uint8_t type_get_function_sig(uint8_t func_type_id) MYCC {
    uint8_t r;
    PROLOG(43)
    r = far_type_get_function_sig(func_type_id);
    EPILOG
    return r;
}

uint16_t type_size(uint8_t type_id) MYCC {
    uint16_t r;
    PROLOG(43)
    r = far_type_size(type_id);
    EPILOG
    return r;
}

uint8_t type_get_element_type_id(uint8_t ptr_or_array_type_id) MYCC {
    uint8_t r;
    PROLOG(43)
    r = far_type_get_element_type_id(ptr_or_array_type_id);
    EPILOG
    return r;
}
