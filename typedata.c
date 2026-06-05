#include "znc.h"
#include "farcall.h"
#include "error.h"

/* BANK_43: Type table storage and type_intern function */
/* Type table storage - accessed via stubs from type.c */
extern uint8_t type_count;
extern uint8_t signature_count;
TypeEntry type_table[MAX_TYPES];

/* Internal: find or create type entry (type interning) */
/* This function must be in BANK_43 as it directly accesses type_table */
uint8_t type_intern(TypeEntry entry) MYCC {
    uint8_t i;
    
    /* Search for existing identical type */
    for (i = 0; i < type_count; i++) {
        if (type_table[i].kind_and_flags == entry.kind_and_flags &&
            type_table[i].aux0 == entry.aux0 &&
            type_table[i].aux1 == entry.aux1) {
            return i;
        }
    }
    
    /* Not found - add new entry */
    if (type_count >= MAX_TYPES) {  /* type_count is uint8_t, max 255 */
        error(errTooManyTypes);
        return 0;  /* Return void type as fallback */
    }
    
    type_table[type_count] = entry;
    return type_count++;
}

/* BANK_43: Function signature storage */
/* Maximum arguments per function and maximum signatures */
#define MAX_FUNC_ARGS 8
#define MAX_SIGNATURES 255

/* Function signature structure - stores return type and argument types */
typedef struct FuncSignature {
    uint8_t return_type_id;
    uint8_t arg_count;           /* Fixed argument count (does not include variadic args) */
    uint8_t is_variadic;         /* 1 if function is variadic, 0 otherwise */
    uint8_t arg_types[MAX_FUNC_ARGS];
} FuncSignature;

extern uint8_t signature_count;
FuncSignature signature_table[MAX_SIGNATURES];

/* Internal: find or create function signature (signature interning) */
/* This function must be in BANK_43 as it directly accesses signature_table */
uint8_t signature_intern_impl(uint8_t return_type_id, uint8_t arg_count, const uint8_t* arg_types, uint8_t is_variadic) MYCC {
    uint8_t i, j;
    
    /* Search for existing identical signature */
    for (i = 0; i < signature_count; i++) {
        if (signature_table[i].return_type_id == return_type_id &&
            signature_table[i].arg_count == arg_count &&
            signature_table[i].is_variadic == is_variadic) {
            /* Check if all argument types match */
            uint8_t match = 1;
            for (j = 0; j < arg_count; j++) {
                if (signature_table[i].arg_types[j] != arg_types[j]) {
                    match = 0;
                    break;
                }
            }
            if (match) return i;
        }
    }
    
    /* Not found - add new entry */
    if (signature_count >= MAX_SIGNATURES) {
        error(errTooManyTypes);
        return 0;  /* Return empty signature as fallback */
    }
    
    /* Store the new signature */
    signature_table[signature_count].return_type_id = return_type_id;
    signature_table[signature_count].arg_count = arg_count;
    signature_table[signature_count].is_variadic = is_variadic;
    for (j = 0; j < arg_count && j < MAX_FUNC_ARGS; j++) {
        signature_table[signature_count].arg_types[j] = arg_types[j];
    }
    
    return signature_count++;
}

/* Legacy wrapper - non-variadic signature */
uint8_t signature_intern(uint8_t return_type_id, uint8_t arg_count, const uint8_t* arg_types) MYCC {
    return signature_intern_impl(return_type_id, arg_count, arg_types, 0);
}

/* Variadic signature creation */
uint8_t signature_intern_variadic(uint8_t return_type_id, uint8_t arg_count, const uint8_t* arg_types) MYCC {
    return signature_intern_impl(return_type_id, arg_count, arg_types, 1);
}

/* Internal: peel indirection levels to get the base (non-pointer) type id */
static uint8_t peel_to_base(uint8_t type_id) MYCC {
    while (type_get_indirection(type_id) > 0) type_id = type_get_element_type_id(type_id);
    return type_id;
}

/* BANK_43: Type compatibility checking
 * to_type_id: target type (assignment destination)
 * from_type_id: source type (value being assigned)
 * This function must be in BANK_43 as it directly accesses type_table */
uint8_t far_type_check_compatible(uint8_t to_type_id, uint8_t from_type_id) MYCC {
    TypeEntry t1, t2;
    uint8_t indir_to, indir_from;
    uint8_t kind_to, kind_from;
    
    /* Same type ID is always compatible */
    if (to_type_id == from_type_id) {
        return 1;
    }

    /* Validate type IDs */
    if (to_type_id >= type_count) return 0;
    if (from_type_id >= type_count) return 0;

    /* Get type entries */
    t1 = type_table[to_type_id];
    t2 = type_table[from_type_id];

    /* Extract indirection level and kind */
    indir_to = t1.kind_and_flags & 0x0F;
    indir_from = t2.kind_and_flags & 0x0F;
    kind_to = (t1.kind_and_flags >> 5) & 0x07;
    kind_from = (t2.kind_and_flags >> 5) & 0x07;
    
    /* Enums: only same-enum compatibility is strict; enum/scalar mixing is allowed. */
    uint8_t is_enum_to = (kind_to == TK_ENUM) ? 1 : 0;
    uint8_t is_enum_from = (kind_from == TK_ENUM) ? 1 : 0;
    if (is_enum_to || is_enum_from) {
        if (is_enum_to && is_enum_from) return (t1.aux0 == t2.aux0) ? 1 : 0;
        if (indir_to == 0 && indir_from == 0) {
            uint8_t scalar_to = (kind_to == TK_CHAR || kind_to == TK_BYTE || kind_to == TK_INT || kind_to == TK_FIXED);
            uint8_t scalar_from = (kind_from == TK_CHAR || kind_from == TK_BYTE || kind_from == TK_INT || kind_from == TK_FIXED);
            if (scalar_to || scalar_from) return 1;
        }
        return 0;
    }

    /* For non-pointer scalars (char/byte/int/fixed), allow compatibility between them */
    if (indir_to == 0 && indir_from == 0 && 
        (kind_to == TK_CHAR || kind_to == TK_BYTE || kind_to == TK_INT || kind_to == TK_FIXED) && 
        (kind_from == TK_CHAR || kind_from == TK_BYTE || kind_from == TK_INT || kind_from == TK_FIXED)) {
        /* Scalars are compatible in both directions */
        return 1;
    }

    /* Detect arrays regardless of indirection (arrays may degrade to pointers) */
    uint8_t is_array_to = (kind_to == TK_ARRAY) ? 1 : 0;
    uint8_t is_array_from = (kind_from == TK_ARRAY) ? 1 : 0;

    /* Array-to-array: require exact match of element type AND length */
    if (is_array_to && is_array_from) {
        if (t1.aux0 == t2.aux0 && t1.aux1 == t2.aux1) return 1;
        return 0;
    }

    /* Reject passing a non-array, non-pointer value to an array parameter */
    if (is_array_to && !is_array_from && indir_from == 0) return 0;
    if (is_array_from && !is_array_to && indir_to == 0) return 0;

    /* Array <-> Pointer: treat T[] and T* as interchangeable (C semantics).
     * Both array->pointer and pointer->array are allowed when element types match. */
    if (is_array_to && indir_from > 0) {
        /* pointer passed to array parameter: allow if element types match */
        uint8_t elem_to   = t1.aux0;
        uint8_t elem_from = type_get_element_type_id(from_type_id);
        if (elem_to == elem_from) return 1;
    }
    if (is_array_from && indir_to > 0) {
        /* array passed to pointer parameter: allow if element types match */
        uint8_t elem_from = t2.aux0;
        uint8_t elem_to   = type_get_element_type_id(to_type_id);
        if (elem_to == elem_from) return 1;
    }

    /* Pointer-pointer compatibility */
    if (indir_to > 0 && indir_from > 0) {
        /* Allow any pointer to or from void* */
        if (kind_to == TK_VOID || kind_from == TK_VOID) return 1;
        /* Indirection levels must match */
        if (indir_to != indir_from) return 0;

        /* Compare base types ignoring const qualification */
        uint8_t base_to = peel_to_base(to_type_id);
        uint8_t base_from = peel_to_base(from_type_id);

        TypeEntry bt = type_table[base_to];
        TypeEntry bf = type_table[base_from];
        uint8_t bt_kind = (bt.kind_and_flags >> 5) & 0x07;
        uint8_t bf_kind = (bf.kind_and_flags >> 5) & 0x07;
        if (bt_kind != bf_kind) return 0;
        /* For structs, ensure struct ids match */
        if (bt_kind == TK_STRUCT && bt.aux0 != bf.aux0) return 0;
        /* For arrays as base (rare), require identical element types */
        if (bt_kind == TK_ARRAY && (bt.aux0 != bf.aux0 || bt.aux1 != bf.aux1)) return 0;
        /* For scalars (char/int), kinds already match; ignore const bit */
        return 1;
    }
    
    /* Pointer<->non-pointer combinations
     * - Allow assigning a scalar (char/int) to a pointer (e.g., p = 1234)
     *   because we treat integers as addresses when initializing pointers.
     * - Do NOT allow assigning a pointer to a scalar (e.g., int p; x = p)
     *   as that would drop the pointer value into a scalar type.
     */
    if (indir_to > 0 && indir_from == 0) {
        /* Target is pointer, source is non-pointer: allow scalar -> pointer */
        if (kind_from == TK_CHAR || kind_from == TK_BYTE || kind_from == TK_INT) return 1;
        return 0;
    }
    if (indir_from > 0 && indir_to == 0) {
        /* Source is pointer, target is non-pointer: disallow */
        return 0;
    }
    
    /* Structs and other types must be exact matches */
    if (t1.kind_and_flags != t2.kind_and_flags) return 0;
    if (t1.aux0 != t2.aux0) return 0;
    if (t1.aux1 != t2.aux1) return 0;

    return 1;
}


