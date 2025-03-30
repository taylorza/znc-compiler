#include "znc.h"

#define ASSIGN_PREC 1
#define OR_PREC 5
#define AND_PREC 10
#define BIT_OR_PREC 15
#define BIT_XOR_PREC 20
#define BIT_AND_PREC 25
#define EQ_PREC 30
#define REL_OP_PREC 35
#define SHIFT_OP_PREC 40
#define ADD_PREC 45
#define MUL_PREC 50
#define COND_PREC 3

typedef struct OP_PREC {
    TOKEN op;
    uint8_t prec;
} OP_PREC;

OP_PREC prectbl[] = {
    {tokCond,   COND_PREC},

    {tokAssign, ASSIGN_PREC},
    
    {tokOr,     OR_PREC},
    {tokAnd,    AND_PREC},
    
    {tokBitOr,  BIT_OR_PREC},
    {tokBitXor, BIT_XOR_PREC},
    {tokBitAnd, BIT_AND_PREC},

    {tokEq,     EQ_PREC},
    {tokNeq,    EQ_PREC},

    {tokLt,     REL_OP_PREC},
    {tokLeq,    REL_OP_PREC},
    {tokGt,     REL_OP_PREC},
    {tokGeq,    REL_OP_PREC},

    {tokShl,    SHIFT_OP_PREC},
    {tokShr,    SHIFT_OP_PREC},

    {tokPlus,   ADD_PREC},
    {tokMinus,  ADD_PREC},

    {tokStar,   MUL_PREC},
    {tokDiv,    MUL_PREC},
    {tokMod,    MUL_PREC},
};

int8_t prec(TOKEN op) {
    for(uint8_t i=0; i < sizeof(prectbl)/sizeof(OP_PREC); ++i) {
        if (prectbl[i].op == op) {
            return prectbl[i].prec;
        }
    }
    return 0;
}

TYPEREC parse_ternary(uint8_t prec);
TYPEREC parse_factor(uint8_t dereference);
TYPEREC parse_binop(TOKEN op, TYPEREC ltyp, uint8_t prec);

TYPEREC parse_expr(uint8_t minprec) {
    TYPEREC ltyp = parse_factor(0);
    
    uint8_t p;
    while((p = prec(tok)) && p && p >= minprec) {
        TOKEN op = tok;
        get_token(); // skip op

        if (op == tokCond) {
            ltyp = parse_ternary(p);
        } else {
            ltyp = parse_binop(op, ltyp, p);
        }
    }
    return ltyp;
}

TYPEREC parse_ternary(uint8_t prec)
{
    uint16_t altlbl = newlbl();
    uint16_t donelbl = newlbl();
    emit_jp_false(altlbl);
    TYPEREC ptyp = parse_expr(0);  // primary expression
    emit_jp(donelbl);
    expect(tokColon, errSyntax);
    emit_lbl(altlbl);
    TYPEREC atyp = parse_expr(prec); // alternate expresion
    emit_lbl(donelbl);

    if (ptyp.basetype != atyp.basetype) error(errTypeError);
    
    return ptyp;
}

TYPEREC parse_factor(uint8_t dereference) {
    SYMBOL* sym = NULL;
    TYPEREC typ;
    uint8_t indexed = 0;
    uint8_t neg = 0;
    uint8_t loadval = 1;
    
    while (tok == tokPlus || tok == tokMinus) {
        if (tok == tokMinus) neg ^= 1;
        get_token();
    }

    switch (tok) {
        case tokLParen:
            get_token(); // skip '('
            typ = parse_expr(0);
            expect(tokRParen, errExpectRParen);
            break;

        case tokIn:
            get_token(); // skip 'in'
            expect(tokLParen, errExpectLParen);
            parse_expr(0);
            expect(tokRParen, errExpectRParen);
            emit_instrln("ld c,l");
            emit_instrln("ld b,h");
            emit_instrln("in a,(c)");
            emit_rtl("ccsxt");
            typ = char_type;
            break;

        case tokReadReg:
            get_token(); // skip 'readreg'
            expect(tokLParen, errExpectLParen);
            parse_expr(0);
            emit_instrln("ld bc, $243b");
            emit_instrln("out (c), l");
            emit_instrln("inc b");
            expect(tokRParen, errExpectRParen);            
            emit_instrln("in a,(c)");
            emit_rtl("ccsxt");
            typ = char_type;
            break;
        case tokString:
            typ = string_type;
            intval = lookupstr(token);
            emit_ld_immed(); emit_strref(intval); emit_nl();
            get_token(); // skip string
            break;

        case tokStar:
            get_token();    // skip '*'
            typ = parse_factor(1);
            if (tok == tokAssign) {
                parse_assign(0, NULL, 0, typ);
            }
            break;

        case tokNumber:
            if (neg) {
                intval = -intval;
                neg = 0;
            }
            emit_ld_immed();
            emit_n16(intval);
            emit_nl();
            get_token();
            typ = int_type;
            break;
                               
        case tokIdent:
            sym = lookupIdent(token);
            get_token(); // skip identifier
            
            if (!sym) {
                error(errNotDefined);
                return int_type;
            }

            typ = sym->type;
            if (dereference) make_scalar(&typ);

            if (tok == tokLParen) {
                parse_funccall(sym);   
                loadval = 0;
            }
            
            if (tok == tokLBrack) {
                if (!is_ptr(&typ) && !is_array(&typ)) error(errSyntax);
                get_token();    // skip '['
                parse_expr(0);   // index expression
                expect(tokRBrack, errExpectRBrack); // skip ']'
                if (is_int(&typ)) emit_mul2();
                dereference = 1;
                indexed = 1;
                emit_push(); // save index
            }

            if (tok == tokAssign) {
                parse_assign(dereference, sym, indexed, typ);
            } else {
                if (loadval) {
                    if (sym->klass == FUNCTION) {
                        emit_ld_immed(); emit_sname(sym->name); emit_nl();
                    }
                    else {
                        emit_ld_symval(sym);
                    }
                }
                    
                if (indexed) {
                    emit_pop();
                    emit_add16();
                }
                if (dereference) {
                    emit_load(typ);
                }
            }
            break; 
        default:
            error(errSyntax);            
            typ = int_type;
            break;
    }
    if (neg) emit_neg();
    return typ;
}

void parse_assign(uint8_t dereference, SYMBOL* sym, uint8_t indexed, TYPEREC typ)
{
    get_token(); // skip '='

    if (!sym) {
        // HL contains address to write to
        emit_push();
        parse_expr(0);
        emit_store(typ);
        return;
    }

    if (dereference && tok == tokLBrace) {
        get_token(); // skip '{'

        emit_ld_symval(sym);
        if (indexed) {
            emit_pop();
            emit_add16();
        }
        
        for(;;) {
            if (tok == tokNumber) {
                emit_instrln("ld (hl),%d", intval);
                if (typ.basetype == INT) {
                    emit_instrln("inc hl");
                    emit_instrln("ld (hl),%d>>8", intval);
                }
                get_token(); // skip number
            }
            else {
                emit_push();
                parse_expr(0);
                emit_swap(); // DE = value
                emit_pop();
                emit_instrln("ld (hl),e");
                if (typ.basetype == INT) {
                    emit_instrln("inc hl");
                    emit_instrln("ld (hl), d", intval);
                }
            }
            if (tok != tokComma) break;
            emit_instrln("inc hl");
            get_token(); // skip ','
        }
        expect(tokRBrace, errExpectRBrace);
       
        return;
    } else if (dereference) {
        emit_ld_symval(sym);
        emit_push();
    }
    
    parse_expr(0);

    if (dereference) {
        emit_store(typ);
    }
    else {
        emit_store_sym(sym);
    }
}

TYPEREC parse_binop(TOKEN op, TYPEREC ltyp, uint8_t opprec) {
    uint8_t scaleIdx = 0;

    if (op == tokOr || op == tokAnd) {
        uint16_t donelbl = newlbl();
        switch (op) {
            case tokOr:
                emit_jp_true(donelbl);
                parse_expr(opprec + 1);
                emit_jp_true(donelbl);
                emit_lbl(donelbl);
                break;
            case tokAnd:
                emit_jp_false(donelbl);
                parse_expr(opprec + 1);
                emit_jp_false(donelbl);
                emit_lbl(donelbl);
                break;
        }
        return ltyp;
    }

    emit_push();
    TYPEREC rtyp = parse_expr(opprec + 1);

    if (is_ptr(&ltyp) && is_ptr(&rtyp)) error(errSyntax); // cannot add two pointers
    scaleIdx = (is_ptr(&ltyp) && is_int(&ltyp));
    uint8_t pointer = is_ptr(&ltyp) || is_ptr(&rtyp);

    switch (op) {
        case tokLt:
            emit_pop();
            emit_rtl(pointer ? "ccult" : "cclt");
            break;

        case tokLeq:
            emit_pop();
            emit_rtl(pointer ? "ccule" : "ccle");
            break;

        case tokGt:
            emit_pop();
            emit_rtl(pointer ? "ccugt" : "ccgt");
            break;

        case tokGeq:
            emit_pop();
            emit_rtl(pointer ? "ccuge" : "ccge");
            break;

        case tokEq:
            emit_pop();
            emit_rtl("cceq");
            break;

        case tokNeq:
            emit_pop();
            emit_rtl("ccne");
            break;

        case tokPlus:
            if (scaleIdx) emit_mul2();
            emit_pop();
            emit_add16();
            break;
        case tokMinus:
            if (scaleIdx) emit_mul2();
            emit_pop();
            emit_sub16();
            break;
        case tokStar:
            if (pointer) error(errSyntax);
            emit_pop();
            emit_rtl("ccmult");
            break;
        case tokDiv:
            if (pointer) error(errSyntax);
            emit_pop();
            emit_rtl("ccdiv");
            break;
        case tokMod:
            if (pointer) error(errSyntax);
            emit_pop();
            emit_rtl("ccdiv");
            emit_swap();
            break;

        case tokOr:
            if (pointer) error(errSyntax);
            emit_pop();
            emit_rtl("ccor");
            break;

        case tokAnd:
            if (pointer) error(errSyntax);
            emit_pop();
            emit_rtl("ccand");
            break;

        case tokShl:
            if (pointer) error(errSyntax);
            emit_pop();             // DE = value to shift
            emit_instrln("ld b, l");  // B  = shift count
            emit_instrln("bsla de, b");
            emit_swap();            // HL = result;
            break;
        case tokShr:
            if (pointer) error(errSyntax);
            emit_pop();             // DE = value to shift
            emit_instrln("ld b, l");  // B  = shift count
            emit_instrln("bsra de, b");
            emit_swap();            // HL = result;
            break;

        case tokBitOr:
            emit_pop();
            emit_rtl("ccor");
            break;

        case tokBitAnd:
            emit_pop();
            emit_rtl("ccand");
            break;

        case tokBitXor:
            emit_pop();
            emit_rtl("ccxor");
            break;

        default:
            error(errSyntax);
            break;
    }
    return ltyp;
}
