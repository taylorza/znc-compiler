#ifndef IDENTTBL_H_
#define IDENTTBL_H_

#include <stdint.h>

/* An identifier id is an index into the ident string table (BANK 45).
 * 0xFFFF means "no/undefined identifier".
 */
#define IDENT_ID_NONE 0xFFFF

typedef uint16_t IDENT_ID;

/* Far implementations (live in BANK 45, called after bank-switch) */
IDENT_ID far_intern_ident(const char* name) MYCC;
const char* far_get_ident(IDENT_ID id) MYCC;
uint8_t far_cmp_ident(IDENT_ID id, const char* name) MYCC;
void far_get_ident_copy(IDENT_ID id, char* buf, uint8_t maxlen) MYCC;

/* Stub wrappers (main bank – safe to call from anywhere) */
IDENT_ID intern_ident(const char* name) MYCC;
void get_ident_copy(IDENT_ID id, char* buf, uint8_t maxlen) MYCC;
uint8_t cmp_ident(IDENT_ID id, const char* name) MYCC;

/* Convenience: intern the current scanner token as an ident */
IDENT_ID intern_token_ident(void) MYCC;

/* Convenience used by sym.c (BANK 41): copy ident into the shared token buffer
 * and call emit_sname.  Both helpers live in the main bank so they are always
 * reachable from a banked module without an extra bank-switch.
 */
void emit_sname_id(IDENT_ID id) MYCC;
void copy_ident_to_token(IDENT_ID id) MYCC;

#endif // IDENTTBL_H_
