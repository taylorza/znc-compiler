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

int8_t prec(TOKEN op) MYCC {
    for(uint8_t i=0; i < sizeof(prectbl)/sizeof(OP_PREC); ++i) {
        if (prectbl[i].op == op) {
            return prectbl[i].prec;
        }
    }
    return 0;
}

EXPR_RESULT parse_ternary(EXPR_RESULT expr_result, uint8_t prec) MYCC;
EXPR_RESULT parse_factor(uint8_t dereference) MYCC;
EXPR_RESULT parse_binop(TOKEN op, EXPR_RESULT ltyp, uint8_t prec) MYCC;
EXPR_RESULT parse_expr_delayconst(uint8_t minprec) MYCC;

EXPR_RESULT parse_expr(uint8_t minprec) MYCC {
    EXPR_RESULT expr_result = parse_expr_delayconst(minprec);
    
    if (is_const(&expr_result.type)) {
        emit_ld_const(expr_result.value);
    }

    return expr_result;
}

EXPR_RESULT parse_expr_delayconst(uint8_t minprec) MYCC {
    EXPR_RESULT expr_result = parse_factor(0);

    uint8_t p;
    while ((p = prec(tok)) && p && p >= minprec) {
        TOKEN op = tok;
        get_token(); // skip op

        if (op == tokCond) {
            expr_result = parse_ternary(expr_result, p);
        }
        else {
            expr_result = parse_binop(op, expr_result, p);
        }
    }
    return expr_result;
}

EXPR_RESULT parse_ternary(EXPR_RESULT expr_result, uint8_t prec) MYCC {
    uint16_t altlbl = newlbl();
    uint16_t donelbl = newlbl();

    if (is_const(&expr_result.type)) {
        emit_ld_const(expr_result.value);
    }
    emit_jp_false(altlbl);
    EXPR_RESULT ptyp = parse_expr(0);  // primary expression
    emit_jp(donelbl);
    expect(tokColon, ':');
    emit_lbl(altlbl);
    EXPR_RESULT atyp = parse_expr(prec); // alternate expresion
    emit_lbl(donelbl);

    if (base_type(&ptyp.type) != base_type(&atyp.type)) error(errTypeError);
    
    expr_result.type.basetype = base_type(&ptyp.type);

    return expr_result;
}

EXPR_RESULT parse_indexer(const TYPEREC *elemtype) {
    get_token();    // skip '['
    EXPR_RESULT index_result = parse_expr_delayconst(0);   // index expression
    expect(tokRBrack, ']'); // skip ']'

    uint8_t scaleidx = is_int(elemtype);
    if (is_const(&index_result.type)) {
        emit_ld_const(index_result.value + (scaleidx * index_result.value));
    }
    else if (scaleidx) {
        emit_mul2();
    }
    return index_result;
}

EXPR_RESULT parse_factor(uint8_t dereference) MYCC {
    SYMBOL* sym = NULL;
    EXPR_RESULT factor_result = { .type = int_type };
    uint8_t indexed = 0;
    uint8_t neg = 0;
    uint8_t not = 0;
    uint8_t loadval = 1;
    uint16_t skiplbl;
    uint16_t tmplbl;
    uint16_t counter = 0;
    
    while (tok == tokPlus || tok == tokMinus || tok == tokNot) {
        if (tok == tokMinus) neg ^= 1;
        if (tok == tokNot) not ^= 1;
        get_token();
    }

    switch (tok) {
        case tokLBrace:
            get_token(); // skip '{'
            skiplbl = newlbl();
            emit_jp(skiplbl);
            tmplbl = newlbl();
            emit_lbl(tmplbl);
            while (tok != tokRBrace) {
                EXPR_RESULT element = parse_expr_delayconst(0);
                if (!is_const(&element.type)) error_expect_const();
                if (counter++ > 0) emit_ch(','); else emit_instr("db ");
                emit_n(element.value);
                if (tok == tokRBrace) break;
                expect_comma();
                if (counter == 8) {
                    emit_nl();                    
                    counter = 0;
                }
            }
            if (counter) emit_nl();
            expect_RBrace();
            emit_lbl(skiplbl);
            emit_ld_immed(); emit_lblref(tmplbl); emit_nl();
            factor_result.type.basetype = CHAR;
            make_ptr(&factor_result.type);
            break;
        case tokLParen:
            get_token(); // skip '('
            factor_result = parse_expr_delayconst(0);
            expect_RParen();
            break;

        case tokIn:
            get_token(); // skip 'in'
            expect_LParen();
            parse_expr(0);
            expect_RParen();
            emit_instrln("ld b,h");
            emit_instrln("ld c,l");
            emit_instrln("in a,(c)");
            emit_rtl("ccsxt");
            factor_result.type = char_type;
            break;

        case tokReadReg:
            get_token(); // skip 'readreg'
            expect_LParen();
            parse_expr(0);
            emit_instrln("ld bc,9275");
            emit_instrln("out (c),l");
            emit_instrln("inc b");
            expect_RParen();            
            emit_instrln("in a,(c)");
            emit_rtl("ccsxt");
            factor_result.type = char_type;
            break;

        case tokString:
            factor_result.type = string_type;
            intval = lookupstr(token);
            emit_ld_immed(); emit_strref(intval); emit_nl();
            get_token(); // skip string
            break;

        case tokAmp:
            get_token(); // skip '&'
            if (tok != tokIdent) error(errNotlvalue);
            sym = lookupIdent(token);
            if (!sym) {
                error(errNotDefined_s, token);
                return factor_result;
            }
            get_token(); // skip identifier
            factor_result.type = sym->type;

            if (tok == tokLBrack) {
                if (is_ptr(&factor_result.type)) {
                    emit_ld_symval(sym);
                } else if (is_array(&factor_result.type) || sym->klass == FUNCTION) {
                    emit_ld_symaddr(sym);
                    make_ptr(&factor_result.type);
                } else {
                    error(errSyntax);
                }
                emit_push();
                parse_indexer(&factor_result.type);
                emit_pop();
                emit_add16();
            } else {
                emit_ld_symaddr(sym);                
            }            
            make_ptr(&factor_result.type);
            break;

        case tokStar:
            get_token();    // skip '*'
            factor_result = parse_factor(1);
            if (tok == tokAssign) {
                parse_assign(0, NULL, 0, factor_result.type);
            }
            break;

        case tokNumber:
            if (neg) {
                intval = -intval;
                neg = 0;
            }
            if (not) {
                intval = !intval;
                not = 0;
            }
            get_token();
            factor_result.type = int_type;
            factor_result.value = intval;
            make_const(&factor_result.type);
            break;
                               
        case tokIdent:
            sym = lookupIdent(token);
            if (!sym) {
                error(errNotDefined_s, token);
                return factor_result;
            }

            get_token(); // skip identifier

            factor_result.type = sym->type;
            if (is_const(&sym->type)) {
                factor_result.value = sym->offset;
                return factor_result;
            }
            if (dereference) make_scalar(&factor_result.type);

            if (tok == tokLParen) {
                parse_funccall(sym);   
                loadval = 0;
            }
            
            if (tok == tokLBrack) {
                if (!is_ptr(&factor_result.type) && !is_array(&factor_result.type)) error(errSyntax);
                parse_indexer(&factor_result.type);

                dereference = 1;
                indexed = 1;
                emit_push(); // save index
                make_scalar(&factor_result.type);
            }

            if (tok == tokAssign) {
                parse_assign(dereference, sym, indexed, factor_result.type);
            } else {
                if (loadval) {
                    if (is_func_or_proto(sym)) {
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
                    emit_load(factor_result.type);
                }
            }
            break; 
        default:
            error(errSyntax);            
            break;
    }
    if (neg) emit_neg();
    if (not) emit_rtl("ccnot");
    return factor_result;
}

void parse_assign(uint8_t dereference, SYMBOL* sym, uint8_t indexed, TYPEREC typ) MYCC {
    get_token(); // skip '='

    if (sym && is_const(&sym->type)) {
        EXPR_RESULT r = parse_expr_delayconst(0);
        if (!is_const(&r.type)) error_expect_const();
        sym->offset = r.value;
        return;
    }

    if (!sym) {
        // HL contains address to write to
        emit_push();
        parse_expr(0);
        emit_store(typ);
        return;
    }
    
    if (dereference) {
        emit_ld_symval(sym);
        if (indexed) {
            emit_pop();
            emit_add16();
        }

        if (tok == tokLBrace) {
            get_token(); // skip '{'

            for (;;) {
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
            expect_RBrace();

            return;
        } else {
            emit_push();
        }
    }
    parse_expr(0);

    if (dereference) {
        emit_store(typ);
    }
    else {
        emit_store_sym(sym);
    }
}

EXPR_RESULT parse_binop(TOKEN op, EXPR_RESULT l_result, uint8_t opprec) MYCC {
    uint8_t scaleL = 0;
    uint8_t scaleR = 0;
    
    EXPR_RESULT r_result = { .type = void_type};
    EXPR_RESULT short_circuit_result = { .type = int_type };

    if (op == tokOr || op == tokAnd) {   
        uint16_t short_circuit_lbl = newlbl();
        uint8_t short_circuit = 0;
        if (is_const(&l_result.type)) {
            emit_ld_const(l_result.value);
            if (op == tokOr) {
                short_circuit = l_result.value != 0;               
            }
            else if (op == tokAnd) {
                short_circuit = l_result.value == 0;                
            }            
        }
        
        if (!short_circuit) {
            switch (op) {
                case tokOr:
                    emit_jp_true(short_circuit_lbl);
                    break;
                case tokAnd:
                    emit_jp_false(short_circuit_lbl);
                    break;
            }
        } else {
            emit_jp(short_circuit_lbl);
        }
        l_result = parse_expr(opprec + 1);
        emit_lbl(short_circuit_lbl);

        return short_circuit_result;
    }

    if (!is_const(&l_result.type)) {
        emit_push();        
    }

    r_result = parse_expr_delayconst(opprec + 1);

    scaleR = (is_ptr(&l_result.type) && is_int(&l_result.type));
    scaleL = (is_ptr(&r_result.type) && is_int(&r_result.type));
    if (scaleL && scaleR) error(errSyntax);

    uint8_t pointer = is_ptr(&l_result.type) || is_ptr(&r_result.type);

    if (pointer) {
        // Check for invalid pointer operations
        switch(op) {
        case tokStar:
        case tokDiv:
        case tokMod:
        case tokOr:
        case tokAnd:
        case tokShl:
        case tokShr:
            error(errSyntax);
            break;
        }
    }

    if (is_const(&l_result.type) && is_const(&r_result.type)) {
        switch (op) {
            case tokLt: 
                if (pointer) l_result.value = l_result.value < r_result.value; 
                else l_result.value = (int16_t)l_result.value < (int16_t)r_result.value; 
                break;
            case tokLeq: if (pointer) l_result.value = l_result.value <= r_result.value; 
            else l_result.value = (int16_t)l_result.value <= (int16_t)r_result.value; 
            break;
            case tokGt: 
                if (pointer) l_result.value = l_result.value > r_result.value; 
                else l_result.value = (int16_t)l_result.value > (int16_t)r_result.value; 
                break;
            case tokGeq: 
                if (pointer) l_result.value = l_result.value >= r_result.value; 
                else l_result.value = (int16_t)l_result.value >= (int16_t)r_result.value; 
                break;
            case tokEq: l_result.value = l_result.value == r_result.value; break;
            case tokNeq: l_result.value = l_result.value != r_result.value; break;
            case tokPlus: l_result.value = (l_result.value + (l_result.value * scaleL)) + (r_result.value + (r_result.value * scaleR)); break;
            case tokMinus: l_result.value = (l_result.value + (l_result.value * scaleL)) - (r_result.value + (r_result.value * scaleR)); break;
            case tokStar: l_result.value = l_result.value * r_result.value; break;
            case tokDiv: l_result.value = l_result.value / r_result.value; break;
            case tokMod: l_result.value = l_result.value % r_result.value; break;
            case tokOr: l_result.value = l_result.value | r_result.value; break;
            case tokAnd: l_result.value = l_result.value & r_result.value; break;
            case tokShl: l_result.value = l_result.value << r_result.value; break;
            case tokShr: l_result.value = l_result.value >> r_result.value; break;
            case tokBitOr: l_result.value = l_result.value | r_result.value; break;
            case tokBitAnd: l_result.value = l_result.value & r_result.value; break;
            case tokBitXor: l_result.value = l_result.value ^ r_result.value; break;
            default:
                error(errSyntax);
                break;
        }        
        return l_result;
    } 
    
    if (is_const(&l_result.type)) {
        emit_ldde_immed();
        emit_n(l_result.value + (l_result.value * scaleL));
        scaleL = 0;
        emit_nl();
        l_result.type.basetype = base_type(&l_result.type);
    }
    else if (is_const(&r_result.type)) {
        emit_ld_immed();
        emit_n((r_result.value + (r_result.value * scaleR)));
        scaleR = 0;
        emit_nl();
        r_result.type.basetype = base_type(&r_result.type);
        emit_pop();
    } else {
        emit_pop();
    }

    switch (op) {
        case tokLt:
            emit_rtl(pointer ? "ccult" : "cclt");
            break;

        case tokLeq:
            emit_rtl(pointer ? "ccule" : "ccle");
            break;

        case tokGt:
            emit_rtl(pointer ? "ccugt" : "ccgt");
            break;

        case tokGeq:
            emit_rtl(pointer ? "ccuge" : "ccge");
            break;

        case tokEq:
            emit_rtl("cceq");
            break;

        case tokNeq:
            emit_rtl("ccne");
            break;

        case tokPlus:
            if (scaleR) emit_mul2();
            if (scaleL) emit_mulDE2();
            emit_add16();
            break;
        case tokMinus:
            if (scaleR) emit_mul2();
            if (scaleL) emit_mulDE2();
            emit_sub16();
            break;
        case tokStar:
            emit_rtl("ccmult");
            break;
        case tokDiv:
            emit_rtl("ccdiv");
            break;
        case tokMod:
            emit_rtl("ccdiv");
            emit_swap();
            break;

        case tokOr:
            emit_rtl("ccor");
            break;

        case tokAnd:
            emit_rtl("ccand");
            break;

        case tokShl:
            emit_instrln("ld b,l");  // B  = shift count
            emit_instrln("bsla de,b");
            emit_swap();            // HL = result;
            break;
        case tokShr:
            emit_instrln("ld b,l");  // B  = shift count
            emit_instrln("bsra de,b");
            emit_swap();            // HL = result;
            break;

        case tokBitOr:
            emit_rtl("ccor");
            break;

        case tokBitAnd:
            emit_rtl("ccand");
            break;

        case tokBitXor:
            emit_rtl("ccxor");
            break;

        default:
            error(errSyntax);
            break;
    }
    return l_result;
}
