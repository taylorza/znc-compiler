#include "znc.h"
#include "struct.h"
#include "shared.h"

#define ASSIGN_PREC 1
#define COND_PREC 3
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

/* Register type for scaling operations */
typedef enum {
    SCALE_DE = 0,
    SCALE_HL = 1
} SCALE_REG;

extern EXPR_RESULT parse_onearg(void) MYCC;
extern void parse_type(uint8_t* type_id_out) MYCC;

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

EXPR_RESULT parse_ternary(EXPR_RESULT expr_result, uint8_t prec, uint8_t expected_type_id) MYCC;
EXPR_RESULT parse_factor(uint8_t dereference, uint8_t expected_type_id) MYCC;
EXPR_RESULT far_parse_expr(uint8_t minprec, uint8_t expected_type_id) MYCC;
EXPR_RESULT far_parse_expr_delayconst(uint8_t minprec, uint8_t expected_type_id) MYCC;
void far_parse_assign(uint8_t dereference, SYMBOL sym, uint8_t indexed, uint8_t type_id) MYCC;


/* Helper: Emit scaling of value in HL or DE by given scale factor */
static void emit_scale_reg(uint16_t scale, SCALE_REG reg) MYCC {
    if (scale == 1) {
        return;
    }
    
    if (reg == SCALE_HL) {
        emit_mul_const_optimized(scale);        
    } else {
        emit_swap();
        emit_mul_const_optimized(scale);
        emit_swap();
    }
}

/* Helper: Parse one or more adjacent string tokens ("a" "b") and return string id */
static uint16_t parse_concat_string_literal(void) MYCC {
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
    uint16_t sid = lookupstr(sbuf, (uint8_t)slen);
    arena_free_to_marker(_am);
    intval = sid; /* keep global consistent with previous behavior */
    return sid;
}

/* Helper: Parse and emit elements for brace-initializer expressions.
 * Returns the number of elements processed.
 * element_type_id: expected type for each element (0 = infer from data)
 * is_char: true if emitting byte data, false for word data
 */
static uint16_t parse_brace_initializer_elements(uint8_t element_type_id) MYCC {
    uint16_t counter = 0;
    uint16_t elementcount = 0;
    
    uint8_t delegate_sig_id = 0xFF;
    uint8_t is_delegate = type_is_delegate(element_type_id);
    if (is_delegate) {
        delegate_sig_id = type_get_function_sig(element_type_id);
    }

    uint8_t is_char = type_is_char(element_type_id);
    while (tok != tokRBrace && tok != tokEOS) {
        /* Special-case string literals: emit DW str+offset directly without loading HL */
        if (tok == tokString && (element_type_id == 0 || type_is_pointer(element_type_id))) {
            /* Only valid when element type expects a pointer (e.g., char*) or is inferred */
            uint16_t sid = parse_concat_string_literal();
            ++elementcount;
            if (counter++ > 0)
                emit_ch(',');
            else
                emit_instr(is_char ? "db " : "dw ");

            /* Emit string label reference (e.g., str+nn) */
            emit_strref(sid);
        } else {
            EXPR_RESULT element = far_parse_expr_delayconst(0, element_type_id);
            uint8_t is_func = element.has_sym && is_func_or_proto(&element.sym);
            if (!type_is_const(element.type_id) && !is_func) error_expect_const();

            if (is_delegate) {
                if (!is_func) error(errTypeError);
                if (!signature_check(delegate_sig_id, element.sym.signature_id)) {
                    error(errTypeError);
                }
            }
            else if (!type_check_compatible(element.type_id, element_type_id) && element_type_id != 0) {
                error(errTypeError);
            }
            
            ++elementcount;
            if (counter++ > 0) 
                emit_ch(','); 
            else 
                emit_instr(is_char ? "db " : "dw ");

            if (is_func) {
                emit_sname(element.sym.name);
            }
            else if (is_char) {
                emit_n(element.value & 0xff);
            }
            else {
                emit_n(element.value);
            }
        }
        
        if (tok == tokRBrace) break;
        expect_comma();
        if (counter == 8) {
            emit_nl();
            counter = 0;
        }
    }
    if (counter) emit_nl();
    
    return elementcount;
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
        /* Both local and global arrays/structs have members at HIGHER addresses
         * The base address points to the start, members grow upward in memory
         * Simply pass the offset through to emit_ld_symaddr_offset */
        emit_ld_symaddr_offset(sym, offset);
    }
}

/* Helper: Parse and scale array index expression. */
static EXPR_RESULT parse_and_scale_index(uint8_t elemtype_id) MYCC {
    get_token();
    EXPR_RESULT index_result = far_parse_expr_delayconst(0, TYPE_ID_INT);
    expect(tokRBrack, ']');

    uint16_t scale = (uint16_t)type_size(elemtype_id);

    if (type_is_const(index_result.type_id)) {
        index_result.value = (uint16_t)(index_result.value * scale);
    } else {
        emit_scale_reg(scale, SCALE_HL);
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
    uint8_t elemtype_id = type_get_element_type_id(sym->type_id);
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
        uint8_t elem_id = type_get_element_type_id(lvalue_type_id);
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
        
        /* Optimize: use repeated inc/dec hl for small steps */
        if (step <= 3) {
            for (uint16_t i = 0; i < step; i++) {
                emit_instrln(isdec ? "dec hl" : "inc hl");
            }
        } else {
            if (isdec) {
                emit_ldde_immed(); emit_n((uint16_t)(0 - (int)step)); emit_nl();
            } else {
                emit_instrln("ld de,%d", step);
            }
            emit_add16();
        }
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
        
        /* Optimize: use repeated inc/dec hl for small steps */
        if (step <= 3) {
            for (uint16_t i = 0; i < step; i++) {
                emit_instrln(isdec ? "dec hl" : "inc hl");
            }
        } else {
            if (isdec) {
                emit_ldde_immed(); emit_n((uint16_t)(0 - (int)step)); emit_nl();
            } else {
                emit_instrln("ld de,%d", step);
            }
            emit_add16();
        }
        
        if (sym) {
            emit_store_sym(sym);
        }
        
        if (!is_prefix) {
            /* Postfix: restore original value to HL for expression result */
            emit_pop_hl();
        }
    }
    return 1;
}

/* Wrapper for normal case where operator is in tok */
static uint8_t handle_incdec(uint8_t is_prefix, SYMBOL *sym, uint8_t lvalue_type_id, uint8_t addr_in_hl) MYCC {
    return handle_incdec_internal(is_prefix, sym, lvalue_type_id, addr_in_hl, tokNone);
}

/* Helper: Lookup struct field and return field info. Returns 0 on error, 1 on success. */
static uint8_t lookup_struct_member(uint8_t check_type_id, FIELDINFO *fi_out, uint16_t *offset_out) MYCC {
    if (!type_is_struct(check_type_id)) {
        error(errSyntax);
        return 0;
    }
    if (tok != tokIdent) {
        error(errSyntax);
        return 0;
    }
    
    char fname[MAX_IDENT_LEN+1];
    strncpy(fname, token, MAX_IDENT_LEN);
    int sid = (int)type_get_struct_id(check_type_id) - 1;
    int fid = find_struct_field(sid, fname);
    if (fid < 0) {
        error(errNotDefined_s, fname);
        return 0;
    }
    
    *fi_out = get_struct_field(sid, fid);
    *offset_out = fi_out->offset;
    get_token();
    return 1;
}

EXPR_RESULT parse_op_right(EXPR_RESULT left, uint8_t minprec, uint8_t expected_type_id) {
    uint8_t p;
    while ((p = prec(tok)) && p && p >= minprec) {
        TOKEN op = tok;
        get_token(); // skip op

        if (op == tokCond) {
            left = parse_ternary(left, p, expected_type_id);
        }
        else {
            /* Inline binary operator handling */
            uint16_t scaleL = 0;
            uint16_t scaleR = 0;
            EXPR_RESULT r_result = { .type_id = TYPE_ID_VOID };

            if (op == tokOr || op == tokAnd) {
                uint16_t short_circuit_lbl = newlbl();
                uint8_t short_circuit = 0;
                if (type_is_const(left.type_id)) {
                    emit_ld_const(left.value);
                    short_circuit = (op == tokOr) ? (left.value != 0) : (left.value == 0);
                }

                if (!short_circuit) {
                    (op == tokOr) ? emit_jp_true(short_circuit_lbl) : emit_jp_false(short_circuit_lbl);
                }
                else {
                    emit_jp(short_circuit_lbl);
                }
                left = far_parse_expr(p + 1, 0);
                
                emit_lbl(short_circuit_lbl);
                left.type_id = TYPE_ID_INT;
                continue;
            }

            if (!type_is_const(left.type_id)) {
                emit_push();
            }

            r_result = far_parse_expr_delayconst(p + 1, 0);

            /* Compute pointer arithmetic scaling */
            if ((type_is_pointer(left.type_id) || type_is_array(left.type_id)) && type_get_kind(r_result.type_id) == TK_INT) {
                uint8_t elem_id = type_get_element_type_id(left.type_id);
                scaleR = type_size(elem_id);
            }
            if ((type_is_pointer(r_result.type_id) || type_is_array(r_result.type_id)) && type_get_kind(left.type_id) == TK_INT) {
                uint8_t elem_id = type_get_element_type_id(r_result.type_id);
                scaleL = type_size(elem_id);
            }

            if (scaleL && scaleR) error(errSyntax);

            uint8_t pointer = type_is_pointer(left.type_id) || type_is_pointer(r_result.type_id);

            if (pointer) {
                switch (op) {
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
            if (type_is_const(left.type_id) && type_is_const(r_result.type_id)) {
                switch (op) {
                    case tokLt:
                        left.value = pointer ? (left.value < r_result.value) : ((int16_t)left.value < (int16_t)r_result.value);
                        break;
                    case tokLeq:
                        left.value = pointer ? (left.value <= r_result.value) : ((int16_t)left.value <= (int16_t)r_result.value);
                        break;
                    case tokGt:
                        left.value = pointer ? (left.value > r_result.value) : ((int16_t)left.value > (int16_t)r_result.value);
                        break;
                    case tokGeq:
                        left.value = pointer ? (left.value >= r_result.value) : ((int16_t)left.value >= (int16_t)r_result.value);
                        break;
                    case tokEq: left.value = left.value == r_result.value; break;
                    case tokNeq: left.value = left.value != r_result.value; break;
                    case tokPlus:
                        if (scaleR) left.value = left.value + (r_result.value * scaleR);
                        else if (scaleL) left.value = (left.value * scaleL) + r_result.value;
                        else left.value = left.value + r_result.value;
                        break;
                    case tokMinus:
                        if (scaleR) left.value = left.value - (r_result.value * scaleR);
                        else if (scaleL) left.value = (left.value * scaleL) - r_result.value;
                        else left.value = left.value - r_result.value;
                        break;
                    case tokStar: left.value = left.value * r_result.value; break;
                    case tokDiv: left.value = left.value / r_result.value; break;
                    case tokMod: left.value = left.value % r_result.value; break;
                    case tokShl: left.value = left.value << r_result.value; break;
                    case tokShr: left.value = left.value >> r_result.value; break;
                    case tokBitOr: left.value = left.value | r_result.value; break;
                    case tokBitAnd: left.value = left.value & r_result.value; break;
                    case tokBitXor: left.value = left.value ^ r_result.value; break;
                    default:
                        error(errSyntax);
                        break;
                }
                continue;
            }

            /* Load constants with pre-applied scaling */
            if (type_is_const(left.type_id)) {
                uint16_t val = left.value;
                if (scaleL) val = val * scaleL;
                emit_ldde_immed(); emit_n(val); emit_nl();
                scaleL = 0;
            }
            else if (type_is_const(r_result.type_id)) {
                uint16_t val = r_result.value;
                if (scaleR) val = val * scaleR;
                emit_ld_immed(); emit_n(val); emit_nl();
                scaleR = 0;
                emit_pop_de();
            }
            else {
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
                    if (scaleR) emit_scale_reg(scaleR, SCALE_HL);
                    if (scaleL) emit_scale_reg(scaleL, SCALE_DE);
                    emit_add16();
                    break;
                case tokMinus:
                    if (scaleR) emit_scale_reg(scaleR, SCALE_HL);
                    if (scaleL) emit_scale_reg(scaleL, SCALE_DE);
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
            
            /* After emitting runtime code, result is no longer const or a simple symbol */
            if (type_is_const(left.type_id)) {
                left.type_id = TYPE_ID_INT;
            }
            left.has_sym = 0;  /* Result is computed, not a direct symbol reference */
        }
    }
    return left;
}

EXPR_RESULT far_parse_expr(uint8_t minprec, uint8_t expected_type_id) MYCC {
    EXPR_RESULT expr_result = parse_factor(0, expected_type_id);
    expr_result = parse_op_right(expr_result, minprec, expected_type_id);
    
    if (type_is_const(expr_result.type_id)) {
        emit_ld_const(expr_result.value);
    } else if (expr_result.has_sym && is_func_or_proto(&expr_result.sym)) {
        /* Function address: emit symbol load */
        emit_ld_immed(); emit_sname(expr_result.sym.name); emit_nl();
    }

    return expr_result;
}

EXPR_RESULT far_parse_expr_delayconst(uint8_t minprec, uint8_t expected_type_id) MYCC {
    EXPR_RESULT expr_result = parse_factor(0, expected_type_id);
    expr_result = parse_op_right(expr_result, minprec, expected_type_id);
    return expr_result;
}

EXPR_RESULT parse_ternary(EXPR_RESULT expr_result, uint8_t prec, uint8_t expected_type_id) MYCC {
    uint16_t altlbl = newlbl();
    uint16_t donelbl = newlbl();

    if (type_is_const(expr_result.type_id)) {
        emit_ld_const(expr_result.value);
    }
    emit_jp_false(altlbl);
    EXPR_RESULT ptyp = far_parse_expr(0, expected_type_id);  // primary expression
    emit_jp(donelbl);
    expect(tokColon, ':' );
    emit_lbl(altlbl);
    EXPR_RESULT atyp = far_parse_expr(prec, expected_type_id); // alternate expresion
    
    /* Type checking: if expected_type_id is given, both branches must be compatible with it.
     * If expected_type_id is 0 (unknown), then the two branches must be compatible with each other. */
    if (expected_type_id != 0) {
        if (!type_check_compatible(ptyp.type_id, expected_type_id)) error(errTypeError);
        if (!type_check_compatible(atyp.type_id, expected_type_id)) error(errTypeError);        
    } else {
        /* No expected type: ensure both branches are compatible with each other */
        if (!type_check_compatible(ptyp.type_id, atyp.type_id) && !type_check_compatible(atyp.type_id, ptyp.type_id)) {
            error(errTypeError);
        }        
    }
  
    emit_lbl(donelbl);

    expr_result.has_sym = 0;
    expr_result.type_id = expected_type_id;
    return expr_result;
}

EXPR_RESULT parse_factor(uint8_t dereference, uint8_t expected_type_id) MYCC {
    SYMBOL sym;
    EXPR_RESULT factor_result = { .type_id = expected_type_id, .has_sym = 0 };
    uint8_t prefix_inc = 0;
    uint8_t prefix_dec = 0;
    uint8_t neg = 0;
    uint8_t not = 0;
    uint8_t cmpl = 0;
    uint8_t addr_in_hl = 0;
    uint8_t initial_deref = dereference;
    uint16_t skiplbl;
    uint16_t tmplbl;
    uint16_t counter = 0;
    uint8_t element_type_id;
    
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
            if (!type_is_array(expected_type_id) 
             && !type_is_pointer(expected_type_id)) error(errTypeError);

            element_type_id = type_get_element_type_id(expected_type_id);
            get_token(); // skip '{'
            skiplbl = newlbl();
            emit_jp(skiplbl);
            tmplbl = newlbl();
            emit_lbl(tmplbl);
                        
            parse_brace_initializer_elements(expected_type_id);
            
            expect_RBrace();
            emit_lbl(skiplbl);
            emit_ld_immed(); emit_lblref(tmplbl); emit_nl();
            factor_result.type_id = expected_type_id;
            break;
        case tokLParen:
            get_token(); // skip '('
            
            /* Check for prefix ++(*ptr) or --(*ptr) patterns only if we have prefix operators */
            if (tok == tokStar && (prefix_inc || prefix_dec)) {
                get_token(); // skip '*'
                
                /* Parse the pointer expression - this will load the pointer into HL */
                EXPR_RESULT ptr_result = parse_factor(0, 0);
                expect_RParen();
                
                /* Get element type */
                uint8_t elem_type_id = type_get_element_type_id(ptr_result.type_id);
                
                /* This is ++(*ptr) or --(*ptr) (prefix) */
                TOKEN op = prefix_inc ? tokInc : tokDec;
                handle_incdec_internal(1, NULL, elem_type_id, 1, op);
                factor_result.type_id = elem_type_id;
                /* Clear the flags so they don't get processed again */
                prefix_inc = 0;
                prefix_dec = 0;
                break;
            }
            
            /* Check for (*...) dereference pattern that might be followed by postfix ++ or -- */
            if (tok == tokStar) {
                get_token(); // skip '*'
                
                /* Parse the pointer expression */
                EXPR_RESULT ptr_result = parse_factor(0, 0);
                if (tok == tokRParen) {
                    get_token(); // skip ')'

                    /* Get element type */
                    uint8_t elem_type_id = type_get_element_type_id(ptr_result.type_id);
                    factor_result.type_id = elem_type_id;

                    /* Don't load the value yet - leave address in HL for potential postfix operator */
                    /* The postfix loop will handle ++ or --, or the normal path will load if needed */
                    addr_in_hl = 1;
                    dereference = 1;                    
                }
                else {
                    emit_load(ptr_result.type_id);
                    factor_result = parse_op_right(ptr_result, 0, expected_type_id);
                    expect_RParen();                    
                }
                break;
            }
            
            /* Normal parenthesized expression */
            factor_result = far_parse_expr_delayconst(0, 0);
            expect_RParen();
            
            break;

        case tokPuts:
            parse_onearg(); // (expr)
            emit_rtl("puts");
            factor_result.type_id = TYPE_ID_INT; 
            break;

        case tokIn:            
            parse_onearg(); // (expr)
            emit_copy_hl_to_bc();
            emit_instrln("in a,(c)");
            emit_rtl("ccsxt");
            factor_result.type_id = TYPE_ID_CHAR; 
            break;

        case tokReadReg:
            get_token(); // skip 'readreg'
            expect_LParen();
            far_parse_expr(0, 0);
            emit_instrln("ld bc,9275");
            emit_instrln("out (c),l");
            emit_instrln("inc b");
            expect_RParen();            
            emit_instrln("in a,(c)");
            emit_rtl("ccsxt");
            factor_result.type_id = TYPE_ID_CHAR;
            break;

        case tokVaArg:
            /* va_arg(valist, type) - returns next variadic arg */
            get_token(); // skip 'va_arg'
            expect_LParen();
            {
                SYMBOL valist_sym = lookupIdent(token);
                if (not_defined(&valist_sym)) {
                    error(errNotDefined_s, token);
                }
                get_token(); // skip valist identifier
                emit_ld_symaddr(&valist_sym);
                emit_rtl("ccvaarg");                
            }
            expect_comma();
            parse_type(&factor_result.type_id);
            expect_RParen();            
            break;

        case tokString: {
            factor_result.type_id = TYPE_ID_CHAR_PTR;
            uint16_t sid = parse_concat_string_literal();
            emit_ld_immed(); emit_strref(sid); emit_nl();            
            break;
        }

        case tokAmp:
            /* Address-of operator: &expr
             * Manually parse identifier and postfix operators ([], .), computing address in HL.
             * Avoid dereference to keep the address itself.
             */
            get_token(); // skip '&'
            if (tok != tokIdent) error(errNotlvalue);
            sym = lookupIdent(token);
            if (not_defined(&sym)) {
                error(errNotDefined_s, token);
            }
            
            get_token(); // skip identifier
            factor_result.type_id = sym.type_id;
            factor_result.has_sym = 1;
            factor_result.sym = sym;
            
            /* Manually handle postfix [], and . operators for address-of.
             * Build address in HL without final dereference.
             */
            uint8_t addr_base_sym = 1;  /* Track if base is still a symbol */
            
            while (tok == tokLBrack || tok == tokMember) {
                if (tok == tokLBrack) {
                    /* Array/pointer indexing: &name[i] or &name.arr[i] */
                    uint8_t elemtype_id = type_get_element_type_id(factor_result.type_id);
                    uint16_t scale = (uint16_t)type_size(elemtype_id);
                    
                    get_token(); // skip '['
                    EXPR_RESULT index_result = far_parse_expr_delayconst(0, TYPE_ID_INT);
                    expect(tokRBrack, ']');
                    
                    if (addr_base_sym) {
                        /* Base is still a symbol - compute address directly */
                        if (type_is_const(index_result.type_id)) {
                            /* Constant index: emit ld hl,sym+offset directly */
                            uint16_t total_offset = index_result.value * scale;
                            emit_ld_symaddr_offset(&sym, total_offset);
                        } else {
                            /* Variable index: scale it, then add to symbol address */
                            if (scale > 1) {
                                emit_scale_reg(scale, 1);
                            }
                            emit_swap();  /* DE = scaled index */
                            emit_ld_symaddr(&sym);  /* HL = base address */
                            emit_add16();  /* HL = base + scaled_index */
                        }
                        addr_base_sym = 0;
                    } else {
                        /* Base address already in HL from previous operation, save it */
                        emit_push();
                        if (type_is_const(index_result.type_id)) {
                            uint16_t offset = index_result.value * scale;
                            if (offset) {
                                emit_pop_de();  /* Restore base to DE */
                                emit_ldde_immed_n(offset);
                                emit_add16();
                            } else {
                                emit_pop_hl();  /* No offset, just restore */
                            }
                        } else {
                            /* Variable index: HL has index, need to scale and add */
                            if (scale > 1) {
                                emit_scale_reg(scale, SCALE_HL);
                            }
                            emit_pop_de();  /* DE = base */
                            emit_add16();   /* HL = base + scaled_index */
                        }
                    }
                    
                    factor_result.type_id = elemtype_id;
                } 
                else if (tok == tokMember) {
                    /* Struct member access: &name.member or &name[i].member */
                    get_token();
                    
                    uint8_t check_type_id = factor_result.type_id;
                    if (type_is_pointer(check_type_id)) {
                        check_type_id = type_get_element_type_id(check_type_id);
                    }
                    if (!type_is_struct(check_type_id)) {
                        error(errSyntax);
                        break;
                    }
                    
                    FIELDINFO fi;
                    uint16_t offset;
                    if (!lookup_struct_member(check_type_id, &fi, &offset)) {
                        break;
                    }
                    
                    /* Add offset to base address */
                    if (addr_base_sym) {
                        emit_sym_address_with_offset(&sym, offset);
                        addr_base_sym = 0;
                    } else {
                        /* Address already in HL, add offset */
                        if (offset) {
                            emit_ldde_immed_n(offset);
                            emit_add16();
                        }
                    }
                    
                    factor_result.type_id = fi.type_id;
                }
            }
            
            /* Ensure address is in HL */
            if (addr_base_sym) {
                emit_ld_symaddr(&sym);
            }
            
            /* Result: address in HL, type is pointer to the element */
            factor_result.type_id = type_make_pointer(factor_result.type_id, 1);
            factor_result.has_sym = 0;
            dereference = 0;
            addr_in_hl = 1;
            break;

        case tokStar:
            get_token();
            factor_result = parse_factor(1, expected_type_id);
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
                uint8_t elem_type_id = type_get_element_type_id(factor_result.type_id);
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
                /* After dereferencing, result is no longer the original symbol */
                factor_result.has_sym = 0;
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
            factor_result.type_id = sym.type_id;
            
            if (type_is_const(sym.type_id)) {
                factor_result.value = sym.offset;
                if (neg) factor_result.value = -factor_result.value;
                if (not) factor_result.value = !factor_result.value;
                if (cmpl) factor_result.value = ~factor_result.value;
                return factor_result;
            }

            /* For function symbols used as addresses (not called), return early like constants
             * to avoid emitting load instructions. The caller can check has_sym and is_func_or_proto
             * to emit the symbol reference directly (e.g., for initializers).
             */
            if (is_func_or_proto(&sym) && tok != tokLParen && tok != tokLBrack) {
                factor_result.value = 0;                  /* Functions don't have a numeric value */
                return factor_result;
            }
            
            /* Don't handle postfix operators here - let the postfix loop handle them */
            break;
        default:
            error(errSyntax);
            break;
    }
    
    /* Postfix operators loop - handles [], (), ., ++, -- */
    while (tok == tokLBrack || tok == tokLParen || tok == tokMember || tok == tokInc || tok == tokDec) {
        
        /* Handle array indexing: expr[index] */
        if (tok == tokLBrack) {
            SYMBOL base_sym = factor_result.has_sym ? factor_result.sym : undefined_sym;
            uint8_t had_sym = factor_result.has_sym;
            
            /* Need address or pointer/array type */
            if (addr_in_hl) {
                /* HL has address, need to load it first if it's a pointer */
                if (dereference && type_is_pointer(factor_result.type_id)) {
                    emit_load(factor_result.type_id);
                    dereference = 0;
                }
            } else if (factor_result.has_sym) {
                /* Symbol - check if it's array or pointer */
                if (!type_is_pointer(factor_result.sym.type_id) && !type_is_array(factor_result.sym.type_id)) {
                    error(errSyntax);
                    break;
                }
                /* For arrays with constant index, we can optimize later, so don't load yet */
                /* For pointers, we need to load the pointer value */
                if (type_is_pointer(factor_result.sym.type_id)) {
                    emit_ld_symval(&factor_result.sym);
                    had_sym = 0;  /* No longer a direct symbol reference */
                }
            } else {
                /* Result already in HL from previous postfix operation */
                if (!type_is_pointer(factor_result.type_id) && !type_is_array(factor_result.type_id)) {
                    error(errSyntax);
                    break;
                }
            }
            
            /* Clear has_sym after using it */
            factor_result.has_sym = 0;
            
            /* Now HL contains the base address/pointer (or we have a direct function symbol) */
            uint8_t elemtype_id = type_get_element_type_id(factor_result.type_id);
            
            /* Parse and scale the index */
            get_token(); // skip '['
            
            /* If base is already in HL (no symbol), save it before parsing index */
            if (!had_sym) {
                emit_push();
            }
            
            EXPR_RESULT index_result = far_parse_expr_delayconst(0, TYPE_ID_INT);
            expect(tokRBrack, ']');
            
            uint16_t scale = (uint16_t)type_size(elemtype_id);
            
            if (type_is_const(index_result.type_id)) {
                /* Constant index: compute offset */
                uint16_t offset = index_result.value * scale;
                
                /* Optimize: For array symbols with constant index, use direct addressing */
                if (had_sym && type_is_array(base_sym.type_id)) {
                    if (offset) {
                        emit_ld_symaddr_offset(&base_sym, offset);
                    } else {
                        emit_ld_symaddr(&base_sym);
                    }
                } else {
                    /* Already loaded - add offset */
                    if (had_sym) {
                        /* Discard saved base for constant case with pointer symbol */
                        /* (pointer was loaded before push) */
                    } else {
                        emit_pop_hl();  /* Restore base that was saved */
                    }
                    if (offset) {
                        emit_ldde_immed_n(offset);
                        emit_add16();  /* HL = base + offset */
                    }
                }
            } else {
                /* Variable index: HL has index, scale it first */
                if (scale > 1) {
                    emit_scale_reg(scale, 1);
                }
                
                /* Now get base address */
                if (had_sym && type_is_array(base_sym.type_id)) {
                    emit_swap();  /* DE = scaled index */
                    emit_ld_symaddr(&base_sym);  /* HL = base */
                } else {
                    /* Base was saved on stack before parsing index */
                    emit_pop_de();  /* DE = base */
                }
                emit_add16();   /* HL = base + scaled_index */
            }
            
            factor_result.type_id = elemtype_id;
            dereference = 1;
            addr_in_hl = 1;
            continue;
        }
        
        /* Handle function calls: expr(args) */
        if (tok == tokLParen) {
            /* Save symbol info before clearing has_sym */
            SYMBOL call_sym = factor_result.has_sym ? factor_result.sym : undefined_sym;
            uint8_t had_sym = factor_result.has_sym;
            
            factor_result.has_sym = 0;
            
            /* If we have an address that needs dereferencing, load it first */
            if (addr_in_hl && dereference) {
                emit_load(factor_result.type_id);
                addr_in_hl = 0;
                dereference = 0;
            }
            
            /* Determine if pointer is already in HL or if we have a direct function symbol */
            uint8_t ptr_in_hl = !had_sym;  /* If no symbol, value is in HL */
            
            parse_funccall(&call_sym, ptr_in_hl);
            
            /* Function return value is in HL - get the return type from signature */
            if (had_sym && is_func_or_proto(&call_sym) && call_sym.signature_id != 0xFF) {
                factor_result.type_id = signature_get_return_type(call_sym.signature_id);
            } else {
                factor_result.type_id = TYPE_ID_VOID;
            }
            
            addr_in_hl = 0;
            dereference = 0;
            continue;
        }
        
        /* Handle member access: expr.member */
        if (tok == tokMember) {
            SYMBOL base_sym = factor_result.has_sym ? factor_result.sym : undefined_sym;
            uint8_t had_sym = factor_result.has_sym;
            
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
            
            FIELDINFO fi;
            uint16_t offset;
            if (!lookup_struct_member(check_type_id, &fi, &offset)) {
                return factor_result;
            }

            /* Load struct address if needed */
            if (!addr_in_hl) {
                if (had_sym) {
                    if (type_is_pointer(factor_result.type_id)) {
                        /* Pointer to struct: load pointer value then add offset */
                        emit_ld_symval(&base_sym);
                        addr_in_hl = 1;
                        /* Add offset after loading pointer */
                        if (offset) {
                            emit_ldde_immed(); emit_n(offset); emit_nl();
                            emit_add16();
                        }
                    } else {
                        /* Direct struct variable: compute address with member offset
                         * emit_sym_address_with_offset handles local/global correctly */
                        emit_sym_address_with_offset(&base_sym, offset);
                        addr_in_hl = 1;
                    }
                } else {
                    error(errSyntax);
                    return factor_result;
                }
            } else {
                /* HL already contains the struct address - just add offset if needed */
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
            continue;
        }
        
        /* Handle postfix increment/decrement: expr++ or expr-- */
        if (tok == tokInc || tok == tokDec) {
            if (factor_result.has_sym || addr_in_hl) {
                /* We have a clear lvalue */
                handle_incdec(0, factor_result.has_sym ? &factor_result.sym : NULL, 
                             factor_result.type_id, addr_in_hl);
                addr_in_hl = 0;  /* Result value is in HL */
                dereference = 0;
                factor_result.has_sym = 0;
            } else {
                /* No lvalue available - this is an error */
                error(errNotlvalue);
                break;
            }
            continue;
        }
    }
    
    /* Handle prefix ++/-- operators (applied after postfix operators are done) */
    if ((prefix_inc || prefix_dec) && !type_is_const(factor_result.type_id)) {
        TOKEN prefix_op = prefix_inc ? tokInc : tokDec;
        handle_incdec_internal(1, factor_result.has_sym ? &factor_result.sym : NULL, 
                             factor_result.type_id, addr_in_hl, prefix_op);
        addr_in_hl = 0;  /* After inc/dec, value is in HL, not an address */
        dereference = 0;
        factor_result.has_sym = 0;
    }
    
    /* Handle assignment */
    if (tok == tokAssign && !initial_deref) {
        if (addr_in_hl) {
            far_parse_assign(1, undefined_sym, 0, factor_result.type_id);
        } else if (factor_result.has_sym) {
            far_parse_assign(dereference, factor_result.sym, 0, factor_result.type_id);
        } else {
            error(errNotlvalue);
        }
        /* After assignment, clear flags to prevent unnecessary reload */
        factor_result.has_sym = 0;
        addr_in_hl = 0;
        dereference = 0;
    }
    
    /* After postfix operators, load value if needed */
    /* Load symbol values if we haven't loaded them yet */
    if (factor_result.has_sym && !initial_deref && !addr_in_hl) {
        if (is_func_or_proto(&factor_result.sym)) {
            emit_ld_immed(); emit_sname(factor_result.sym.name); emit_nl();
        } else if (type_is_struct(factor_result.sym.type_id) && !type_is_pointer(factor_result.sym.type_id)) {
            /* Structs are loaded as address, not value */
            emit_ld_symaddr(&factor_result.sym);
        } else {
            emit_ld_symval(&factor_result.sym);
        }
        addr_in_hl = 0;
    }
    
    /* If we have an address in HL that needs dereferencing, load it */
    if (addr_in_hl && dereference && !initial_deref) {
        emit_load(factor_result.type_id);
        addr_in_hl = 0;
        dereference = 0;
    }
    
    if (neg) emit_neg();
    if (not) emit_rtl("ccnot");
    if (cmpl) emit_rtl("cccom");
    return factor_result;
}

void far_parse_assign(uint8_t dereference, SYMBOL sym, uint8_t indexed, uint8_t type_id) MYCC {
    get_token(); // skip '='

    if (sym.klass != CLASS_UNDEFINED && type_is_const(sym.type_id)) {
        EXPR_RESULT r = far_parse_expr_delayconst(0, 0);
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
        
        uint8_t element_type_id = type_id;
        if (type_is_array(type_id) || type_is_pointer(type_id)) {
            element_type_id = type_get_element_type_id(type_id);
        }
        
        elementcount = parse_brace_initializer_elements(element_type_id);
        
        if (datalen != NO_LABEL) {
            emit_lblequ16(datalen, elementcount * type_size(type_id));
        }
        expect_RBrace();

        /* place the skip label here so code continues after the data */
        emit_lbl(skiplbl);
        return;
    }

    /* Handle struct assignment */
    if (type_is_struct(type_id) && !type_is_pointer(type_id)) {
        EXPR_RESULT r = far_parse_expr(0, type_id);
        
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
        far_parse_expr(0, 0);
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
    EXPR_RESULT r_result = far_parse_expr(0, type_id);

    /* If left-hand side is a delegate type (function pointer), ensure
     * assigned function or function-pointer has a matching signature. */
    if (type_is_function(type_get_element_type_id(type_id)) && type_get_indirection(type_id) == 1) {
        if (r_result.has_sym && is_func_or_proto(&r_result.sym)) {
            uint8_t left_sig = type_get_function_sig(type_get_element_type_id(type_id));
            uint8_t right_sig = r_result.sym.signature_id;
            if (left_sig == 0 || right_sig == 0xFF || left_sig != right_sig) {
                error(errTypeError);
            }
        } else if (!type_check_compatible(type_id, r_result.type_id)) {
            error(errTypeError);
        }
    }

    if (dereference) {
        emit_store(type_id);
    }
    else {
        emit_store_sym(&sym);
    }
}
