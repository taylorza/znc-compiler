#include "znc.h"

/* Enum storage and parsing compiled into BANK_51.
 * IDENT_IDs from the identifier table (BANK_45).
 */

#define MAX_ENUMS 64
#define MAX_ENUM_MEMBERS 256

typedef struct ENUM_MEMBER {
    IDENT_ID name_id;
    uint8_t enum_id;
    uint16_t value;
} ENUM_MEMBER;

typedef struct ENUMDEF {
    IDENT_ID name_id;
    uint16_t first_member;
    uint8_t membercount;
} ENUMDEF;

static ENUMDEF enum_tab[MAX_ENUMS];
static ENUM_MEMBER enum_member_pool[MAX_ENUM_MEMBERS];
static uint16_t enum_member_next = 0;
static uint8_t enum_count = 0;

static int far_find_enum(const char* name) MYCC {
    IDENT_ID name_id = intern_ident(name);
    for (int i = 0; i < enum_count; ++i) {
        if (enum_tab[i].name_id == name_id) return i;
    }
    return -1;
}

static int far_add_enum(const char* name) MYCC {
    if (enum_count >= MAX_ENUMS) {
        error(errTooManySymbols);
        return -1;
    }
    if (far_find_enum(name) != -1) return -1;
    ENUMDEF* e = &enum_tab[enum_count];
    e->name_id = intern_ident(name);
    e->first_member = 0xFFFF;
    e->membercount = 0;
    return enum_count++;
}

static void far_add_enum_member(int id, const char* name, uint16_t value) MYCC {
    if (id < 0 || id >= enum_count) return;
    if (enum_member_next >= MAX_ENUM_MEMBERS) {
        error(errTooManySymbols);
        return;
    }
    ENUMDEF* e = &enum_tab[id];
    uint16_t idx = enum_member_next++;
    ENUM_MEMBER* m = &enum_member_pool[idx];
    m->name_id = intern_ident(name);
    m->enum_id = (uint8_t)id;
    m->value = value;
    if (e->first_member == 0xFFFF) e->first_member = idx;
    e->membercount++;
}

static int far_find_enum_member(int id, const char* name, uint16_t* value_out) MYCC {
    if (id < 0 || id >= enum_count) return -1;
    ENUMDEF* e = &enum_tab[id];
    if (e->first_member == 0xFFFF) return -1;
    IDENT_ID name_id = intern_ident(name);
    uint16_t idx = e->first_member;
    for (int i = 0; i < e->membercount; ++i) {
        if (enum_member_pool[idx + i].name_id == name_id) {
            if (value_out) *value_out = enum_member_pool[idx + i].value;
            return i;
        }
    }
    return -1;
}

void far_parse_enum_member(EXPR_RESULT *result, const char* enum_name) MYCC {
    result->type_id = TYPE_ID_VOID;
    result->value = 0;
    result->has_sym = 0;

    if (tok != tokMember) {
        error(errExpected_c, '.');
        return;
    }
    get_token(); // skip '.'
    if (tok != tokIdent) {
        error(errExpected_s, "member name");
        return;
    }

    char* enum_copy = arena_strdup(enum_name, strnlen(enum_name, MAX_IDENT_LEN));
    uint8_t enum_type_id = type_find_by_name(enum_copy);
    if (enum_type_id == 0xFF || !type_is_enum(enum_type_id)) {
        error(errNotDefined_s, enum_name);
        return;
    }

    uint8_t enum_id = type_get_enum_id(enum_type_id);
    uint16_t value = 0;
    if (far_find_enum_member(enum_id, token, &value) < 0) {
        error(errNotDefined_s, token);
        return;
    }

    get_token(); // skip member name
    result->type_id = type_make_enum(enum_id, 1);
    result->value = value;
}

void far_parse_enum(void) MYCC {
    static char name[MAX_IDENT_LEN + 1];
    static char enum_name[MAX_IDENT_LEN + 1];
    uint16_t current_value = 0;

    get_token(); // skip 'enum'
    if (tok != tokIdent) {
        error(errExpected_s, "identifier");
        return;
    }

    strncpy(enum_name, token, MAX_IDENT_LEN);
    enum_name[MAX_IDENT_LEN] = '\0';

    if (far_find_enum(enum_name) != -1) {
        error(errAlreadyDefined_s, enum_name);
        return;
    }

    int eid = far_add_enum(enum_name);
    if (eid < 0) return;

    get_token(); // skip enum name
    expect_LBrace();

    while (tok != tokRBrace && tok != tokEOS) {
        if (tok != tokIdent) {
            error(errExpected_s, "member name");
            return;
        }

        strncpy(name, token, MAX_IDENT_LEN);
        name[MAX_IDENT_LEN] = '\0';
        get_token(); // skip member name

        if (tok == tokAssign) {
            get_token(); // skip '='
            expr_result = parse_expr_delayconst(0, TYPE_ID_INT);
            if (!type_is_const(expr_result.type_id)) error(errConstExpected);
            current_value = expr_result.value;
        }

        far_add_enum_member(eid, name, current_value);
        ++current_value;

        if (tok == tokComma) {
            get_token();
            if (tok == tokRBrace) break;
            continue;
        }
        break;
    }

    expect_RBrace();
    if (tok == tokSemi) get_token();

    char* enum_copy = arena_strdup(enum_name, strnlen(enum_name, MAX_IDENT_LEN));
    type_register_name(enum_copy, type_make_enum((uint8_t)eid, 0));
}
