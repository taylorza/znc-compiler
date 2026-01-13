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
void far_parse_assign(uint8_t dereference, SYMBOL sym, uint8_t indexed, uint8_t type_id) MYCC;

/* Helper: Get element type ID from pointer/array type */
static uint8_t get_element_type_id_local(uint8_t ptr_or_array_type_id) MYCC {
    return type_get_element_type_id(ptr_or_array_type_id);
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
    if (type_is_pointer(sym->type_id)) {
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

/* Helper: Parse and scale array index expression. */
static EXPR_RESULT parse_and_scale_index(uint8_t elemtype_id) MYCC {
    get_token();
    EXPR_RESULT index_result = far_parse_expr_delayconst(0);
    expect(tokRBrack, ']');

    uint16_t scale = (uint16_t)type_size(elemtype_id);

    if (type_is_const(index_result.type_id)) {
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
    uint8_t type_id;
    uint8_t dereference;
    uint8_t addr_in_hl;
} INDEXED_RESULT;

static INDEXED_RESULT compute_indexed_address(SYMBOL *sym, uint16_t base_offset, TOKEN next_tok) MYCC {
    INDEXED_RESULT result;
    uint8_t elemtype_id = get_element_type_id_local(sym->type_id);
    EXPR_RESULT idx = parse_and_scale_index(elemtype_id);
    
    if (type_is_const(idx.type_id)) {
        uint16_t total_offset = base_offset + idx.value;
        
        /* Optimize: global word-sized element with constant index and no assignment */
        if (sym->scope == GLOBAL && type_size(elemtype_id) == 2 && next_tok != tokAssign) {
            emit_instr("ld hl,("); emit_sname(sym->name); emit_ch('+'); 
            emit_n(total_offset); emit_strln(")");
            result.type_id = elemtype_id;
            result.dereference = 0;
            result.addr_in_hl = 0;
        } else {
            /* Compute address with folded offset */
            emit_sym_address_with_offset(sym, total_offset);
            result.type_id = elemtype_id;
            result.dereference = 1;
            result.addr_in_hl = 1;
        }
    } else {
        /* Non-constant: push index, compute base, pop and add */
        emit_push();
        emit_sym_address_with_offset(sym, base_offset);
        emit_pop_de();
        emit_add16();
        result.type_id = elemtype_id;
        result.dereference = 1;
        result.addr_in_hl = 1;
    }
    return result;
}

/* Helper: Handle prefix/postfix increment/decrement for lvalue at address in HL or simple sym.
 * Returns 1 if handled, 0 if no inc/dec operator present.
 */
static uint8_t handle_incdec_internal(uint8_t is_prefix, SYMBOL *sym, uint8_t lvalue_type_id, uint8_t addr_in_hl, TOKEN op_override) MYCC {
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
    if (type_is_pointer(lvalue_type_id) || type_is_array(lvalue_type_id)) {
        uint8_t elem_id = get_element_type_id_local(lvalue_type_id);
        step = type_size(elem_id);
    } else {
        step = 1;
    }
    
    if (addr_in_hl) {
        /* Address in HL: load, adjust, store */
        emit_push();
        emit_load(lvalue_type_id);
        
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
        emit_store(lvalue_type_id);
        
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
static uint8_t handle_incdec(uint8_t is_prefix, SYMBOL *sym, uint8_t lvalue_type_id, uint8_t addr_in_hl) MYCC {
    return handle_incdec_internal(is_prefix, sym, lvalue_type_id, addr_in_hl, tokNone);
}

EXPR_RESULT far_parse_expr(uint8_t minprec) MYCC {
    EXPR_RESULT expr_result = far_parse_expr_delayconst(minprec);
    
    if (type_is_const(expr_result.type_id)) {
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

    if (type_is_const(expr_result.type_id)) {
        emit_ld_const(expr_result.value);
    }
    emit_jp_false(altlbl);
    EXPR_RESULT ptyp = far_parse_expr(0);  // primary expression
    emit_jp(donelbl);
    expect(tokColon, ':' );
    emit_lbl(altlbl);
    EXPR_RESULT atyp = far_parse_expr(prec); // alternate expresion
    emit_lbl(donelbl);

    TypeKind ptyp_kind = type_get_kind(ptyp.type_id);
    TypeKind atyp_kind = type_get_kind(atyp.type_id);
    if (ptyp_kind != atyp_kind) error(errTypeError);
    
    /* Result type is the primary branch type, but mark as non-const since
     * we've already emitted the conditional code to compute the result in HL
     */
    if (ptyp_kind == TK_CHAR) {
        expr_result.type_id = TYPE_ID_CHAR;
    } else if (ptyp_kind == TK_INT) {
        expr_result.type_id = TYPE_ID_INT;
    } else {
        expr_result.type_id = ptyp.type_id;
    }

    return expr_result;
}

EXPR_RESULT parse_factor(uint8_t dereference) MYCC {
    SYMBOL sym;
    EXPR_RESULT factor_result = { .type_id = TYPE_ID_INT, .has_sym = 0 };
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
                if (!type_is_const(element.type_id)) error_expect_const();
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
            factor_result.type_id = TYPE_ID_CHAR_PTR;
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
                uint8_t elem_type_id = get_element_type_id_local(ptr_result.type_id);
                
                /* Check for postfix ++ or -- */
                if (tok == tokInc || tok == tokDec) {
                    /* This is (*ptr)++ or (*ptr)-- (postfix) */
                    /* HL already contains the pointer value (address) from parse_factor */
                    handle_incdec(0, NULL, elem_type_id, 1);
                    factor_result.type_id = elem_type_id;
                    break;
                } else if (prefix_inc || prefix_dec) {
                    /* This is ++(*ptr) or --(*ptr) (prefix) */
                    /* The prefix tokens were already consumed before the switch */
                    /* HL already contains the pointer value (address) */
                    TOKEN op = prefix_inc ? tokInc : tokDec;
                    handle_incdec_internal(1, NULL, elem_type_id, 1, op);
                    factor_result.type_id = elem_type_id;
                    /* Clear the flags so they don't get processed again */
                    prefix_inc = 0;
                    prefix_dec = 0;
                    break;
                } else {
                    /* Not an inc/dec, so just dereference normally */
                    /* HL already contains pointer from parse_factor, just load the value */
                    emit_load(elem_type_id);
                    factor_result.type_id = elem_type_id;
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
            factor_result.type_id = TYPE_ID_CHAR; 
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
            factor_result.type_id = TYPE_ID_CHAR;
            break;

        case tokString: {
            factor_result.type_id = TYPE_ID_CHAR_PTR;
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
            factor_result.type_id = sym.type_id;

            if (tok == tokLBrack) {
                if (type_is_pointer(factor_result.type_id)) {
                    emit_ld_symval(&sym);
                    emit_push();  /* Save base before parse_and_scale_index overwrites HL */
                    uint8_t elem_type_id = get_element_type_id_local(factor_result.type_id);
                    EXPR_RESULT idx = parse_and_scale_index(elem_type_id);
                    if (type_is_const(idx.type_id)) {
                        /* Optimize: add constant offset directly */
                        emit_pop_de();  /* Restore base */
                        if (idx.value != 0) {
                            emit_ldde_immed_n(idx.value);
                            emit_add16();
                        } else {
                            emit_swap();  /* Base to HL */
                        }
                    } else {
                        /* Variable index: HL has scaled index, DE has base */
                        emit_pop_de();
                        emit_add16();
                    }
                } else if (type_is_array(factor_result.type_id)) {
                    emit_ld_symaddr(&sym);
                    emit_push();  /* Save base before parse_and_scale_index overwrites HL */
                    uint8_t elem_type_id = get_element_type_id_local(factor_result.type_id);
                    factor_result.type_id = type_make_pointer(elem_type_id, 1);
                    EXPR_RESULT idx = parse_and_scale_index(elem_type_id);
                    if (type_is_const(idx.type_id)) {
                        /* Optimize: ld hl,_arr+offset */
                        emit_pop_de();  /* Discard saved base */
                        emit_ld_symaddr_offset(&sym, idx.value);
                    } else {
                        /* Variable index: HL has scaled index, DE will have base */
                        emit_pop_de();
                        emit_add16();
                    }
                } else if (sym.klass == FUNCTION) {
                    emit_ld_symaddr(&sym);
                    emit_push();  /* Save base before parse_and_scale_index overwrites HL */
                    uint8_t elem_type_id = get_element_type_id_local(factor_result.type_id);
                    factor_result.type_id = type_make_pointer(elem_type_id, 1);
                    EXPR_RESULT idx = parse_and_scale_index(elem_type_id);
                    if (type_is_const(idx.type_id)) {
                        /* Optimize: ld hl,_func+offset */
                        emit_pop_de();  /* Discard saved base */
                        emit_ld_symaddr_offset(&sym, idx.value);
                    } else {
                        /* Variable index: HL has scaled index, DE will have base */
                        emit_pop_de();
                        emit_add16();
                    }
                } else {
                    error(errSyntax);
                }
            } else {
                emit_ld_symaddr(&sym);
                uint8_t elem_type_id = get_element_type_id_local(factor_result.type_id);
                factor_result.type_id = type_make_pointer(elem_type_id, 1);
            }
            break;

        case tokStar:
            get_token();
            factor_result = parse_factor(1);
            if (type_is_const(factor_result.type_id)) {
                emit_ld_const(factor_result.value);
                uint8_t base_id = factor_result.type_id;
                factor_result.type_id = type_make_pointer(base_id, 0);
            }
            if (tok == tokAssign) {
                /* For *p, pass the pointer info to far_parse_assign
                 * Do NOT load the pointer value here - let far_parse_assign handle it
                 * after parsing the right side, to avoid register corruption
                 */
                uint8_t elem_type_id = get_element_type_id_local(factor_result.type_id);
                SYMBOL ptr_sym = factor_result.has_sym ? factor_result.sym : undefined_sym;
                far_parse_assign(1, ptr_sym, 0, elem_type_id);
            } else {
                /* For reading (not assignment), load the value */
                if (!type_is_const(factor_result.type_id)) {
                    if (factor_result.has_sym) {
                        emit_ld_symval(&factor_result.sym);
                    }
                }
                /* For structs, we want the address, not the dereferenced value */
                if (!type_is_struct(factor_result.type_id)) {
                    emit_load(factor_result.type_id);
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
                emit_load(TYPE_ID_INT);
                factor_result.type_id = TYPE_ID_INT;
            } else {
                factor_result.type_id = type_make_int(1);
                factor_result.value = intval;
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
                if (!type_is_pointer(sym.type_id) && !type_is_array(sym.type_id)) error(errSyntax);
                
                INDEXED_RESULT ir = compute_indexed_address(&sym, 0, tok);
                factor_result.type_id = ir.type_id;
                dereference = ir.dereference;
                addr_in_hl = ir.addr_in_hl;
                /* If the helper emitted a direct load into HL (addr_in_hl == 0),
                 * the value is already in HL so avoid emitting another load later.
                 */
                if (ir.addr_in_hl == 0) {
                    loadval = 0;
                }
            }

            if (!addr_in_hl) factor_result.type_id = sym.type_id;
            if (type_is_const(sym.type_id)) {
                factor_result.value = sym.offset;
                if (neg) factor_result.value = -factor_result.value;
                if (not) factor_result.value = !factor_result.value;
                if (cmpl) factor_result.value = ~factor_result.value;
                return factor_result;
            }

            /* Handle member access */
            while (tok == tokMember) {
                factor_result.has_sym = 0;
                get_token();
                
                uint8_t check_type_id = factor_result.type_id;
                if (type_is_pointer(check_type_id)) {
                    check_type_id = type_get_element_type_id(check_type_id);
                }
                if (!type_is_struct(check_type_id)) {
                    error(errSyntax);
                    return factor_result;
                }
                if (tok != tokIdent) { error(errSyntax); return factor_result; }
                
                char fname[MAX_IDENT_LEN+1];
                strncpy(fname, token, MAX_IDENT_LEN);
                int sid = (int)type_get_struct_id(check_type_id) - 1;
                int fid = find_struct_field(sid, fname);
                if (fid < 0) error(errNotDefined_s, fname);

                FIELDINFO fi = get_struct_field(sid, fid);
                uint16_t offset = fi.offset;
                get_token();

                if (tok == tokLBrack) {
                    /* Member array indexing: p.field[i] */
                    INDEXED_RESULT ir = compute_indexed_address(&sym, offset, tok);
                    factor_result.type_id = ir.type_id;
                    dereference = ir.dereference;
                    addr_in_hl = ir.addr_in_hl;
                    /* Avoid emitting a separate load if compute_indexed_address already loaded the value */
                    if (ir.addr_in_hl == 0) {
                        loadval = 0;
                    }
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

                    factor_result.type_id = fi.type_id;
                    if (type_is_array(factor_result.type_id)) {
                        uint8_t elem_id = type_get_element_type_id(factor_result.type_id);
                        factor_result.type_id = type_make_pointer(elem_id, 0);
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
                /* Update result type to be the function's return type */
                factor_result.type_id = sym.type_id;
                loadval = 0;
                
                /* Handle member access on function return value (e.g., func().member) */
                while (tok == tokMember) {
                    get_token();
                    
                    uint8_t check_type_id = factor_result.type_id;
                    if (type_is_pointer(check_type_id)) {
                        check_type_id = type_get_element_type_id(check_type_id);
                    }
                    if (!type_is_struct(check_type_id)) {
                        error(errSyntax);
                        return factor_result;
                    }
                    if (tok != tokIdent) { error(errSyntax); return factor_result; }
                    
                    char fname[MAX_IDENT_LEN+1];
                    strncpy(fname, token, MAX_IDENT_LEN);
                    int sid = (int)type_get_struct_id(check_type_id) - 1;
                    int fid = find_struct_field(sid, fname);
                    if (fid < 0) error(errNotDefined_s, fname);

                    FIELDINFO fi = get_struct_field(sid, fid);
                    uint16_t offset = fi.offset;
                    get_token();

                    /* For function return values, HL contains the pointer/struct address */
                    /* Add offset to access the member */
                    if (offset) {
                        emit_ldde_immed(); emit_n(offset); emit_nl();
                        emit_add16();
                    }
                    
                    factor_result.type_id = fi.type_id;
                    if (type_is_array(factor_result.type_id)) {
                        uint8_t elem_id = type_get_element_type_id(factor_result.type_id);
                        factor_result.type_id = type_make_pointer(elem_id, 0);
                        dereference = 0;
                    } else {
                        dereference = 1;
                    }
                    addr_in_hl = 1;
                    loadval = 0;
                }

                /* Handle array indexing on function return value (e.g., func()[index]) */
                if (tok == tokLBrack) {
                    if (!type_is_pointer(factor_result.type_id) && !type_is_array(factor_result.type_id)) {
                        error(errSyntax);
                        return factor_result;
                    }
                    
                    /* HL contains the array/pointer from function return */
                    uint8_t elemtype_id = get_element_type_id_local(factor_result.type_id);
                    
                    /* Parse and scale the index */
                    get_token(); // skip '['
                    EXPR_RESULT index_result = far_parse_expr_delayconst(0);
                    expect(tokRBrack, ']');
                    
                    uint16_t scale = (uint16_t)type_size(elemtype_id);
                    
                    if (type_is_const(index_result.type_id)) {
                        /* Constant index: add scaled offset */
                        uint16_t offset = index_result.value * scale;
                        if (offset) {
                            emit_ldde_immed_n(offset);
                            emit_add16();
                        }
                    } else {
                        /* Variable index: scale and add */
                        emit_push();  /* Save base address */
                        /* HL now has the index value */
                        if (scale > 1) {
                            emit_scale_hl(scale);
                        }
                        emit_pop_de();  /* Restore base to DE */
                        emit_add16();   /* HL = base + scaled_index */
                    }
                    
                    factor_result.type_id = elemtype_id;
                    dereference = 1;
                    addr_in_hl = 1;
                    loadval = 0;
                }
            }

            /* Handle prefix ++/-- */
            uint8_t had_addr_in_hl = addr_in_hl;
            if ((prefix_inc || prefix_dec) && !type_is_const(sym.type_id)) {
                TOKEN prefix_op = prefix_inc ? tokInc : tokDec;
                handle_incdec_internal(1, &sym, factor_result.type_id, had_addr_in_hl, prefix_op);
                loadval = 0;
            }

            if (tok == tokAssign) {
                if (!initial_deref) {
                    if (had_addr_in_hl) {
                        far_parse_assign(1, undefined_sym, 0, factor_result.type_id);
                    } else {
                        far_parse_assign(dereference, sym, 0, factor_result.type_id);
                    }
                }
            } else {
                /* Handle postfix ++/-- */
                if ((tok == tokInc || tok == tokDec) && !type_is_const(sym.type_id)) {
                    handle_incdec(0, &sym, factor_result.type_id, had_addr_in_hl);
                    loadval = 0;
                    dereference = 0;  /* Result value is already in HL, not an address */
                }

                if (loadval) {
                    if (factor_result.has_sym) {
                        if (is_func_or_proto(&factor_result.sym)) {
                            emit_ld_immed(); emit_sname(factor_result.sym.name); emit_nl();
                        } else if (type_is_struct(factor_result.sym.type_id) && !type_is_pointer(factor_result.sym.type_id)) {
                            /* Structs are loaded as address, not value */
                            emit_ld_symaddr(&factor_result.sym);
                        } else {
                            /* Don't load value here if initial_deref - let caller handle it */
                            /* Also don't load if address is already in HL from array/member access */
                            if (!initial_deref && !had_addr_in_hl) {
                                emit_ld_symval(&factor_result.sym);
                            }
                        }
                    }
                }

                /* Only emit_load if not in initial dereference mode - caller will handle it */
                if (dereference && !initial_deref) {
                    emit_load(factor_result.type_id);
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

void far_parse_assign(uint8_t dereference, SYMBOL sym, uint8_t indexed, uint8_t type_id) MYCC {
    get_token(); // skip '='

    if (sym.klass != CLASS_UNDEFINED && type_is_const(sym.type_id)) {
        EXPR_RESULT r = far_parse_expr_delayconst(0);
        if (!type_is_const(r.type_id)) error_expect_const();
        sym.offset = r.value;
        updatesym(&sym);
        return;
    }

    /* If assigning a brace-initializer to a global symbol, emit data
     * directly for that symbol using the correct directive width.
     */
    if (tok == tokLBrace) {
        get_token(); // skip '{'
        uint16_t counter = 0;
        uint16_t elementcount = 0;
        TypeKind bt = type_get_kind(type_id);
        uint16_t skiplbl = newlbl();
        uint16_t datalbl = newlbl();
        uint16_t datalen = NO_LABEL;

        if (sym.klass == CLASS_UNDEFINED && !dereference) error(errSyntax);
        
        if (!dereference) {            
            emit_ld_immed(); emit_lblref(datalbl); emit_nl();
            if (sym.klass != CLASS_UNDEFINED) {
                emit_store_sym(&sym);
            }
            else {
                emit_store(type_id);
            }
        }
        else {     
            datalen = newlbl();

            if (sym.klass != CLASS_UNDEFINED) {
                emit_ld_symval(&sym);                
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
            if (!type_is_const(element.type_id)) error_expect_const();

            ++elementcount;
            if (counter++ > 0) emit_ch(','); else emit_instr(bt == TK_INT ? "dw " : "db ");

            if (bt == TK_INT) {
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
            emit_lblequ16(datalen, elementcount * (bt == TK_INT ? 2 : 1));
        }
        expect_RBrace();

        /* place the skip label here so code continues after the data */
        emit_lbl(skiplbl);
        return;
    }

    /* Handle struct assignment */
    if (type_is_struct(type_id) && !type_is_pointer(type_id)) {
        EXPR_RESULT r = far_parse_expr(0);
        
        /* Verify types match */
        if (!type_is_struct(r.type_id) || type_get_struct_id(r.type_id) != type_get_struct_id(type_id)) {
            error(errTypeError);
            return;
        }
        
        /* HL now contains the source address (from far_parse_expr loading the struct address)
         * We need to set up dest address and copy */
        uint16_t struct_size = type_size(type_id);
        
        if (dereference) {
            /* Dereferenced pointer case: *p1 = p2 or *p1 = *p2 
             * HL = source address (from parsing right side)
             * Need to get destination address from pointer value
             */
            emit_push();  /* Save source address on stack */
            
            if (sym.klass != CLASS_UNDEFINED) {
                /* Load the pointer value (the address it points to) */
                emit_ld_symval(&sym);
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
            
            if (sym.klass != CLASS_UNDEFINED) {
                emit_ld_symaddr(&sym);
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

    if (sym.klass == CLASS_UNDEFINED) {
        // HL contains address to write to
        emit_push();
        far_parse_expr(0);
        emit_store(type_id);
        return;
    }
    
    if (dereference) {
        emit_ld_symval(&sym);
        if (indexed) {
            emit_pop_de();
            emit_add16();
        }

        emit_push();        
    }
    far_parse_expr(0);

    if (dereference) {
        emit_store(type_id);
    }
    else {
        emit_store_sym(&sym);
    }
}

EXPR_RESULT parse_binop(TOKEN op, EXPR_RESULT l_result, uint8_t opprec) MYCC {
    uint16_t scaleL = 0;
    uint16_t scaleR = 0;
    EXPR_RESULT r_result = { .type_id = TYPE_ID_VOID };
    EXPR_RESULT short_circuit_result = { .type_id = TYPE_ID_INT };

    if (op == tokOr || op == tokAnd) {
        uint16_t short_circuit_lbl = newlbl();
        uint8_t short_circuit = 0;
        if (type_is_const(l_result.type_id)) {
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

    if (!type_is_const(l_result.type_id)) {
        emit_push();
    }

    r_result = far_parse_expr_delayconst(opprec + 1);

    /* Compute pointer arithmetic scaling */
    if ((type_is_pointer(l_result.type_id) || type_is_array(l_result.type_id)) && type_get_kind(r_result.type_id) == TK_INT) {
        uint8_t elem_id = get_element_type_id_local(l_result.type_id);
        scaleR = type_size(elem_id);
    }
    if ((type_is_pointer(r_result.type_id) || type_is_array(r_result.type_id)) && type_get_kind(l_result.type_id) == TK_INT) {
        uint8_t elem_id = get_element_type_id_local(r_result.type_id);
        scaleL = type_size(elem_id);
    }

    if (scaleL && scaleR) error(errSyntax);

    uint8_t pointer = type_is_pointer(l_result.type_id) || type_is_pointer(r_result.type_id);

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
    if (type_is_const(l_result.type_id) && type_is_const(r_result.type_id)) {
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
    if (type_is_const(l_result.type_id)) {
        uint16_t val = l_result.value;
        if (scaleL) val = val * scaleL;
        emit_ldde_immed(); emit_n(val); emit_nl();
        scaleL = 0;
    } else if (type_is_const(r_result.type_id)) {
        uint16_t val = r_result.value;
        if (scaleR) val = val * scaleR;
        emit_ld_immed(); emit_n(val); emit_nl();
        scaleR = 0;
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
    
    /* After emitting runtime code, result is no longer const */
    if (type_is_const(l_result.type_id)) {
        l_result.type_id = TYPE_ID_INT;
    }
    
    return l_result;
}
