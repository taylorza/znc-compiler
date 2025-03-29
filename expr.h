#ifndef EXPR_H_
#define EXPR_H_

#define TYPE_CHAR 1
#define TYPE_INT  2
#define CLASS_POINTER 0x80

int8_t prec(TOKEN op);

TYPEREC parse_expr(uint8_t minprec);

void parse_assign(uint8_t dereference, SYMBOL* sym, uint8_t indexed, TYPEREC typ);

#endif // EXPR_H_