#include "znc.h"
#include "farcall.h"
#include "expr.h"

EXPR_RESULT far_parse_expr(uint8_t minprec, uint8_t expected_type_id) MYCC;
EXPR_RESULT far_parse_expr_delayconst(uint8_t minprec, uint8_t expected_type_id) MYCC;

EXPR_RESULT parse_expr(uint8_t minprec, uint8_t expected_type_id) MYCC {
    PROLOG(43)
    EXPR_RESULT r = far_parse_expr(minprec, expected_type_id);
    EPILOG_RETURN(r);
}

EXPR_RESULT parse_expr_delayconst(uint8_t minprec, uint8_t expected_type_id) MYCC {
    PROLOG(43)
    EXPR_RESULT r = far_parse_expr_delayconst(minprec, expected_type_id);
    EPILOG_RETURN(r);
}

void parse_ternary(EXPR_RESULT *result, uint8_t prec, uint8_t expected_type_id) MYCC {
    PROLOG(45)
    far_parse_ternary(result, prec, expected_type_id);
    EPILOG
}

void parse_assign(uint8_t dereference, SYMBOL *sym, uint8_t indexed, uint8_t type_id) MYCC {
    PROLOG(45)
    far_parse_assign_ex(dereference, sym, indexed, type_id);
    EPILOG
}

void parse_compound_assign(TOKEN op, uint8_t dereference, SYMBOL *sym, uint8_t addr_in_hl, uint8_t type_id) MYCC {
    PROLOG(45)
    far_parse_compound_assign(op, dereference, sym, addr_in_hl, type_id);
    EPILOG
}

void parse_abs(EXPR_RESULT* result) MYCC {
    PROLOG(45)
    far_parse_abs(result);
    EPILOG
}