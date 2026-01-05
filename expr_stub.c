#include "znc.h"
#include "farcall.h"
#include "expr.h"

EXPR_RESULT far_parse_expr(uint8_t minprec) MYCC;
EXPR_RESULT far_parse_expr_delayconst(uint8_t minprec) MYCC;
void far_parse_assign(uint8_t dereference, SYMBOL* sym, uint8_t indexed, TYPEREC typ) MYCC;

EXPR_RESULT parse_expr(uint8_t minprec) MYCC {
    PROLOG(43)
    EXPR_RESULT r = far_parse_expr(minprec);
    EPILOG_RETURN(r);
}

EXPR_RESULT parse_expr_delayconst(uint8_t minprec) MYCC {
    PROLOG(43)
    EXPR_RESULT r = far_parse_expr_delayconst(minprec);
    EPILOG_RETURN(r);
}

void parse_assign(uint8_t dereference, SYMBOL* sym, uint8_t indexed, TYPEREC typ) MYCC {
    PROLOG(43)
    far_parse_assign(dereference, sym, indexed, typ);
    EPILOG
}
