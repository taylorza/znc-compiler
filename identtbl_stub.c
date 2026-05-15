#include "znc.h"
#include "farcall.h"
#include "identtbl.h"
#include "shared.h"
#include "codegen.h"

/* Declare far prototypes (defined in identtbl.c, BANK 45) */
IDENT_ID far_intern_ident(const char* name) MYCC;
const char* far_get_ident(IDENT_ID id) MYCC;
uint8_t far_cmp_ident(IDENT_ID id, const char* name) MYCC;
void far_get_ident_copy(IDENT_ID id, char* buf, uint8_t maxlen) MYCC;

/* intern_ident: switch to BANK 45, intern the name, return IDENT_ID */
IDENT_ID intern_ident(const char* name) MYCC {
    ARENA_MARKER m = arena_get_marker();
    char* ncopy = arena_strdup(name, strnlen(name, MAX_IDENT_LEN));

    IDENT_ID id;
    PROLOG(45)
    id = far_intern_ident(ncopy);
    EPILOG

    arena_free_to_marker(m);
    return id;
}

/* get_ident_copy: switch to BANK 45, copy the ident string into caller's buf */
void get_ident_copy(IDENT_ID id, char* buf, uint8_t maxlen) MYCC {
    PROLOG(45)
    far_get_ident_copy(id, buf, maxlen);
    EPILOG
}

/* cmp_ident: switch to BANK 45, compare ident against a name string */
uint8_t cmp_ident(IDENT_ID id, const char* name) MYCC {
    ARENA_MARKER m = arena_get_marker();
    char* ncopy = arena_strdup(name, strnlen(name, MAX_IDENT_LEN));

    uint8_t result;
    PROLOG(45)
    result = far_cmp_ident(id, ncopy);
    EPILOG

    arena_free_to_marker(m);
    return result;
}

/* intern_token_ident: convenience wrapper – intern the current scanner token */
IDENT_ID intern_token_ident(void) MYCC {
    return intern_ident(token);
}

/* copy_ident_to_token: copy an ident string into the shared token[] buffer.
 * Called from sym.c (BANK 41) – emit_sname / error helpers need the name in
 * the main bank's token buffer before they can use it.
 */
void copy_ident_to_token(IDENT_ID id) MYCC {
    get_ident_copy(id, token, MAX_STR_LEN);
}

/* emit_sname_id: emit the assembly name prefix for an ident id.
 * Fetches the string from BANK 45, then calls emit_sname (main bank).
 * Safe to call from any banked module.
 */
void emit_sname_id(IDENT_ID id) MYCC {
    /* Use a small static buffer in the main bank – avoids arena overhead for
     * what is a very frequent hot-path call during code generation.
     */
    static char namebuf[MAX_IDENT_LEN + 1];
    get_ident_copy(id, namebuf, MAX_IDENT_LEN);
    emit_sname(namebuf);
}
