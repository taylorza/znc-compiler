#include "znc.h"
#include "farcall.h"
#include "strtbl.h"
#include "shared.h"

int16_t far_lookupstr(const char* s, uint8_t len) MYCC;
void far_dump_strings(void) MYCC;

// Wrappers: switch to BANK 40, call far implementations, switch back
int16_t lookupstr(const char* s, uint8_t len) MYCC {
    ARENA_MARKER m = arena_get_marker();
    char* scall = arena_strdup(s, len);

    int16_t i;
    PROLOG(40)
    i = far_lookupstr(scall, len);
    EPILOG

    arena_free_to_marker(m);
    return i;
}

void dump_strings(void) MYCC {
    PROLOG(40)
    far_dump_strings();
    EPILOG
}
