#include "znc.h"
#include "struct.h"
#include "shared.h"
#include "expr.h"

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

/* ------------------------------------------------------------------ */

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

    EXPR_RESULT cond = parse_expr_delayconst(0, 0);
    expect_RParen();

    if (type_is_const(cond.type_id) && !cond.value) {
        skip_statement_far(); // while(0) — skip body, emit nothing
        return;
    }

    if (!type_is_const(cond.type_id))
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
    EXPR_RESULT expr_result = parse_onearg();
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
    EXPR_RESULT expr_result = parse_expr_delayconst(0, 0);
    if (!type_is_const(expr_result.type_id)) error(errConstExpected);
    emit_org(expr_result.value);
    expect_semi();
}

