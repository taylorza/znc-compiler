#include "znc.h"
#include "struct.h"
#include "shared.h"
#include "error.h"
#include "farcall.h"

/* Type table storage is in typedata.c (BANK_43) to save shared memory */
/* Access to the banked type table goes through stub functions */

/* Stub functions to access banked type table - defined in type_stub.c */
extern TypeEntry type_read_from_bank(uint8_t type_id) MYCC;
extern void type_write_to_bank(uint8_t type_id, TypeEntry entry) MYCC;

/* Predefined type IDs - in shared memory so accessible from all banks */
uint8_t TYPE_ID_VOID = 0;
uint8_t TYPE_ID_CHAR = 0;
uint8_t TYPE_ID_INT = 0;
uint8_t TYPE_ID_FIXED = 0;
uint8_t TYPE_ID_CHAR_PTR = 0;

uint8_t type_count = 0;
uint8_t signature_count = 0;

/* Macros for packing/unpacking kind_and_flags byte */
#define TYPE_KIND_SHIFT      5
#define TYPE_CONST_BIT       0x10
#define TYPE_INDIR_MASK      0x0F

#define TYPE_GET_KIND(entry)       ((TypeKind)(((entry)->kind_and_flags >> TYPE_KIND_SHIFT) & 0x07))
#define TYPE_GET_INDIR(entry)      ((entry)->kind_and_flags & TYPE_INDIR_MASK)
#define TYPE_IS_CONST(entry)       ((entry)->kind_and_flags & TYPE_CONST_BIT)

#define TYPE_SET_KIND(kaf, kind)   ((kaf) = ((kaf) & ~(0x07 << TYPE_KIND_SHIFT)) | (((uint8_t)(kind) & 0x07) << TYPE_KIND_SHIFT))
#define TYPE_SET_CONST(kaf)        ((kaf) |= TYPE_CONST_BIT)
#define TYPE_SET_INDIR(kaf, ind)   ((kaf) = ((kaf) & ~TYPE_INDIR_MASK) | ((uint8_t)(ind) & TYPE_INDIR_MASK))

uint8_t far_type_intern(TypeEntry entry) MYCC;

/* Initialize type system - allocate table in shared memory */
void type_init(void) MYCC {
    uint8_t i;
    if (type_count != 0) return;  /* Already initialized */
    
    /* Zero out type table in case BSS not initialized */
    type_count = 0;
    for (i = 0; i < MAX_TYPES; i++) {
        TypeEntry empty = {0};
        type_write_to_bank(i, empty);
    }
        
    /* Create predefined types */
    TYPE_ID_VOID = type_make_void();
    TYPE_ID_CHAR = type_make_char(0);
    TYPE_ID_INT = type_make_int(0);
    TYPE_ID_FIXED = type_make_fixed(0);
    TYPE_ID_CHAR_PTR = type_make_pointer(TYPE_ID_CHAR, 1);
}

uint8_t type_as_const(uint8_t type_id) MYCC {
    TypeEntry entry = type_get(type_id);
    if (!TYPE_IS_CONST(&entry)) {
        TYPE_SET_CONST(entry.kind_and_flags);
        return far_type_intern(entry);
    }
    return type_id;  /* Already const */
}   

/* Type constructors */
uint8_t type_make_char(uint8_t is_const) MYCC {
    TypeEntry entry = {0};
    TYPE_SET_KIND(entry.kind_and_flags, TK_CHAR);
    if (is_const) TYPE_SET_CONST(entry.kind_and_flags);
    return far_type_intern(entry);
}

uint8_t type_make_int(uint8_t is_const) MYCC {
    TypeEntry entry = {0};
    TYPE_SET_KIND(entry.kind_and_flags, TK_INT);
    if (is_const) TYPE_SET_CONST(entry.kind_and_flags);
    return far_type_intern(entry);
}

uint8_t type_make_fixed(uint8_t is_const) MYCC {
    TypeEntry entry = {0};
    TYPE_SET_KIND(entry.kind_and_flags, TK_FIXED);
    if (is_const) TYPE_SET_CONST(entry.kind_and_flags);
    return far_type_intern(entry);
}

uint8_t type_make_void(void) MYCC {
    TypeEntry entry = {0};
    TYPE_SET_KIND(entry.kind_and_flags, TK_VOID);
    return far_type_intern(entry);
}

uint8_t type_make_pointer(uint8_t base_type_id, uint8_t extra_indir) MYCC {
    if (base_type_id >= type_count) return TYPE_ID_VOID;
    
    TypeEntry base = type_read_from_bank(base_type_id);
    TypeEntry entry = base;
    
    /* Increment indirection level */
    uint8_t new_indir = TYPE_GET_INDIR(&entry) + extra_indir;
    if (new_indir > 15) {
        error(errTooManyPointerLevels);
        new_indir = 15;
    }
    TYPE_SET_INDIR(entry.kind_and_flags, new_indir);
    
    return far_type_intern(entry);
}

uint8_t type_make_struct(uint8_t struct_id, uint8_t is_const) MYCC {
    TypeEntry entry = {0};
    TYPE_SET_KIND(entry.kind_and_flags, TK_STRUCT);
    if (is_const) TYPE_SET_CONST(entry.kind_and_flags);
    entry.aux0 = struct_id;
    return far_type_intern(entry);
}

uint8_t type_make_function(uint8_t signature_id) MYCC {
    TypeEntry entry = {0};
    TYPE_SET_KIND(entry.kind_and_flags, TK_FUNCTION);
    entry.aux0 = signature_id;
    return far_type_intern(entry);
}

uint8_t type_make_array(uint8_t element_type_id, uint16_t length) MYCC {
    if (element_type_id >= type_count) return TYPE_ID_VOID;
    
    TypeEntry entry = {0};
    TYPE_SET_KIND(entry.kind_and_flags, TK_ARRAY);
    entry.aux0 = element_type_id;
    entry.aux1 = length;
    return far_type_intern(entry);
}

/* Type queries */
const TypeEntry type_get(uint8_t type_id) MYCC {
    TypeEntry entry;
    if (type_id >= type_count || type_id >= MAX_TYPES) {
        entry = type_read_from_bank(0);
    } else {
        entry = type_read_from_bank(type_id);
    }
    return entry;
}

TypeKind type_get_kind(uint8_t type_id) MYCC {
    TypeEntry entry = type_get(type_id);
    return TYPE_GET_KIND(&entry);
}

uint8_t type_get_indirection(uint8_t type_id) MYCC {
    TypeEntry entry = type_get(type_id);
    return TYPE_GET_INDIR(&entry);
}

uint8_t type_is_pointer(uint8_t type_id) MYCC {
    return type_get_indirection(type_id) > 0;
}

uint8_t type_is_array(uint8_t type_id) MYCC {
    return type_get_kind(type_id) == TK_ARRAY && type_get_indirection(type_id) == 0;
}

uint8_t type_is_const(uint8_t type_id) MYCC {
    TypeEntry entry = type_get(type_id);
    return TYPE_IS_CONST(&entry) ? 1 : 0;
}

uint8_t type_is_void(uint8_t type_id) MYCC {
    return type_get_kind(type_id) == TK_VOID && type_get_indirection(type_id) == 0;
}

uint8_t type_is_char(uint8_t type_id) MYCC {
    return type_get_kind(type_id) == TK_CHAR && type_get_indirection(type_id) == 0;
}

uint8_t type_is_int(uint8_t type_id) MYCC {
    return type_get_kind(type_id) == TK_INT && type_get_indirection(type_id) == 0;
}

uint8_t type_is_fixed(uint8_t type_id) MYCC {
    return type_get_kind(type_id) == TK_FIXED && type_get_indirection(type_id) == 0;
}

uint8_t type_is_struct(uint8_t type_id) MYCC {
    return type_get_kind(type_id) == TK_STRUCT && type_get_indirection(type_id) == 0;
}

uint8_t type_is_function(uint8_t type_id) MYCC {
    return type_get_kind(type_id) == TK_FUNCTION && type_get_indirection(type_id) == 0;
}

uint8_t type_is_delegate(uint8_t type_id) MYCC {
    return type_get_kind(type_id) == TK_FUNCTION && type_get_indirection(type_id) > 0;
}

/* Accessors */
uint8_t type_get_element_type(uint8_t array_type_id) MYCC {
    TypeEntry entry = type_get(array_type_id);
    if (TYPE_GET_KIND(&entry) != TK_ARRAY) return TYPE_ID_VOID;
    return entry.aux0;
}

uint16_t type_get_array_length(uint8_t array_type_id) MYCC {
    TypeEntry entry = type_get(array_type_id);
    if (TYPE_GET_KIND(&entry) != TK_ARRAY) return 0;
    return entry.aux1;
}

uint8_t type_get_struct_id(uint8_t struct_type_id) MYCC {
    TypeEntry entry = type_get(struct_type_id);
    if (TYPE_GET_KIND(&entry) != TK_STRUCT) return 0;
    return entry.aux0;
}

uint8_t type_get_function_sig(uint8_t func_type_id) MYCC {
    TypeEntry entry = type_get(func_type_id);
    if (TYPE_GET_KIND(&entry) != TK_FUNCTION) return 0;
    return entry.aux0;
}

/* Size calculation */
uint16_t type_size(uint8_t type_id) MYCC {
    TypeEntry entry = type_get(type_id);
    TypeKind kind = TYPE_GET_KIND(&entry);
    uint8_t indir = TYPE_GET_INDIR(&entry);
    
    /* All pointers are 2 bytes */
    if (indir > 0) return 2;
    
    /* Arrays */
    if (kind == TK_ARRAY) {
        uint8_t elem_type_id = entry.aux0;
        uint16_t length = entry.aux1;
        if (length == 0) return 0;  /* Unspecified length */
        return length * type_size(elem_type_id);
    }
    
    /* Structs */
    if (kind == TK_STRUCT) {
        uint8_t struct_id = entry.aux0;
        if (struct_id == 0) return 0;
        return get_struct_size(struct_id - 1);  /* struct_id is 1-based */
    }
    
    /* Scalars */
    if (kind == TK_CHAR) return 1;
    if (kind == TK_INT) return 2;
    if (kind == TK_FIXED) return 2;  /* 16-bit fixed point 12.4 */
    
    return 0;  /* VOID, FUNCTION, etc. */
}

/* Derive element type for pointers/arrays */
uint8_t type_get_element_type_id(uint8_t ptr_or_array_type_id) MYCC {
    TypeEntry entry = type_get(ptr_or_array_type_id);
    TypeKind kind = TYPE_GET_KIND(&entry);
    uint8_t indir = TYPE_GET_INDIR(&entry);
    
    /* For arrays, return the stored element type */
    if (kind == TK_ARRAY && indir == 0) {
        return entry.aux0;
    }
    
    /* For pointers, create a type with one less indirection */
    if (indir > 0) {
        TypeEntry elem_entry = entry;
        TYPE_SET_INDIR(elem_entry.kind_and_flags, indir - 1);
        return far_type_intern(elem_entry);
    }
    
    /* Not a pointer or array */
    return TYPE_ID_VOID;
}


/* Stub functions to access type table in BANK_43 */
/* These are the only functions that directly access the banked type_table */
extern TypeEntry type_table[MAX_TYPES];

/* Read a type entry from the banked type table */
TypeEntry type_read_from_bank(uint8_t type_id) MYCC {
    PROLOG(43)
    TypeEntry entry = type_table[type_id];
    EPILOG_RETURN(entry);
}

/* Write a type entry to the banked type table */
void type_write_to_bank(uint8_t type_id, TypeEntry entry) MYCC {
    PROLOG(43)
    type_table[type_id] = entry;
    EPILOG
}

/* Wrapper to call type_intern from other banks */
extern uint8_t type_intern(TypeEntry entry) MYCC;
uint8_t far_type_intern(TypeEntry entry) MYCC {
    PROLOG(43)
    uint8_t result = type_intern(entry);
    EPILOG_RETURN(result);
}

/* Function signature storage - MAX_FUNC_ARGS and MAX_SIGNATURES defined in typedata.c */
#define MAX_FUNC_ARGS 8

typedef struct FuncSignature {
    uint8_t return_type_id;
    uint8_t arg_count;           /* Fixed argument count (does not include variadic args) */
    uint8_t is_variadic;         /* 1 if function is variadic, 0 otherwise */
    uint8_t arg_types[MAX_FUNC_ARGS];
} FuncSignature;

extern FuncSignature signature_table[];
extern uint8_t signature_count;

/* Stub functions to access signature table in BANK_43 */
extern uint8_t signature_intern(uint8_t return_type_id, uint8_t arg_count, const uint8_t* arg_types) MYCC;
extern uint8_t signature_intern_variadic(uint8_t return_type_id, uint8_t arg_count, const uint8_t* arg_types) MYCC;

/* Read a signature entry from the banked signature table */
FuncSignature signature_read_from_bank(uint8_t sig_id) MYCC {
    PROLOG(43)
    FuncSignature sig = signature_table[sig_id];
    EPILOG_RETURN(sig);
}

/* Wrapper to call signature_intern from other banks */
uint8_t far_signature_intern(uint8_t return_type_id, uint8_t arg_count, const uint8_t* arg_types) MYCC {
    PROLOG(43)
    uint8_t result = signature_intern(return_type_id, arg_count, arg_types);
    EPILOG_RETURN(result);
}

/* Wrapper to call signature_intern_variadic from other banks */
uint8_t far_signature_intern_variadic(uint8_t return_type_id, uint8_t arg_count, const uint8_t* arg_types) MYCC {
    PROLOG(43)
    uint8_t result = signature_intern_variadic(return_type_id, arg_count, arg_types);
    EPILOG_RETURN(result);
}

/* Function signature API */
uint8_t signature_create(uint8_t return_type_id, uint8_t arg_count, const uint8_t* arg_types) MYCC {
    if (arg_count > MAX_FUNC_ARGS) {
        error(errTooManyArgs);
        arg_count = MAX_FUNC_ARGS;
    }
    return far_signature_intern(return_type_id, arg_count, arg_types);
}

uint8_t signature_create_variadic(uint8_t return_type_id, uint8_t arg_count, const uint8_t* arg_types) MYCC {
    if (arg_count > MAX_FUNC_ARGS) {
        error(errTooManyArgs);
        arg_count = MAX_FUNC_ARGS;
    }
    return far_signature_intern_variadic(return_type_id, arg_count, arg_types);
}

uint8_t signature_get_return_type(uint8_t sig_id) MYCC {
    if (sig_id >= signature_count) return TYPE_ID_VOID;
    FuncSignature sig = signature_read_from_bank(sig_id);
    return sig.return_type_id;
}

uint8_t signature_get_arg_count(uint8_t sig_id) MYCC {
    if (sig_id >= signature_count) return 0;
    FuncSignature sig = signature_read_from_bank(sig_id);
    return sig.arg_count;
}

uint8_t signature_get_arg_type(uint8_t sig_id, uint8_t arg_index) MYCC {
    if (sig_id >= signature_count) return TYPE_ID_VOID;
    FuncSignature sig = signature_read_from_bank(sig_id);
    if (arg_index >= sig.arg_count) return TYPE_ID_VOID;
    return sig.arg_types[arg_index];
}

uint8_t signature_is_variadic(uint8_t sig_id) MYCC {
    if (sig_id >= signature_count) return 0;
    FuncSignature sig = signature_read_from_bank(sig_id);
    return sig.is_variadic;
}

uint8_t signature_check(uint8_t sig_id1, uint8_t sig_id2) MYCC {
    if (sig_id1 >= signature_count || sig_id2 >= signature_count) return 0;
    FuncSignature sig1 = signature_read_from_bank(sig_id1);
    FuncSignature sig2 = signature_read_from_bank(sig_id2);
    
    if (sig1.arg_count != sig2.arg_count) return 0;

    /* Check if sig2 (from) is compatible with sig1 (to) */
    if (!type_check_compatible(sig2.return_type_id, sig1.return_type_id)) return 0;

    for (uint8_t i = 0; i < sig1.arg_count; i++) {
        if (!type_check_compatible(sig2.arg_types[i], sig1.arg_types[i])) return 0;
    }
    
    return 1; /* Signatures match */
}

/* Type compatibility checking */
extern uint8_t far_type_check_compatible(uint8_t to_type_id, uint8_t from_type_id) MYCC;
uint8_t type_check_compatible(uint8_t from_type_id, uint8_t to_type_id) MYCC {
    PROLOG(43)
    uint8_t result = far_type_check_compatible(to_type_id, from_type_id);
    EPILOG_RETURN(result);
}

/* Named-type registry is implemented in banked typedata.c; wrappers below call
 * the banked implementations. */
extern int type_find_by_name_bank(const char* name) MYCC;
extern void type_register_name_bank(const char* name, uint8_t type_id) MYCC;

int type_find_by_name(const char* name) MYCC {
    PROLOG(44)
    int res = type_find_by_name_bank(name);
    EPILOG_RETURN(res);
}

void type_register_name(const char* name, uint8_t type_id) MYCC {
    PROLOG(44)
    type_register_name_bank(name, type_id);
    EPILOG
}