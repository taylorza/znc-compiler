#include "znc.h"
#include "error.h"

/* BANK_44: Named-type registry for delegate/type names.
 * Moved here from typedata.c (BANK_43) to relieve bank pressure.
 * These functions do not access type_table or signature_table directly.
 */

#define MAX_NAMED_TYPES 255
static char type_name_table[MAX_NAMED_TYPES][MAX_IDENT_LEN+1];
static int16_t type_name_id[MAX_NAMED_TYPES];
static uint8_t type_name_count = 0;

int type_find_by_name_bank(const char* name) MYCC {
    uint8_t i;
    for (i = 0; i < type_name_count; ++i) {
        if (strncmp(type_name_table[i], name, MAX_IDENT_LEN) == 0) return type_name_id[i];
    }
    return -1;
}

void type_register_name_bank(const char* name, uint8_t type_id) MYCC {
    if (type_name_count >= MAX_NAMED_TYPES) {
        error(errTooManySymbols);
        return;
    }
    /* Prevent duplicate registration */
    if (type_find_by_name_bank(name) != -1) {
        error(errAlreadyDefined_s, name);
        return;
    }
    strncpy(type_name_table[type_name_count], name, MAX_IDENT_LEN);
    type_name_table[type_name_count][MAX_IDENT_LEN] = '\0';
    type_name_id[type_name_count] = type_id;
    ++type_name_count;
}
