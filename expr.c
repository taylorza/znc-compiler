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

    //{tokAssign, ASSIGN_PREC},
    
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

    uint16_t scale = is_int(elemtype) ? 2 : 1;

    if (is_const(&index_result.type)) {
        emit_ld_const((uint16_t)(index_result.value * scale));
    }
    else {
        if (scale == 2) {
            emit_mul2();
        }
        /* fall through; implement
         * general multiplication here when needed.
         */
    }
    return index_result;
}

EXPR_RESULT parse_factor(uint8_t dereference) MYCC {
    SYMBOL sym;
    EXPR_RESULT factor_result = { .type = int_type };
    uint8_t indexed = 0;
    uint8_t prefix_inc = 0;
    uint8_t prefix_dec = 0;
    uint8_t neg = 0;
    uint8_t not = 0;
    uint8_t cmpl = 0;
    uint8_t loadval = 1;
    uint16_t skiplbl;
    uint16_t tmplbl;
    uint16_t counter = 0;
    
    /* handle prefix ++/-- */
    while (tok == tokInc || tok == tokDec) {
        if (tok == tokInc) prefix_inc = 1;
        if (tok == tokDec) prefix_dec = 1;
        get_token();
    }

    while (tok == tokPlus || tok == tokMinus || tok == tokNot || tok == tokBitNot) {
        if (tok == tokMinus) neg ^= 1;
        if (tok == tokNot) not ^= 1;
        if (tok == tokBitNot) cmpl ^= 1;
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
            emit_copy_hl_to_bc();
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

        case tokString: {
            factor_result.type = string_type;
            char str[MAX_STR_LEN << 1];
            char* p = str;
            while (tok == tokString) {
                memcpy(p, token, token_length);
                p += token_length;
                get_token();
            }                
            *p = '\0';
            intval = lookupstr(str, p - str);
            emit_ld_immed(); emit_strref(intval); emit_nl();

            break;
        }

        case tokAmp:
            get_token(); // skip '&'
            if (tok != tokIdent) error(errNotlvalue);
            sym = lookupIdent(token);
            if (not_defined(&sym)) error(errNotDefined_s, token);
                
            get_token(); // skip identifier
            factor_result.type = sym.type;

            if (tok == tokLBrack) {
                if (is_ptr(&factor_result.type)) {
                    emit_ld_symval(&sym);
                } else if (is_array(&factor_result.type) || sym.klass == FUNCTION) {
                    emit_ld_symaddr(&sym);
                    make_ptr(&factor_result.type);
                } else {
                    error(errSyntax);
                }
                emit_push();
                parse_indexer(&factor_result.type);
                emit_pop();
                emit_add16();
            } else {
                emit_ld_symaddr(&sym);
            }            
            make_ptr(&factor_result.type);
            break;

        case tokStar:
            get_token();    // skip '*'
            factor_result = parse_factor(1);
            if (is_const(&factor_result.type)) {
                emit_ld_const(factor_result.value);
                factor_result.type.basetype = base_type(&factor_result.type);
                make_ptr(&factor_result.type);
            }
            if (tok == tokAssign) {
                parse_assign(is_ptr(&factor_result.type), NULL, 0, factor_result.type);
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
            if (dereference) {
                emit_ld_const(intval);
                emit_load(int_type);
                factor_result.type = int_type;                
            } 
            else {
                factor_result.type = int_type;
                factor_result.value = intval;
                make_const(&factor_result.type);
            }
            break;
                               
        case tokIdent:
            sym = lookupIdent(token);
            if (not_defined(&sym)) {
                error(errNotDefined_s, token);
                return factor_result;
            }

            get_token(); // skip identifier

            factor_result.type = sym.type;
            if (is_const(&sym.type)) {
                factor_result.value = sym.offset;
                if (neg) factor_result.value = -factor_result.value;
                if (not) factor_result.value = !factor_result.value;
                if (cmpl) factor_result.value = ~factor_result.value;
                return factor_result;
            }
            if (dereference) make_scalar(&factor_result.type);

            if (tok == tokLParen) {
                parse_funccall(&sym);   
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

            /* handle prefix ++/-- for identifiers (supports indexed lvalues)
             * For indexed lvalues the parse_indexer already pushed the index on the stack.
             */
            if ((prefix_inc || prefix_dec)) {
                if (is_const(&sym.type)) error(errSyntax);
                uint16_t step = 1;
                if (indexed) {
                    /* incrementing the element value -> numeric step = 1 */
                    step = 1;
                } else {
                    /* simple identifier: if it's a pointer, step is element size */
                    if (is_ptr(&sym.type)) {
                        step = is_int(&sym.type) ? 2 : 1;
                    } else {
                        step = 1;
                    }
                }
                if (indexed) {
                    /* compute effective address into HL */
                    if (is_ptr(&sym.type)) {
                        emit_ld_symval(&sym); /* pointer value = base */
                    } else {
                        emit_ld_symaddr(&sym); /* array base address */
                    }
                    emit_pop(); /* pop index into DE */
                    emit_add16(); /* HL = base + index */

                    emit_push(); /* push address for store */
                    emit_load(factor_result.type); /* HL = original value */

                    if (prefix_dec) {
                        emit_ldde_immed(); emit_n((uint16_t)(0 - (int)step)); emit_nl();
                    } else {
                        emit_instrln("ld de,%d", step);
                    }
                    emit_add16(); /* HL = new value */

                    emit_store(factor_result.type); /* pop address and store HL */
                    loadval = 0;
                    indexed = 0;
                    dereference = 0;
                } else {
                    /* simple identifier */
                    emit_ld_symval(&sym);
                    if (prefix_dec) {
                        emit_ldde_immed(); emit_n((uint16_t)(0 - (int)step)); emit_nl();
                    } else {
                        emit_instrln("ld de,%d", step);
                    }
                    emit_add16();
                    emit_store_sym(&sym);
                    loadval = 0;
                }
            }
            if (tok == tokAssign) {
                parse_assign(dereference, &sym, indexed, factor_result.type);
            } else {
                /* handle postfix ++/-- */
                if (tok == tokInc || tok == tokDec) {
                    uint8_t isdec = (tok == tokDec);
                    get_token();
                    if (is_const(&sym.type)) error(errSyntax);
                    uint16_t step = 1;
                    if (indexed) {
                        /* element value increment */
                        step = 1;
                    } else {
                        if (is_ptr(&sym.type)) {
                            step = is_int(&sym.type) ? 2 : 1;
                        } else {
                            step = 1;
                        }
                    }
                    if (indexed) {
                        /* compute effective address */
                        if (is_ptr(&sym.type)) {
                            emit_ld_symval(&sym);
                        } else {
                            emit_ld_symaddr(&sym);
                        }
                        emit_pop(); /* pop index into DE */
                        emit_add16(); /* HL = address */

                        emit_push(); /* push address */
                        emit_load(factor_result.type); /* HL = original value */

                        /* save original in BC */
                        emit_copy_hl_to_bc();

                        /* compute new value */
                        if (isdec) {
                            emit_ldde_immed(); emit_n((uint16_t)(0 - (int)step)); emit_nl();
                            emit_add16();
                        } else {
                            emit_ldde_immed_n(step);
                            emit_add16();
                        }

                        emit_store(factor_result.type); /* pop address and store new value */

                        /* restore original from BC */
                        emit_copy_bc_to_hl(); 

                        loadval = 0;
                        indexed = 0;
                        dereference = 0;
                    } else {
                        /* simple identifier postfix */
                        if (loadval) {
                            if (is_func_or_proto(&sym)) {
                                emit_ld_immed(); emit_sname(sym.name); emit_nl();
                            }
                            else {
                                emit_ld_symval(&sym);
                            }
                        }

                        /* save original, compute new, store, restore original */
                        emit_push();
                        if (isdec) {
                            emit_ldde_immed(); emit_n((uint16_t)(0 - (int)step)); emit_nl();
                        } else {
                            emit_ldde_immed_n(step);
                        }
                        emit_add16();
                        emit_store_sym(&sym);
                        emit_pop();
                        emit_swap();
                        /* now HL contains original value for postfix expression */
                        loadval = 0;
                    }
                }
                
                if (loadval) {
                    if (is_func_or_proto(&sym)) {
                        emit_ld_immed(); emit_sname(sym.name); emit_nl();
                    }
                    else {
                        emit_ld_symval(&sym);
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
    if (cmpl) emit_rtl("cccom");
    return factor_result;
}

void parse_assign(uint8_t dereference, SYMBOL* sym, uint8_t indexed, TYPEREC typ) MYCC {
    get_token(); // skip '='

    if (sym && is_const(&sym->type)) {
        EXPR_RESULT r = parse_expr_delayconst(0);
        if (!is_const(&r.type)) error_expect_const();
        sym->offset = r.value;
        updatesym(sym);
        return;
    }

    /* If assigning a brace-initializer to a global symbol, emit data
     * directly for that symbol using the correct directive width.
     */
    if (tok == tokLBrace) {
        get_token(); // skip '{'
        uint16_t counter = 0;
        uint16_t elementcount = 0;
        BASE_TYPE bt = base_type(&typ);
        uint16_t skiplbl = newlbl();
        uint16_t datalbl = newlbl();
        uint16_t datalen = NO_LABEL;

        if (!sym && !dereference) error(errSyntax);
        
        if (!dereference) {            
            emit_ld_immed(); emit_lblref(datalbl); emit_nl();
            if (sym) {
                emit_store_sym(sym);
            }
            else {
                emit_store(typ);
            }
        }
        else {     
            datalen = newlbl();

            if (sym) {
                emit_ld_symval(sym);                
            }
            else {
                emit_load(typ);
            }
            if (indexed) {
                emit_pop();
                emit_add16();
            }
            
            emit_ldde_immed(); emit_lblref(datalbl); emit_nl();
            emit_swap();
            emit_ldbc_immed(); emit_lblref(datalen); emit_nl();
            emit_instrln("ldir");
        }
        
        /* Jump over the data so CPU doesn't execute it at runtime */
        emit_jp(skiplbl);
        /* Emit label + data for the global variable */
        emit_lbl(datalbl);
        emit_ch(' ');
        
        while (tok != tokRBrace && tok != tokEOS) {
            EXPR_RESULT element = parse_expr_delayconst(0);
            if (!is_const(&element.type)) error_expect_const();

            ++elementcount;
            if (counter++ > 0) emit_ch(','); else emit_instr(bt == INT ? "dw " : "db ");

            if (bt == INT) {
                emit_n(element.value);
            } else {
                emit_n(element.value & 0xff);
            }

            if (tok == tokRBrace) break;
            expect_comma();
            if (counter == 8) {
                emit_nl();
                counter = 0;
            }
        }
        if (counter) emit_nl();
        if (datalen != NO_LABEL) {
            emit_lblequ16(datalen, elementcount * (bt == INT ? 2 : 1));
        }
        expect_RBrace();

        /* place the skip label here so code continues after the data */
        emit_lbl(skiplbl);
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
