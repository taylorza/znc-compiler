#include "znc.h"
#include "farcall.h"
#include "error.h"

/* BANK_46: Function signature storage and signature_intern implementation */
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
/* This function must be in BANK_46 as it directly accesses signature_table */
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
