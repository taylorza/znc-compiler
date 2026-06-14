#include "znc.h"
#include "farcall.h"
#include "strtbl.h"
#include "shared.h"

int16_t far_lookupstr(const char* s, uint8_t len) MYCC;

size_t far_get_laststr(void) MYCC;
void far_reset_laststr(size_t to) MYCC;
void far_set_str_search_base(size_t base) MYCC;
void far_dump_strings_range(const char* label, size_t from, size_t to) MYCC;

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

size_t get_laststr(void) MYCC {
    PROLOG(40)
    size_t i = far_get_laststr();
    EPILOG_RETURN(i)
}

void reset_laststr(size_t to) MYCC {
    PROLOG(40)
    far_reset_laststr(to);
    EPILOG
}

void set_str_search_base(size_t base) MYCC {
    PROLOG(40)
    far_set_str_search_base(base);
    EPILOG
}

void dump_strings_range(const char* label, size_t from, size_t to) MYCC {
    ARENA_MARKER m = arena_get_marker();
    char* lcopy = arena_strdup(label, strnlen(label, 15));
    PROLOG(40)
    far_dump_strings_range(lcopy, from, to);
    EPILOG
    arena_free_to_marker(m);
}

