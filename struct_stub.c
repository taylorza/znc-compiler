#include "znc.h"
#include "farcall.h"
#include "shared.h"
#include "struct.h"

int find_struct(const char* name) MYCC {
    ARENA_MARKER m = arena_get_marker();
    char* ncopy = arena_strdup(name, strnlen(name, MAX_IDENT_LEN));

    int r;
    PROLOG(44)
    r = far_find_struct(ncopy);
    EPILOG

    arena_free_to_marker(m);
    return r;
}

int add_struct(const char* name) MYCC {
    ARENA_MARKER m = arena_get_marker();
    char* ncopy = arena_strdup(name, strnlen(name, MAX_IDENT_LEN));

    int r;
    PROLOG(44)
    r = far_add_struct(ncopy);
    EPILOG

    arena_free_to_marker(m);
    return r;
}

void add_struct_field(int id, const char* name, TYPEREC type) MYCC {
    ARENA_MARKER m = arena_get_marker();
    char* ncopy = arena_strdup(name, strnlen(name, MAX_IDENT_LEN));

    /* Compute offset and new struct size in main bank to avoid pulling type_size into BANK_44 */
    uint16_t cur = get_struct_size(id);
    uint16_t inc = type_size(&type);
    uint16_t newsize = cur + inc;

    PROLOG(44)
    far_add_struct_field_with_offset(id, ncopy, type, cur);
    far_set_struct_size(id, newsize);
    EPILOG

    arena_free_to_marker(m);
}

int find_struct_field(int id, const char* name) MYCC {
    ARENA_MARKER m = arena_get_marker();
    char* ncopy = arena_strdup(name, strnlen(name, MAX_IDENT_LEN));

    int r;
    PROLOG(44)
    r = far_find_struct_field(id, ncopy);
    EPILOG

    arena_free_to_marker(m);
    return r;
}

uint16_t get_struct_size(int id) MYCC {
    PROLOG(44)
    uint16_t r = far_get_struct_size(id);
    EPILOG_RETURN(r)
}

FIELDINFO get_struct_field(int id, int fid) MYCC {
    PROLOG(44)
    FIELDINFO fi = far_get_struct_field(id, fid);
    EPILOG_RETURN(fi)
}
