#include "znc.h"
#include "farcall.h"

/* Declare far prototypes (defined in enum.c, BANK 51) */
void far_parse_enum(void) MYCC;
void far_parse_enum_member(EXPR_RESULT *result, const char* enum_name) MYCC;

void parse_enum_def(void) MYCC {
    PROLOG(51)
    far_parse_enum();
    EPILOG
}

void parse_enum_member(EXPR_RESULT *result, const char* enum_name) MYCC {
    ARENA_MARKER m = arena_get_marker();
    char* ncopy = arena_strdup(enum_name, strnlen(enum_name, MAX_IDENT_LEN));

    PROLOG(51)
    far_parse_enum_member(result, ncopy);
    EPILOG

    arena_free_to_marker(m);
}
