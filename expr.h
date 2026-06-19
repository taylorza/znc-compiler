#ifndef EXPR_H_
#define EXPR_H_

#define TYPE_CHAR 1
#define TYPE_INT  2
#define CLASS_POINTER 0x80

typedef struct EXPR_RESULT {
    SYMBOL sym;    /* copy of symbol when expression is a simple identifier */        
    uint16_t value;
    uint8_t type_id;
    uint8_t has_sym; /* whether sym is valid */
    
} EXPR_RESULT;

EXPR_RESULT parse_expr(uint8_t minprec, uint8_t expected_type_id) MYCC;
EXPR_RESULT parse_expr_delayconst(uint8_t minprec, uint8_t expected_type_id) MYCC;
void parse_ternary(EXPR_RESULT *result, uint8_t prec, uint8_t expected_type_id) MYCC;
void parse_enum_member(EXPR_RESULT *result, const char* enum_name) MYCC;
void parse_assign(uint8_t dereference, SYMBOL *sym, uint8_t indexed, uint8_t type_id) MYCC;
void parse_abs(EXPR_RESULT* result) MYCC;

/* Far entry points in BANK_45 (exprex.c) */
void far_parse_ternary(EXPR_RESULT *result, uint8_t prec, uint8_t expected_type_id) MYCC;
void far_parse_assign_ex(uint8_t dereference, SYMBOL *sym, uint8_t indexed, uint8_t type_id) MYCC;
void far_parse_compound_assign(TOKEN op, uint8_t dereference, SYMBOL *sym, uint8_t addr_in_hl, uint8_t type_id) MYCC;
void far_parse_abs(EXPR_RESULT* result) MYCC;

void parse_compound_assign(TOKEN op, uint8_t dereference, SYMBOL *sym, uint8_t addr_in_hl, uint8_t type_id) MYCC;

#endif // EXPR_H_