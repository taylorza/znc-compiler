#include "znc.h"
#include "struct.h"
#include "shared.h"
#include "expr.h"
#include "type.h"

/* Simple statement parsers compiled into BANK_47 to reduce the main-bank
 * footprint of compiler.c.  Every function here is a far_* implementation
 * called from a matching main-bank stub defined in compiler.c.
 *
 * Cross-bank rules observed:
 *  - All recursive parse calls go through main-bank stubs (parse_expr,
 *    parse_expr_delayconst, parse_statement, etc.) — never direct.
 *  - do_exit(), skip_statement_far(), parse_onearg()
 *    all live in the main bank — always safe to call directly.
 *  - Scanner globals (tok, token, intval, curr_line, curr_col) live in the
 *    main bank — always accessible.
 *  - Compiler globals below (infunc, func_rettype, retlbl, tokMakeType) live
 *    in compiler.c (main bank) — always accessible.
 */

/* Compiler globals from compiler.c (main bank) */
extern uint8_t  infunc;
extern uint8_t  func_rettype;
extern uint16_t retlbl;
extern TOKEN    tokMakeType;

/* Main-bank helpers declared in compiler.c */
void do_exit(EXPR_RESULT exit_expr);
void skip_statement_far(void) MYCC;
EXPR_RESULT parse_onearg(void) MYCC;

/* Main-bank parse entry points (always safe to call) */
void parse_statement(uint16_t brklbl, uint16_t contlbl) MYCC;
void parse_decl(void) MYCC;
void parse_type(uint8_t* type_id_out) MYCC;

/* Enum storage in BANK_47 */
#define MAX_ENUMS 64
#define MAX_ENUM_MEMBERS 256

typedef struct ENUM_MEMBER {
    char name[MAX_IDENT_LEN + 1];
    uint8_t enum_id;
    uint16_t value;
} ENUM_MEMBER;

typedef struct ENUMDEF {
    char name[MAX_IDENT_LEN + 1];
    uint16_t first_member;
    uint8_t membercount;
} ENUMDEF;

static ENUMDEF enum_tab[MAX_ENUMS];
static ENUM_MEMBER enum_member_pool[MAX_ENUM_MEMBERS];
static uint16_t enum_member_next = 0;
static uint8_t enum_count = 0;

static int far_find_enum(const char* name) MYCC {
    for (int i = 0; i < enum_count; ++i) {
        if (strncmp(enum_tab[i].name, name, MAX_IDENT_LEN) == 0) return i;
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
    strncpy(e->name, name, MAX_IDENT_LEN);
    e->name[MAX_IDENT_LEN] = '\0';
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
    strncpy(m->name, name, MAX_IDENT_LEN);
    m->name[MAX_IDENT_LEN] = '\0';
    m->enum_id = (uint8_t)id;
    m->value = value;
    if (e->first_member == 0xFFFF) e->first_member = idx;
    e->membercount++;
}

static int far_find_enum_member(int id, const char* name, uint16_t* value_out) MYCC {
    if (id < 0 || id >= enum_count) return -1;
    ENUMDEF* e = &enum_tab[id];
    if (e->first_member == 0xFFFF) return -1;
    uint16_t idx = e->first_member;
    for (int i = 0; i < e->membercount; ++i) {
        if (strncmp(enum_member_pool[idx + i].name, name, MAX_IDENT_LEN) == 0) {
            if (value_out) *value_out = enum_member_pool[idx + i].value;
            return i;
        }
    }
    return -1;
}

/* ------------------------------------------------------------------ */

void far_parse_struct_def(void) MYCC {
    static char name[MAX_IDENT_LEN + 1];

    /* parse: struct Name { <field-decls> } ; */
    get_token(); // skip 'struct'
    if (tok != tokIdent) {
        error(errExpected_s, "identifier");
        return;
    }

    if (find_struct(token) != -1) {
        error(errAlreadyDefined_s, token);
        return;
    }

    int sid = add_struct(token);

    get_token(); // skip name
    expect_LBrace();

    while (tok != tokRBrace && tok != tokEOS) {
        uint8_t ftype_id;
        parse_type(&ftype_id);

        for (;;) {
            if (tok != tokIdent) error(errExpected_s, "field name");

            strncpy(name, token, MAX_IDENT_LEN);
            get_token(); // skip field name

            add_struct_field(sid, name, ftype_id);

            if (tok == tokComma) {
                get_token(); // skip ',' and continue
                continue;
            }
            break;
        }
        expect_semi();
    }

    expect_RBrace();
    if (tok == tokSemi) get_token(); // optional semicolon
}

void far_parse_include(void) MYCC {
    get_token(); // skip 'include'
    if (tok != tokString) error(errExpected_s, "filename");
    /* token[] is in the main bank — safe to pass directly */
    parse(token, NULL, 0);
    get_token(); // skip filename
}

void far_parse_while(void) MYCC {
    get_token(); // skip 'while'
    expect_LParen();

    uint16_t lblCond = newlbl();
    uint16_t lblEndWhile = newlbl();
    emit_lbl(lblCond);

    expr_result = parse_expr_delayconst(0, 0);
    expect_RParen();

    if (type_is_const(expr_result.type_id) && !expr_result.value) {
        skip_statement_far(); // while(0) — skip body, emit nothing
        return;
    }

    if (!type_is_const(expr_result.type_id))
        emit_jp_false(lblEndWhile);
    parse_statement(lblEndWhile, lblCond);
    emit_jp(lblCond);

    emit_lbl(lblEndWhile);
}

void far_parse_break(uint16_t brklbl) MYCC {
    get_token(); // skip 'break'
    if (brklbl == NO_LABEL) error(errBreakOutsideLoop);
    expect_semi();
    emit_jp(brklbl);
}

void far_parse_continue(uint16_t contlbl) MYCC {
    get_token(); // skip 'continue'
    if (contlbl == NO_LABEL) error(errContinueOutsideLoop);
    expect_semi();
    emit_jp(contlbl);
}

void far_parse_putc(void) MYCC {
    parse_onearg(); // (expr)
    expect_semi();
    emit_rtl("putc");
}

void far_parse_out(void) MYCC {
    get_token(); // skip 'out'
    expect_LParen();
    parse_expr(0, 0);
    emit_copy_hl_to_bc();
    expect_comma();
    parse_expr(0, 0);
    emit_instrln("out (c),l");
    expect_RParen();
    expect_semi();
}

void far_parse_nextreg(void) MYCC {
    get_token(); // skip 'nextreg'
    expect_LParen();
    parse_expr(0, 0);
    emit_push();
    expect_comma();
    parse_expr(0, 0);
    emit_pop_de();
    emit_instrln("ld bc,9275");
    emit_instrln("out (c),e");
    emit_instrln("inc b");
    emit_instrln("out (c),l");
    expect_RParen();
    expect_semi();
}

void far_parse_return(void) MYCC {
    EXPR_RESULT expr_result = { .type_id = TYPE_ID_VOID };

    get_token(); // skip 'return'

    if (infunc) {
        if (type_is_void(func_rettype)) {
            if (tok != tokSemi) error(errReturnValueUnexpected);
        } else {
            if (tok == tokSemi) error(errReturnValueExpected);
            expr_result = parse_expr(0, func_rettype);
            if (!type_check_compatible(expr_result.type_id, func_rettype))
                error(errTypeError);
            expect_semi();
        }
        emit_jp(retlbl);
    } else {
        if (tok != tokSemi) {
            expr_result = parse_expr(0, 0);
        } else {
            expr_result.type_id = TYPE_ID_INT;
            expr_result.value = 0;
        }
        do_exit(expr_result);
    }
}

void far_parse_exit(void) MYCC {
    expr_result = parse_onearg();
    do_exit(expr_result);
}

void far_parse_vastart(void) MYCC {
    get_token(); // skip 'va_start'
    expect_LParen();
    SYMBOL valist = lookupIdent(token);
    if (valist.klass != VARIABLE) error(errTypeError);
    get_token(); // skip valist name
    if (tok == tokComma) {
        get_token(); // skip ','
        SYMBOL last_fixed = lookupIdent(token);
        if (last_fixed.klass != ARGUMENT) error(errTypeError);
        get_token(); // skip last_fixed name
        emit_ld_symaddr(&last_fixed);
    } else {
        emit_rtl("ccvafirst");
        emit_instrln("inc de");
        emit_instrln("inc de");
    }
    expect_RParen();
    expect_semi();
    emit_store_sym(&valist);
}

void far_parse_vaend(void) MYCC {
    get_token(); // skip 'va_end'
    expect_LParen();
    SYMBOL valist = lookupIdent(token);
    if (valist.klass != VARIABLE) error(errTypeError);
    get_token(); // skip valist name
    expect_RParen();
    expect_semi();
}

void far_parse_org(void) MYCC {
    get_token(); // skip 'org'
    expr_result = parse_expr_delayconst(0, 0);
    if (!type_is_const(expr_result.type_id)) error(errConstExpected);
    emit_org(expr_result.value);
    current_org = expr_result.value;
    expect_semi();
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

