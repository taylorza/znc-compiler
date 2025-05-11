#ifndef EXPR_H_
#define EXPR_H_

#define TYPE_CHAR 1
#define TYPE_INT  2
#define CLASS_POINTER 0x80

typedef struct EXPR_RESULT {
    TYPEREC type;
    uint16_t value;
} EXPR_RESULT;

int8_t prec(TOKEN op) MYCC;

EXPR_RESULT parse_expr(uint8_t minprec) MYCC;
EXPR_RESULT parse_expr_delayconst(uint8_t minprec) MYCC;

void parse_assign(uint8_t dereference, SYMBOL* sym, uint8_t indexed, TYPEREC typ) MYCC;

#endif // EXPR_H_