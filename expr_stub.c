#include "znc.h"
#include "farcall.h"
#include "expr.h"

EXPR_RESULT far_parse_expr(uint8_t minprec, uint8_t expected_type_id) MYCC;
EXPR_RESULT far_parse_expr_delayconst(uint8_t minprec, uint8_t expected_type_id) MYCC;
void far_parse_assign(uint8_t dereference, SYMBOL sym, uint8_t indexed, uint8_t type_id) MYCC;

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

void parse_assign(uint8_t dereference, SYMBOL sym, uint8_t indexed, uint8_t type_id) MYCC {
    PROLOG(43)
    far_parse_assign(dereference, sym, indexed, type_id);
    EPILOG
}
