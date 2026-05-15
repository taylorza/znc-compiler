#include "znc.h"

/* Ident string table – compiled into BANK 45.
 * Identifiers are stored as NUL-terminated strings packed sequentially,
 * exactly like strtbl.  The IDENT_ID is the byte offset of the string
 * within this table.
 *
 * Max entries: MAX_SYMBOLS (650) identifiers, each up to MAX_IDENT_LEN (14)
 * chars + NUL = 15 bytes → 650*15 = 9750 bytes worst-case, well within one
 * 8 KB bank once the code overhead is subtracted.
 */

#define MAX_IDENTTBL_SIZE 9750

static char identtbl[MAX_IDENTTBL_SIZE];
static uint16_t lastident = 0;

/* Return the IDENT_ID for 'name', adding it if not already present. */
IDENT_ID far_intern_ident(const char* name) MYCC {
    uint8_t len = (uint8_t)strnlen(name, MAX_IDENT_LEN);

    /* Search for existing entry */
    uint16_t i = 0;
    while (i < lastident) {
        const char* entry = &identtbl[i];
        if (strncmp(entry, name, MAX_IDENT_LEN) == 0) {
            return (IDENT_ID)i;
        }
        i += (uint16_t)(strlen(entry) + 1);
    }

    /* Add new entry */
    if ((uint16_t)(lastident + len + 1) > MAX_IDENTTBL_SIZE) {
        error(errTooManySymbols);
        return IDENT_ID_NONE;
    }
    IDENT_ID id = (IDENT_ID)lastident;
    strncpy(&identtbl[lastident], name, len);
    identtbl[lastident + len] = '\0';
    lastident += len + 1;
    return id;
}

/* Return a pointer to the NUL-terminated identifier string for 'id'.
 * Only safe to call while BANK 45 is mapped. */
const char* far_get_ident(IDENT_ID id) MYCC {
    if (id == IDENT_ID_NONE || id >= lastident) return "";
    return &identtbl[id];
}

/* Return 1 if the identifier at 'id' equals 'name', 0 otherwise. */
uint8_t far_cmp_ident(IDENT_ID id, const char* name) MYCC {
    if (id == IDENT_ID_NONE) return 0;
    return strncmp(&identtbl[id], name, MAX_IDENT_LEN) == 0;
}

/* Copy the identifier string at 'id' into 'buf' (max 'maxlen' chars + NUL). */
void far_get_ident_copy(IDENT_ID id, char* buf, uint8_t maxlen) MYCC {
    if (id == IDENT_ID_NONE || id >= lastident) {
        buf[0] = '\0';
        return;
    }
    strncpy(buf, &identtbl[id], maxlen);
    buf[maxlen] = '\0';
}
