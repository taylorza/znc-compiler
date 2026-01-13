#ifndef TYPE_H__
#define TYPE_H__

/* TypeKind enumeration (3 bits, values 0-7) */
typedef enum TypeKind {
    TK_CHAR = 0,
    TK_INT = 1,
    TK_STRUCT = 2,
    TK_FUNCTION = 3,
    TK_ARRAY = 4,
    TK_VOID = 5
    /* values 6-7 reserved for future use */
} TypeKind;

/* Compact 3-byte type descriptor */
typedef struct TypeEntry {
    uint8_t kind_and_flags;  /* bits 7..5: TypeKind, bit 4: const, bits 3..0: indirection */
    uint8_t aux0;            /* STRUCT→struct_id, FUNCTION→signature_id, ARRAY→element_type_id */
    uint8_t aux1;            /* ARRAY→length (0=unspecified), others→unused */
} TypeEntry;

/* Type table - max 255 entries (uint8_t limit) */
#define MAX_TYPES 255

/* Initialize type system */
void type_init(void) MYCC;

/* Type constructors - return type_id (uint8_t) */
uint8_t type_make_char(uint8_t is_const) MYCC;
uint8_t type_make_int(uint8_t is_const) MYCC;
uint8_t type_make_void(void) MYCC;
uint8_t type_make_pointer(uint8_t base_type_id, uint8_t extra_indir) MYCC;
uint8_t type_make_struct(uint8_t struct_id, uint8_t is_const) MYCC;
uint8_t type_make_function(uint8_t signature_id) MYCC;
uint8_t type_make_array(uint8_t element_type_id, uint8_t length) MYCC;

/* Type queries - by type_id */
const TypeEntry type_get(uint8_t type_id) MYCC;
TypeKind type_get_kind(uint8_t type_id) MYCC;
uint8_t type_get_indirection(uint8_t type_id) MYCC;
uint8_t type_is_pointer(uint8_t type_id) MYCC;
uint8_t type_is_array(uint8_t type_id) MYCC;
uint8_t type_is_const(uint8_t type_id) MYCC;
uint8_t type_is_void(uint8_t type_id) MYCC;
uint8_t type_is_char(uint8_t type_id) MYCC;
uint8_t type_is_int(uint8_t type_id) MYCC;
uint8_t type_is_struct(uint8_t type_id) MYCC;
uint8_t type_is_function(uint8_t type_id) MYCC;

/* Array/struct/function accessors */
uint8_t type_get_element_type(uint8_t array_type_id) MYCC;
uint8_t type_get_array_length(uint8_t array_type_id) MYCC;
uint8_t type_get_struct_id(uint8_t struct_type_id) MYCC;
uint8_t type_get_function_sig(uint8_t func_type_id) MYCC;

/* Size calculation */
uint16_t type_size(uint8_t type_id) MYCC;

/* Pointer/array element type derivation */
uint8_t type_get_element_type_id(uint8_t ptr_or_array_type_id) MYCC;

/* Predefined type IDs (initialized by type_init) */
extern uint8_t TYPE_ID_VOID;
extern uint8_t TYPE_ID_CHAR;
extern uint8_t TYPE_ID_INT;
extern uint8_t TYPE_ID_CHAR_PTR;

#endif //TYPE_H__