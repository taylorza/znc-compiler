#ifndef TYPE_H__
#define TYPE_H__

#define SIGNATURE_INVALID MAX_SIGNATURES

/* Macros for packing/unpacking kind_and_flags byte */
#define TYPE_KIND_SHIFT      4
#define TYPE_CONST_BIT       0x08
#define TYPE_INDIR_MASK      0x07

#define TYPE_GET_KIND(entry)       ((TypeKind)(((entry).kind_and_flags >> TYPE_KIND_SHIFT) & 0x0F))
#define TYPE_GET_INDIR(entry)      ((entry).kind_and_flags & TYPE_INDIR_MASK)
#define TYPE_IS_CONST(entry)       ((entry).kind_and_flags & TYPE_CONST_BIT)

#define TYPE_SET_KIND(entry, kind)   ((entry).kind_and_flags = ((entry).kind_and_flags & 0x0F) | (((uint8_t)(kind) & 0x0F) << TYPE_KIND_SHIFT))
#define TYPE_SET_CONST(entry)        ((entry).kind_and_flags |= TYPE_CONST_BIT)
#define TYPE_SET_INDIR(entry, ind)   ((entry).kind_and_flags = ((entry).kind_and_flags & ~TYPE_INDIR_MASK) | ((uint8_t)(ind) & TYPE_INDIR_MASK))

/* TypeKind enumeration (4 bits, values 0-8) */
typedef enum TypeKind {
    TK_VOID = 0,    /* void */
    TK_CHAR = 1,    /* signed 8-bit */
    TK_BYTE = 2,    /* unsigned 8-bit */
    TK_INT = 3,     /* signed 16-bit */
    TK_FIXED = 4,   /* 16-bit fixed point Q12.4 format */
    TK_FUNCTION = 6,/* function pointer */
    TK_STRUCT = 5,  /* structure */
    TK_ARRAY = 7,   /* array */
    TK_ENUM = 8     /* named enumeration */
} TypeKind;

/* Compact type descriptor (now 4 bytes with uint16_t aux1 for larger arrays) */
typedef struct TypeEntry {
    uint8_t kind_and_flags;  /* bits 7..4: TypeKind (4 bits), bit 3: const, bits 2..0: indirection (0..7) */
    uint8_t aux0;            /* STRUCT→struct_id, FUNCTION→signature_id, ARRAY→element_type_id */
    uint16_t aux1;           /* ARRAY→length (0=unspecified), others→unused */
} TypeEntry;

/* Initialize type system */
void type_init(void) MYCC;

/* Type constructors - return type_id (uint8_t) */
uint8_t type_as_const(uint8_t type_id) MYCC;
uint8_t type_make_char(uint8_t is_const) MYCC;
uint8_t type_make_byte(uint8_t is_const) MYCC;
uint8_t type_make_int(uint8_t is_const) MYCC;
uint8_t type_make_fixed(uint8_t is_const) MYCC;
uint8_t type_make_void(void) MYCC;
uint8_t type_make_pointer(uint8_t base_type_id, uint8_t extra_indir) MYCC;
uint8_t type_make_struct(uint8_t struct_id, uint8_t is_const) MYCC;
uint8_t type_make_enum(uint8_t enum_id, uint8_t is_const) MYCC;
uint8_t type_make_function(uint8_t signature_id) MYCC;
uint8_t type_make_array(uint8_t element_type_id, uint16_t length) MYCC;

/* Type queries - by type_id */
const TypeEntry type_get(uint8_t type_id) MYCC;
TypeKind type_get_kind(uint8_t type_id) MYCC;
uint8_t type_get_indirection(uint8_t type_id) MYCC;
uint8_t type_is_pointer(uint8_t type_id) MYCC;
uint8_t type_is_array(uint8_t type_id) MYCC;
uint8_t type_is_const(uint8_t type_id) MYCC;
uint8_t type_is_void(uint8_t type_id) MYCC;
uint8_t type_is_char(uint8_t type_id) MYCC;
uint8_t type_is_byte(uint8_t type_id) MYCC;
uint8_t type_is_int(uint8_t type_id) MYCC;
uint8_t type_is_fixed(uint8_t type_id) MYCC;
uint8_t type_is_struct(uint8_t type_id) MYCC;
uint8_t type_is_enum(uint8_t type_id) MYCC;
uint8_t type_is_function(uint8_t type_id) MYCC;
uint8_t type_is_delegate(uint8_t type_id) MYCC;

uint8_t type_is_integral(uint8_t type_id) MYCC; /* char, byte, int */
uint8_t type_is_8bit(uint8_t type_id) MYCC;    /* char, byte */

/* Array/struct/function accessors */
uint8_t type_get_element_type(uint8_t array_type_id) MYCC;
uint16_t type_get_array_length(uint8_t array_type_id) MYCC;
uint8_t type_get_struct_id(uint8_t struct_type_id) MYCC;
uint8_t type_get_enum_id(uint8_t enum_type_id) MYCC;
uint8_t type_get_function_sig(uint8_t func_type_id) MYCC;

/* Type name registry (for typedef-like named types such as delegates) */
int type_find_by_name(const char* name) MYCC; /* returns type_id or -1 */
void type_register_name(const char* name, uint8_t type_id) MYCC;

/* Size calculation */
uint16_t type_size(uint8_t type_id) MYCC;

/* Pointer/array element type derivation */
uint8_t type_get_element_type_id(uint8_t ptr_or_array_type_id) MYCC;

typedef struct FuncSignature {
    uint8_t return_type_id;
    uint8_t arg_count;           /* Fixed argument count (does not include variadic args) */
    uint8_t is_variadic;         /* 1 if function is variadic, 0 otherwise */
    uint8_t calling_convention;   /* 0 = default (caller cleanup), 1 = callee cleanup (__znccall(1)) */
    uint8_t arg_types[MAX_FUNC_ARGS];
} FuncSignature;

uint8_t signature_create(uint8_t calling_convention, uint8_t return_type_id, uint8_t arg_count, const uint8_t* arg_types, uint8_t is_variadic) MYCC;
uint8_t signature_get_return_type(uint8_t sig_id) MYCC;
uint8_t signature_get_arg_count(uint8_t sig_id) MYCC;
uint8_t signature_get_arg_type(uint8_t sig_id, uint8_t arg_index) MYCC;
uint8_t signature_get_calling_convention(uint8_t sig_id) MYCC;
uint8_t signature_is_variadic(uint8_t sig_id) MYCC;
uint8_t signature_check(uint8_t sig_id1, uint8_t sig_id2) MYCC;

/* Type compatibility checking
 * Checks whether a value of `from_type_id` can be assigned to a target of
 * `to_type_id` (i.e., is `from` compatible with `to` without an explicit cast)
 */
uint8_t type_check_compatible(uint8_t from_type_id, uint8_t to_type_id) MYCC;

/* Predefined type IDs (initialized by type_init) */
extern uint8_t TYPE_ID_VOID;
extern uint8_t TYPE_ID_CHAR;
extern uint8_t TYPE_ID_BYTE;
extern uint8_t TYPE_ID_INT;
extern uint8_t TYPE_ID_FIXED;
extern uint8_t TYPE_ID_CHAR_PTR;

#endif //TYPE_H__