#include "znc.h"
#include "farcall.h"
#include "error.h"

/* BANK_43: Type table storage and type_intern function */
/* Type table storage - accessed via stubs from type.c */
extern uint8_t type_count;
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
    if (type_count >= 255) {  /* type_count is uint8_t, max 255 */
        error(errTooManyTypes);
        return 0;  /* Return void type as fallback */
    }
    
    type_table[type_count] = entry;
    return type_count++;
}


