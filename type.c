#include "znc.h"
#include "struct.h"
#include "shared.h"
#include "error.h"

/* Forward declarations */
static uint8_t type_intern(TypeEntry entry) MYCC;
uint8_t far_type_make_char(uint8_t is_const) MYCC;
uint8_t far_type_make_int(uint8_t is_const) MYCC;
uint8_t far_type_make_void(void) MYCC;
uint8_t far_type_make_pointer(uint8_t base_type_id, uint8_t extra_indir) MYCC;

/* Global type table in shared memory */
static uint8_t type_count = 0;
static TypeEntry type_table[MAX_TYPES];

/* Predefined type IDs */
uint8_t TYPE_ID_VOID = 0;
uint8_t TYPE_ID_CHAR = 0;
uint8_t TYPE_ID_INT = 0;
uint8_t TYPE_ID_CHAR_PTR = 0;

/* Initialize type system - allocate table in shared memory */
void far_type_init(void) MYCC {
    uint8_t i;
    if (type_count != 0) return;  /* Already initialized */
    
    /* Zero out type table in case BSS not initialized */
    for (i = 0; i < MAX_TYPES; i++) {
        type_table[i].kind_and_flags = 0;
        type_table[i].aux0 = 0;
        type_table[i].aux1 = 0;
    }
        
    /* Create predefined types */
    TYPE_ID_VOID = far_type_make_void();
    TYPE_ID_CHAR = far_type_make_char(0);
    TYPE_ID_INT = far_type_make_int(0);
    TYPE_ID_CHAR_PTR = far_type_make_pointer(TYPE_ID_CHAR, 1);
}

/* Internal: find or create type entry (type interning) */
static uint8_t type_intern(TypeEntry entry) MYCC {
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
    if (type_count >= 255) {  /* type_count is uint8_t, max 255 */
        error(errTooManyTypes);
        return 0;  /* Return void type as fallback */
    }
    
    type_table[type_count] = entry;
    return type_count++;
}

/* Type constructors */
uint8_t far_type_make_char(uint8_t is_const) MYCC {
    TypeEntry entry = {0};
    TYPE_SET_KIND(entry.kind_and_flags, TK_CHAR);
    if (is_const) TYPE_SET_CONST(entry.kind_and_flags);
    return type_intern(entry);
}

uint8_t far_type_make_int(uint8_t is_const) MYCC {
    TypeEntry entry = {0};
    TYPE_SET_KIND(entry.kind_and_flags, TK_INT);
    if (is_const) TYPE_SET_CONST(entry.kind_and_flags);
    return type_intern(entry);
}

uint8_t far_type_make_void(void) MYCC {
    TypeEntry entry = {0};
    TYPE_SET_KIND(entry.kind_and_flags, TK_VOID);
    return type_intern(entry);
}

uint8_t far_type_make_pointer(uint8_t base_type_id, uint8_t extra_indir) MYCC {
    if (base_type_id >= type_count) return TYPE_ID_VOID;
    
    const TypeEntry* base = &type_table[base_type_id];
    TypeEntry entry = *base;
    
    /* Increment indirection level */
    uint8_t new_indir = TYPE_GET_INDIR(&entry) + extra_indir;
    if (new_indir > 15) {
        error(errTooManyPointerLevels);
        new_indir = 15;
    }
    TYPE_SET_INDIR(entry.kind_and_flags, new_indir);
    
    return type_intern(entry);
}

uint8_t far_type_make_struct(uint8_t struct_id, uint8_t is_const) MYCC {
    TypeEntry entry = {0};
    TYPE_SET_KIND(entry.kind_and_flags, TK_STRUCT);
    if (is_const) TYPE_SET_CONST(entry.kind_and_flags);
    entry.aux0 = struct_id;
    return type_intern(entry);
}

uint8_t far_type_make_function(uint8_t signature_id) MYCC {
    TypeEntry entry = {0};
    TYPE_SET_KIND(entry.kind_and_flags, TK_FUNCTION);
    entry.aux0 = signature_id;
    return type_intern(entry);
}

uint8_t far_type_make_array(uint8_t element_type_id, uint8_t length) MYCC {
    if (element_type_id >= type_count) return TYPE_ID_VOID;
    
    TypeEntry entry = {0};
    TYPE_SET_KIND(entry.kind_and_flags, TK_ARRAY);
    entry.aux0 = element_type_id;
    entry.aux1 = length;
    return type_intern(entry);
}

/* Type queries */
const TypeEntry* type_get(uint8_t type_id) MYCC {
    if (type_id >= type_count || type_id >= MAX_TYPES) return &type_table[0];
    return &type_table[type_id];
}

TypeKind far_type_get_kind(uint8_t type_id) MYCC {
    const TypeEntry* entry = type_get(type_id);
    return TYPE_GET_KIND(entry);
}

uint8_t far_type_get_indirection(uint8_t type_id) MYCC {
    const TypeEntry* entry = type_get(type_id);
    return TYPE_GET_INDIR(entry);
}

uint8_t far_type_is_pointer(uint8_t type_id) MYCC {
    return far_type_get_indirection(type_id) > 0;
}

uint8_t far_type_is_array(uint8_t type_id) MYCC {
    return far_type_get_kind(type_id) == TK_ARRAY && far_type_get_indirection(type_id) == 0;
}

uint8_t far_type_is_const(uint8_t type_id) MYCC {
    const TypeEntry* entry = type_get(type_id);
    return TYPE_IS_CONST(entry) ? 1 : 0;
}

uint8_t far_type_is_void(uint8_t type_id) MYCC {
    return far_type_get_kind(type_id) == TK_VOID && far_type_get_indirection(type_id) == 0;
}

uint8_t far_type_is_char(uint8_t type_id) MYCC {
    return far_type_get_kind(type_id) == TK_CHAR && far_type_get_indirection(type_id) == 0;
}

uint8_t far_type_is_int(uint8_t type_id) MYCC {
    return far_type_get_kind(type_id) == TK_INT && far_type_get_indirection(type_id) == 0;
}

uint8_t far_type_is_struct(uint8_t type_id) MYCC {
    return far_type_get_kind(type_id) == TK_STRUCT && far_type_get_indirection(type_id) == 0;
}

uint8_t far_type_is_function(uint8_t type_id) MYCC {
    return far_type_get_kind(type_id) == TK_FUNCTION && far_type_get_indirection(type_id) == 0;
}

/* Accessors */
uint8_t far_type_get_element_type(uint8_t array_type_id) MYCC {
    const TypeEntry* entry = type_get(array_type_id);
    if (TYPE_GET_KIND(entry) != TK_ARRAY) return TYPE_ID_VOID;
    return entry->aux0;
}

uint8_t far_type_get_array_length(uint8_t array_type_id) MYCC {
    const TypeEntry* entry = type_get(array_type_id);
    if (TYPE_GET_KIND(entry) != TK_ARRAY) return 0;
    return entry->aux1;
}

uint8_t far_type_get_struct_id(uint8_t struct_type_id) MYCC {
    const TypeEntry* entry = type_get(struct_type_id);
    if (TYPE_GET_KIND(entry) != TK_STRUCT) return 0;
    return entry->aux0;
}

uint8_t far_type_get_function_sig(uint8_t func_type_id) MYCC {
    const TypeEntry* entry = type_get(func_type_id);
    if (TYPE_GET_KIND(entry) != TK_FUNCTION) return 0;
    return entry->aux0;
}

/* Size calculation */
uint16_t far_type_size(uint8_t type_id) MYCC {
    const TypeEntry* entry = type_get(type_id);
    TypeKind kind = TYPE_GET_KIND(entry);
    uint8_t indir = TYPE_GET_INDIR(entry);
    
    /* All pointers are 2 bytes */
    if (indir > 0) return 2;
    
    /* Arrays */
    if (kind == TK_ARRAY) {
        uint8_t elem_type_id = entry->aux0;
        uint8_t length = entry->aux1;
        if (length == 0) return 0;  /* Unspecified length */
        return length * far_type_size(elem_type_id);
    }
    
    /* Structs */
    if (kind == TK_STRUCT) {
        uint8_t struct_id = entry->aux0;
        if (struct_id == 0) return 0;
        return get_struct_size(struct_id - 1);  /* struct_id is 1-based */
    }
    
    /* Scalars */
    if (kind == TK_CHAR) return 1;
    if (kind == TK_INT) return 2;
    
    return 0;  /* VOID, FUNCTION, etc. */
}

/* Derive element type for pointers/arrays */
uint8_t far_type_get_element_type_id(uint8_t ptr_or_array_type_id) MYCC {
    const TypeEntry* entry = type_get(ptr_or_array_type_id);
    TypeKind kind = TYPE_GET_KIND(entry);
    uint8_t indir = TYPE_GET_INDIR(entry);
    
    /* For arrays, return the stored element type */
    if (kind == TK_ARRAY && indir == 0) {
        return entry->aux0;
    }
    
    /* For pointers, create a type with one less indirection */
    if (indir > 0) {
        TypeEntry elem_entry = *entry;
        TYPE_SET_INDIR(elem_entry.kind_and_flags, indir - 1);
        return type_intern(elem_entry);
    }
    
    /* Not a pointer or array */
    return TYPE_ID_VOID;
}

