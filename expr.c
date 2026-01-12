#include "znc.h"
#include "struct.h"
#include "shared.h"

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
EXPR_RESULT far_parse_expr(uint8_t minprec) MYCC;
EXPR_RESULT far_parse_expr_delayconst(uint8_t minprec) MYCC;
void far_parse_assign(uint8_t dereference, SYMBOL* sym, uint8_t indexed, TYPEREC typ) MYCC;

/* Helper: Get element type from pointer/array type */
static TYPEREC get_element_type(const TYPEREC *ptr_or_array_type) MYCC {
    TYPEREC elemtype = { .basetype = ptr_or_array_type->basetype, .dim = 1, .struct_id = ptr_or_array_type->struct_id };
    return elemtype;
}

/* Helper: Emit scaling of value in HL or DE by given scale factor */
static void emit_scale_reg(uint16_t scale, uint8_t is_hl) MYCC {
    if (scale == 1) {
        return;
    }
    
    if (is_hl) {
        /* HL scaling */
        if (scale <= 16 && (scale == 2 || scale == 3 || scale == 4 || 
                            scale == 5 || scale == 6 || scale == 8 || 
                            scale == 10 || scale == 16)) {
            emit_mul_const_optimized(scale);
        } else if ((scale & (scale - 1)) == 0) {
            /* power of two: use repeated doubling */
            uint16_t s = scale;
            while (s > 1) { emit_mul2(); s >>= 1; }
        } else {
            /* arbitrary scale: use multiplication */
            emit_ldde_immed_n(scale);
            emit_rtl("ccmult");
        }
    } else {
        /* DE scaling */
        if (scale == 2) {
            emit_mulDE2();
        } else if ((scale & (scale - 1)) == 0) {
            /* power of two: use repeated doubling */
            uint16_t s = scale;
            while (s > 1) { emit_mulDE2(); s >>= 1; }
        } else {
            /* General case: swap, multiply HL, swap back */
            emit_copy_hl_to_bc();
            emit_swap();
            emit_mul_const_optimized(scale);
            emit_swap();
            emit_copy_bc_to_hl();
        }
    }
}

/* Wrapper for HL scaling */
static void emit_scale_hl(uint16_t scale) MYCC {
    emit_scale_reg(scale, 1);
}

/* Wrapper for DE scaling */
static void emit_scale_de(uint16_t scale) MYCC {
    emit_scale_reg(scale, 0);
}

/* Helper: Compute address of sym+offset into HL (handles global folding) */
static void emit_sym_address_with_offset(SYMBOL *sym, uint16_t offset) MYCC {
    if (is_ptr(&sym->type)) {
        emit_ld_symval(sym);
        if (offset) {
            if (offset <= 3) {
                /* Use optimized small addition (inc hl is actually faster/smaller) */
                emit_add_hl_small((int16_t)offset);
            } else {
                emit_ldde_immed_n(offset);
                emit_add16();
            }
        }
    } else {
        /* Use emit_ld_symaddr_offset which handles local variable offset folding */
        emit_ld_symaddr_offset(sym, offset);
    }
}

/* Helper: Parse and scale array index expression.
 * Returns scaled index as EXPR_RESULT (constant or in HL).
 */
static EXPR_RESULT parse_and_scale_index(const TYPEREC *elemtype) MYCC {
    get_token();
    EXPR_RESULT index_result = far_parse_expr_delayconst(0);
    expect(tokRBrack, ']');

    uint16_t scale = (uint16_t)type_size(elemtype);

    if (is_const(&index_result.type)) {
        index_result.value = (uint16_t)(index_result.value * scale);
    } else {
        emit_scale_hl(scale);
    }
    return index_result;
}

/* Helper: Compute indexed address (sym[index]) into HL.
 * Returns result with type set to element type and dereference/addr_in_hl flags updated.
 * For constant index + global sym + word size, tries to optimize as direct load.
 */
typedef struct {
    TYPEREC type;
    uint8_t dereference;
    uint8_t addr_in_hl;
} INDEXED_RESULT;

static INDEXED_RESULT compute_indexed_address(SYMBOL *sym, uint16_t base_offset, TOKEN next_tok) MYCC {
    INDEXED_RESULT result;
    TYPEREC elemtype = get_element_type(&sym->type);
    EXPR_RESULT idx = parse_and_scale_index(&elemtype);
    
    if (is_const(&idx.type)) {
        uint16_t total_offset = base_offset + idx.value;
        
        /* Optimize: global word-sized element with constant index and no assignment */
        if (sym->scope == GLOBAL && type_size(&elemtype) == 2 && next_tok != tokAssign) {
            emit_instr("ld hl,("); emit_sname(sym->name); emit_ch('+'); 
            emit_n(total_offset); emit_strln(")");
            result.type = elemtype;
            make_scalar(&result.type);
            result.dereference = 0;
            result.addr_in_hl = 0;
        } else {
            /* Compute address with folded offset */
            emit_sym_address_with_offset(sym, total_offset);
            result.type = elemtype;
            make_scalar(&result.type);
            result.dereference = 1;
            result.addr_in_hl = 1;
        }
    } else {
        /* Non-constant: push index, compute base, pop and add */
        emit_push();
        emit_sym_address_with_offset(sym, base_offset);
        emit_pop_de();
        emit_add16();
        result.type = elemtype;
        make_scalar(&result.type);
        result.dereference = 1;
        result.addr_in_hl = 1;
    }
    return result;
}

/* Helper: Handle prefix/postfix increment/decrement for lvalue at address in HL or simple sym.
 * Returns 1 if handled, 0 if no inc/dec operator present.
 */
static uint8_t handle_incdec_internal(uint8_t is_prefix, SYMBOL *sym, TYPEREC lvalue_type, uint8_t addr_in_hl, TOKEN op_override) MYCC {
    TOKEN op = (op_override != tokNone) ? op_override : tok;
    if (op != tokInc && op != tokDec) return 0;
    
    if (is_prefix) {
        /* already consumed by caller or passed as override */
    } else {
        if (op_override == tokNone) {
            get_token(); /* postfix: consume now */
        }
    }
    
    uint8_t isdec = (op == tokDec);
    
    /* Compute step size */
    uint16_t step;
    if (is_ptr(&lvalue_type) || is_array(&lvalue_type)) {
        TYPEREC elem = get_element_type(&lvalue_type);
        step = type_size(&elem);
    } else {
        step = 1;
    }
    
    if (addr_in_hl) {
        /* Address in HL: load, adjust, store */
        emit_push();
        emit_load(lvalue_type);
        
        if (!is_prefix) {
            /* Postfix: save original */
            emit_copy_hl_to_bc();
        }
        
        if (isdec) {
            emit_ldde_immed(); emit_n((uint16_t)(0 - (int)step)); emit_nl();
        } else {
            emit_instrln("ld de,%d", step);
        }
        emit_add16();
        emit_store(lvalue_type);
        
        if (!is_prefix) {
            /* Postfix: restore original */
            emit_copy_bc_to_hl();
        }
    } else {
        /* Simple symbol */
        if (!is_prefix && sym) {
            /* Postfix: load and save */
            emit_ld_symval(sym);
            emit_push();
        }
        
        if (is_prefix && sym) {
            emit_ld_symval(sym);
        }
        
        if (isdec) {
            emit_ldde_immed(); emit_n((uint16_t)(0 - (int)step)); emit_nl();
        } else {
            emit_instrln("ld de,%d", step);
        }
        emit_add16();
        
        if (sym) {
            emit_store_sym(sym);
        }
        
        if (!is_prefix) {
            /* Postfix: restore original */
            emit_pop_de();
            emit_swap();
        }
    }
    return 1;
}

/* Wrapper for normal case where operator is in tok */
static uint8_t handle_incdec(uint8_t is_prefix, SYMBOL *sym, TYPEREC lvalue_type, uint8_t addr_in_hl) MYCC {
    return handle_incdec_internal(is_prefix, sym, lvalue_type, addr_in_hl, tokNone);
}

EXPR_RESULT far_parse_expr(uint8_t minprec) MYCC {
    EXPR_RESULT expr_result = far_parse_expr_delayconst(minprec);
    
    if (is_const(&expr_result.type)) {
        emit_ld_const(expr_result.value);
    }

    return expr_result;
}

EXPR_RESULT far_parse_expr_delayconst(uint8_t minprec) MYCC {
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
    EXPR_RESULT ptyp = far_parse_expr(0);  // primary expression
    emit_jp(donelbl);
    expect(tokColon, ':' );
    emit_lbl(altlbl);
    EXPR_RESULT atyp = far_parse_expr(prec); // alternate expresion
    emit_lbl(donelbl);

    if (base_type(&ptyp.type) != base_type(&atyp.type)) error(errTypeError);
    
    expr_result.type.basetype = base_type(&ptyp.type);

    return expr_result;
}

EXPR_RESULT parse_factor(uint8_t dereference) MYCC {
    SYMBOL sym;
    EXPR_RESULT factor_result = { .type = int_type, .has_sym = 0 };
    uint8_t prefix_inc = 0;
    uint8_t prefix_dec = 0;
    uint8_t neg = 0;
    uint8_t not = 0;
    uint8_t cmpl = 0;
    uint8_t loadval = 1;
    uint8_t addr_in_hl = 0;
    uint8_t initial_deref = dereference;
    uint16_t skiplbl;
    uint16_t tmplbl;
    uint16_t counter = 0;
    uint8_t ident_base = 0;
    
    /* Handle prefix ++/-- */
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
                EXPR_RESULT element = far_parse_expr_delayconst(0);
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
            
            /* Check for prefix ++(*ptr) or --(*ptr) OR postfix (*ptr)++ or (*ptr)-- patterns */
            if (tok == tokStar) {
                get_token(); // skip '*'
                
                /* Parse the pointer expression - this will load the pointer into HL */
                EXPR_RESULT ptr_result = parse_factor(0);
                expect_RParen();
                
                /* Get element type */
                TYPEREC elem_type = get_element_type(&ptr_result.type);
                
                /* Check for postfix ++ or -- */
                if (tok == tokInc || tok == tokDec) {
                    /* This is (*ptr)++ or (*ptr)-- (postfix) */
                    /* HL already contains the pointer value (address) from parse_factor */
                    handle_incdec(0, NULL, elem_type, 1);
                    factor_result.type = elem_type;
                    break;
                } else if (prefix_inc || prefix_dec) {
                    /* This is ++(*ptr) or --(*ptr) (prefix) */
                    /* The prefix tokens were already consumed before the switch */
                    /* HL already contains the pointer value (address) */
                    TOKEN op = prefix_inc ? tokInc : tokDec;
                    handle_incdec_internal(1, NULL, elem_type, 1, op);
                    factor_result.type = elem_type;
                    /* Clear the flags so they don't get processed again */
                    prefix_inc = 0;
                    prefix_dec = 0;
                    break;
                } else {
                    /* Not an inc/dec, so just dereference normally */
                    /* HL already contains pointer from parse_factor, just load the value */
                    emit_load(elem_type);
                    factor_result.type = elem_type;
                    break;
                }
            }
            
            /* Normal parenthesized expression */
            factor_result = far_parse_expr_delayconst(0);
            expect_RParen();
            break;

        case tokIn:
            get_token(); // skip 'in'
            expect_LParen();
            far_parse_expr(0);
            expect_RParen();
            emit_copy_hl_to_bc();
            emit_instrln("in a,(c)");
            emit_rtl("ccsxt");
            factor_result.type = char_type; 
            break;

        case tokReadReg:
            get_token(); // skip 'readreg'
            expect_LParen();
            far_parse_expr(0);
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
            ARENA_MARKER _am = arena_get_marker();
            char *sbuf = NULL;
            size_t slen = 0;
            
            while (tok == tokString) {
                sbuf = arena_strappend(sbuf, slen, token, token_length);
                if (!sbuf) error(errTooManySymbols);
                slen += token_length;
                get_token();
            }

            if (!sbuf) sbuf = arena_strdup("", 0);
            intval = lookupstr(sbuf, (uint8_t)slen);
            emit_ld_immed(); emit_strref(intval); emit_nl();

            arena_free_to_marker(_am);
            break;
        }

        case tokAmp:
            get_token(); // skip '&'
            if (tok != tokIdent) error(errNotlvalue);
            sym = lookupIdent(token);
            if (not_defined(&sym)) {
                error(errNotDefined_s, token);
            }
                
            get_token(); // skip identifier
            factor_result.type = sym.type;

            if (tok == tokLBrack) {
                if (is_ptr(&factor_result.type)) {
                    emit_ld_symval(&sym);
                    TYPEREC elem_type = get_element_type(&factor_result.type);
                    emit_push();
                    parse_and_scale_index(&elem_type);
                    emit_pop_de();
                    emit_add16();
                } else if (is_array(&factor_result.type)) {
                    emit_ld_symaddr(&sym);
                    make_ptr(&factor_result.type);
                    TYPEREC elem_type = get_element_type(&factor_result.type);
                    emit_push();
                    parse_and_scale_index(&elem_type);
                    emit_pop_de();
                    emit_add16();
                } else if (sym.klass == FUNCTION) {
                    emit_ld_symaddr(&sym);
                    make_ptr(&factor_result.type);
                    TYPEREC elem_type = get_element_type(&factor_result.type);
                    emit_push();
                    parse_and_scale_index(&elem_type);
                    emit_pop_de();
                    emit_add16();
                } else {
                    error(errSyntax);
                }
            } else {
                emit_ld_symaddr(&sym);
            }            
            make_ptr(&factor_result.type);
            break;

        case tokStar:
            get_token();
            factor_result = parse_factor(1);
            if (is_const(&factor_result.type)) {
                emit_ld_const(factor_result.value);
                factor_result.type.basetype = base_type(&factor_result.type);
                make_ptr(&factor_result.type);
            }
            if (tok == tokAssign) {
                /* For *p, pass the pointer info to far_parse_assign
                 * Do NOT load the pointer value here - let far_parse_assign handle it
                 * after parsing the right side, to avoid register corruption
                 */
                TYPEREC elem_type = get_element_type(&factor_result.type);
                SYMBOL *ptr_sym = factor_result.has_sym ? &factor_result.sym : NULL;
                far_parse_assign(1, ptr_sym, 0, elem_type);
            } else {
                /* For reading (not assignment), load the value */
                if (!is_const(&factor_result.type)) {
                    if (factor_result.has_sym) {
                        emit_ld_symval(&factor_result.sym);
                    }
                }
                /* For structs, we want the address, not the dereferenced value */
                if (!is_struct(&factor_result.type)) {
                    emit_load(factor_result.type);
                }
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
            } else {
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

            factor_result.sym = sym;
            factor_result.has_sym = 1;
            ident_base = 0;

            /* Handle immediate indexing on identifier */
            if (tok == tokLBrack) {
                factor_result.has_sym = 0;
                if (!is_ptr(&sym.type) && !is_array(&sym.type)) error(errSyntax);
                
                INDEXED_RESULT ir = compute_indexed_address(&sym, 0, tok);
                factor_result.type = ir.type;
                dereference = ir.dereference;
                addr_in_hl = ir.addr_in_hl;
            }

            if (!addr_in_hl) factor_result.type = sym.type;
            if (is_const(&sym.type)) {
                factor_result.value = sym.offset;
                if (neg) factor_result.value = -factor_result.value;
                if (not) factor_result.value = !factor_result.value;
                if (cmpl) factor_result.value = ~factor_result.value;
                return factor_result;
            }
            if (dereference) make_scalar(&factor_result.type);

            /* Handle member access */
            while (tok == tokMember) {
                factor_result.has_sym = 0;
                get_token();
                
                if (!is_struct(&factor_result.type) && 
                    !(is_ptr(&factor_result.type) && (factor_result.type.basetype & 0xff) == STRUCT)) {
                    error(errSyntax);
                    return factor_result;
                }
                if (tok != tokIdent) { error(errSyntax); return factor_result; }
                
                char fname[MAX_IDENT_LEN+1];
                strncpy(fname, token, MAX_IDENT_LEN);
                int sid = (int)factor_result.type.struct_id - 1;
                int fid = find_struct_field(sid, fname);
                if (fid < 0) error(errNotDefined_s, fname);

                FIELDINFO fi = get_struct_field(sid, fid);
                uint16_t offset = fi.offset;
                get_token();

                if (tok == tokLBrack) {
                    /* Member array indexing: p.field[i] */
                    INDEXED_RESULT ir = compute_indexed_address(&sym, offset, tok);
                    factor_result.type = ir.type;
                    dereference = ir.dereference;
                    addr_in_hl = ir.addr_in_hl;
                } else {
                    /* Simple member access: p.field */
                    if (!addr_in_hl) {
                        emit_sym_address_with_offset(&sym, offset);
                    } else {
                        if (offset) {
                            emit_ldde_immed(); emit_n(offset); emit_nl();
                            emit_add16();
                        }
                    }

                    factor_result.type = fi.type;
                    if (is_array(&factor_result.type)) {
                        make_ptr(&factor_result.type);
                        dereference = 0;
                    } else {
                        dereference = 1;
                    }
                    addr_in_hl = 1;
                }
            }

            if (addr_in_hl) loadval = 0;

            if (tok == tokLParen) {
                factor_result.has_sym = 0;
                parse_funccall(&sym);
                loadval = 0;
            }

            /* Handle prefix ++/-- */
            uint8_t had_addr_in_hl = addr_in_hl;
            if ((prefix_inc || prefix_dec) && !is_const(&sym.type)) {
                handle_incdec(1, &sym, factor_result.type, had_addr_in_hl);
                loadval = 0;
            }

            if (tok == tokAssign) {
                if (!initial_deref) {
                    if (had_addr_in_hl) {
                        far_parse_assign(1, NULL, 0, factor_result.type);
                    } else {
                        far_parse_assign(dereference, &sym, 0, factor_result.type);
                    }
                }
            } else {
                /* Handle postfix ++/-- */
                if ((tok == tokInc || tok == tokDec) && !is_const(&sym.type)) {
                    handle_incdec(0, &sym, factor_result.type, had_addr_in_hl);
                    loadval = 0;
                }

                if (loadval) {
                    if (is_func_or_proto(&sym)) {
                        emit_ld_immed(); emit_sname(sym.name); emit_nl();
                    } else if (is_struct(&sym.type) && !is_ptr(&sym.type)) {
                        /* Structs are loaded as address, not value */
                        emit_ld_symaddr(&sym);
                    } else {
                        /* Don't load value here if initial_deref - let caller handle it */
                        if (!initial_deref) {
                            emit_ld_symval(&sym);
                        }
                    }
                }

                /* Only emit_load if not in initial dereference mode - caller will handle it */
                if (dereference && !initial_deref) {
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

void far_parse_assign(uint8_t dereference, SYMBOL* sym, uint8_t indexed, TYPEREC typ) MYCC {
    get_token(); // skip '='

    if (sym && is_const(&sym->type)) {
        EXPR_RESULT r = far_parse_expr_delayconst(0);
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
            /* else: HL already contains the pointer value from parse_factor (*p case) */
            if (indexed) {
                emit_pop_de();
                emit_add16();
            }
            
            /* Set up registers for ldir: HL=source, DE=dest, BC=count */
            emit_swap();  /* DE = destination (from HL) */
            emit_ld_immed(); emit_lblref(datalbl); emit_nl();  /* HL = source */
            emit_ldbc_immed(); emit_lblref(datalen); emit_nl();  /* BC = count */
            emit_instrln("ldir");
        }
        
        /* Jump over the data so CPU doesn't execute it at runtime */
        emit_jp(skiplbl);
        /* Emit label + data for the global variable */
        emit_lbl(datalbl);
        emit_ch(' ');
        
        while (tok != tokRBrace && tok != tokEOS) {
            EXPR_RESULT element = far_parse_expr_delayconst(0);
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

    /* Handle struct assignment */
    if (is_struct(&typ) && !is_ptr(&typ)) {
        EXPR_RESULT r = far_parse_expr(0);
        
        /* Verify types match */
        if (!is_struct(&r.type) || r.type.struct_id != typ.struct_id) {
            error(errTypeError);
            return;
        }
        
        /* HL now contains the source address (from far_parse_expr loading the struct address)
         * We need to set up dest address and copy */
        uint16_t struct_size = type_size(&typ);
        
        if (dereference) {
            /* Dereferenced pointer case: *p1 = p2 or *p1 = *p2 
             * HL = source address (from parsing right side)
             * Need to get destination address from pointer value
             */
            emit_push();  /* Save source address on stack */
            
            if (sym) {
                /* Load the pointer value (the address it points to) */
                emit_ld_symval(sym);
            }
            /* else: HL already has dest address from parse_factor */
            
            if (indexed) {
                emit_swap();  /* DE = dest */
                emit_pop_hl();   /* HL = index */
                emit_add16(); /* HL = dest + index */
                emit_push();  /* Save adjusted dest */
            }
            
            /* At this point we need: DE = dest, HL = source */
            emit_swap();  /* DE = dest (pointer value or adjusted address) */
            emit_pop_hl();   /* HL = source */
            emit_ldbc_immed_n(struct_size);
            emit_instrln("ldir");
        } else {
            /* Direct struct assignment: p1 = p2 */
            emit_push();  /* Save source address */
            
            if (sym) {
                emit_ld_symaddr(sym);
            } else {
                error(errNotlvalue);
                return;
            }
            
            emit_swap();  /* DE = dest */
            emit_pop_hl();   /* HL = source */
            emit_ldbc_immed_n(struct_size);
            emit_instrln("ldir");
        }
        return;
    }

    if (!sym) {
        // HL contains address to write to
        emit_push();
        far_parse_expr(0);
        emit_store(typ);
        return;
    }
    
    if (dereference) {
        emit_ld_symval(sym);
        if (indexed) {
            emit_pop_de();
            emit_add16();
        }

        emit_push();        
    }
    far_parse_expr(0);

    if (dereference) {
        emit_store(typ);
    }
    else {
        emit_store_sym(sym);
    }
}

EXPR_RESULT parse_binop(TOKEN op, EXPR_RESULT l_result, uint8_t opprec) MYCC {
    uint16_t scaleL = 0;
    uint16_t scaleR = 0;
    EXPR_RESULT r_result = { .type = void_type};
    EXPR_RESULT short_circuit_result = { .type = int_type };

    if (op == tokOr || op == tokAnd) {
        uint16_t short_circuit_lbl = newlbl();
        uint8_t short_circuit = 0;
        if (is_const(&l_result.type)) {
            emit_ld_const(l_result.value);
            short_circuit = (op == tokOr) ? (l_result.value != 0) : (l_result.value == 0);
        }
        
        if (!short_circuit) {
            (op == tokOr) ? emit_jp_true(short_circuit_lbl) : emit_jp_false(short_circuit_lbl);
        } else {
            emit_jp(short_circuit_lbl);
        }
        l_result = far_parse_expr(opprec + 1);
        emit_lbl(short_circuit_lbl);
        return short_circuit_result;
    }

    if (!is_const(&l_result.type)) {
        emit_push();
    }

    r_result = far_parse_expr_delayconst(opprec + 1);

    /* Compute pointer arithmetic scaling */
    if ((is_ptr(&l_result.type) || is_array(&l_result.type)) && base_type(&r_result.type) == INT) {
        TYPEREC elem = get_element_type(&l_result.type);
        scaleR = type_size(&elem);
    }
    if ((is_ptr(&r_result.type) || is_array(&r_result.type)) && base_type(&l_result.type) == INT) {
        TYPEREC elem = get_element_type(&r_result.type);
        scaleL = type_size(&elem);
    }

    if (scaleL && scaleR) error(errSyntax);

    uint8_t pointer = is_ptr(&l_result.type) || is_ptr(&r_result.type);

    if (pointer) {
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

    /* Constant folding */
    if (is_const(&l_result.type) && is_const(&r_result.type)) {
        switch (op) {
            case tokLt: 
                l_result.value = pointer ? (l_result.value < r_result.value) : ((int16_t)l_result.value < (int16_t)r_result.value);
                break;
            case tokLeq:
                l_result.value = pointer ? (l_result.value <= r_result.value) : ((int16_t)l_result.value <= (int16_t)r_result.value);
                break;
            case tokGt:
                l_result.value = pointer ? (l_result.value > r_result.value) : ((int16_t)l_result.value > (int16_t)r_result.value);
                break;
            case tokGeq:
                l_result.value = pointer ? (l_result.value >= r_result.value) : ((int16_t)l_result.value >= (int16_t)r_result.value);
                break;
            case tokEq: l_result.value = l_result.value == r_result.value; break;
            case tokNeq: l_result.value = l_result.value != r_result.value; break;
            case tokPlus:
                if (scaleR) l_result.value = l_result.value + (r_result.value * scaleR);
                else if (scaleL) l_result.value = (l_result.value * scaleL) + r_result.value;
                else l_result.value = l_result.value + r_result.value;
                break;
            case tokMinus:
                if (scaleR) l_result.value = l_result.value - (r_result.value * scaleR);
                else if (scaleL) l_result.value = (l_result.value * scaleL) - r_result.value;
                else l_result.value = l_result.value - r_result.value;
                break;
            case tokStar: l_result.value = l_result.value * r_result.value; break;
            case tokDiv: l_result.value = l_result.value / r_result.value; break;
            case tokMod: l_result.value = l_result.value % r_result.value; break;
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
    
    /* Load constants with pre-applied scaling */
    if (is_const(&l_result.type)) {
        uint16_t val = l_result.value;
        if (scaleL) val = val * scaleL;
        emit_ldde_immed(); emit_n(val); emit_nl();
        scaleL = 0;
        l_result.type.basetype = base_type(&l_result.type);
    } else if (is_const(&r_result.type)) {
        uint16_t val = r_result.value;
        if (scaleR) val = val * scaleR;
        emit_ld_immed(); emit_n(val); emit_nl();
        scaleR = 0;
        r_result.type.basetype = base_type(&r_result.type);
        emit_pop_de();
    } else {
        emit_pop_de();
    }

    /* Emit runtime operation with scaling */
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
            if (scaleR) emit_scale_hl(scaleR);
            if (scaleL) emit_scale_de(scaleL);
            emit_add16();
            break;
        case tokMinus:
            if (scaleR) emit_scale_hl(scaleR);
            if (scaleL) emit_scale_de(scaleL);
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
            emit_instrln("ld b,l");
            emit_instrln("bsla de,b");
            emit_swap();
            break;
        case tokShr:
            emit_instrln("ld b,l");
            emit_instrln("bsra de,b");
            emit_swap();
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
