#ifndef EXPR_H_
#define EXPR_H_

#define TYPE_CHAR 1
#define TYPE_INT  2
#define CLASS_POINTER 0x80

typedef struct EXPR_RESULT {
    uint8_t type_id;
    uint16_t value;
    SYMBOL sym;    /* copy of symbol when expression is a simple identifier */
    uint8_t has_sym; /* whether sym is valid */
} EXPR_RESULT;

EXPR_RESULT parse_expr(uint8_t minprec, uint8_t expected_type_id) MYCC;
EXPR_RESULT parse_expr_delayconst(uint8_t minprec, uint8_t expected_type_id) MYCC;
EXPR_RESULT parse_ternary(EXPR_RESULT expr_result, uint8_t prec, uint8_t expected_type_id) MYCC;
void parse_assign(uint8_t dereference, SYMBOL sym, uint8_t indexed, uint8_t type_id) MYCC;

/* Far entry points in BANK_46 (exprex.c) */
EXPR_RESULT far_parse_ternary(EXPR_RESULT expr_result, uint8_t prec, uint8_t expected_type_id) MYCC;
void far_parse_assign_ex(uint8_t dereference, SYMBOL sym, uint8_t indexed, uint8_t type_id) MYCC;

#endif // EXPR_H_