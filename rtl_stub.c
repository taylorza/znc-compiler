#include "znc.h"
#include "farcall.h"
#include "rtl.h"
#include "shared.h"

uint8_t far_inc_rtl(const char* fn);
void far_dump_rtl(void);

// Wrapper for RTL functions compiled in BANK 42
uint8_t inc_rtl(const char* fn) MYCC {
    ARENA_MARKER m = arena_get_marker();
    /* copy into shared arena so far implementation can safely read it */
    size_t len = strlen(fn);
    char* fcopy = arena_strdup(fn, len);

    uint8_t r;
    PROLOG(42)
    r = far_inc_rtl(fcopy);
    EPILOG

    arena_free_to_marker(m);
    return r;
}

void dump_rtl(void) MYCC {
    PROLOG(42)
    far_dump_rtl();
    EPILOG
}
