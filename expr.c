#include "znc.h"
#include "struct.h"
#include "shared.h"
#include "initializer.h"

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

int8_t prec(TOKEN op) MYCC {
    switch (op) {
        case tokCond:                           return COND_PREC;
        case tokOr:                             return OR_PREC;
        case tokAnd:                            return AND_PREC;
        case tokBitOr:                          return BIT_OR_PREC;
        case tokBitXor:                         return BIT_XOR_PREC;
        case tokBitAnd:                         return BIT_AND_PREC;
        case tokEq:     case tokNeq:            return EQ_PREC;
        case tokLt:     case tokLeq:
        case tokGt:     case tokGeq:            return REL_OP_PREC;
        case tokShl:    case tokShr:            return SHIFT_OP_PREC;
        case tokPlus:   case tokMinus:          return ADD_PREC;
        case tokStar:   case tokDiv: case tokMod: return MUL_PREC;
        default:                                return 0;
    }
}

EXPR_RESULT parse_factor(uint8_t dereference, uint8_t expected_type_id) MYCC;
EXPR_RESULT far_parse_expr(uint8_t minprec, uint8_t expected_type_id) MYCC;
EXPR_RESULT far_parse_expr_delayconst(uint8_t minprec, uint8_t expected_type_id) MYCC;


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

/* Helper: Emit inc/dec of HL by 'step' (modifies HL, isdec=1 for decrement) */
static void emit_incdec_step(uint16_t step, uint8_t isdec) MYCC {
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
}

/* Helper: returns 1 for ops where both operands must share the same numeric
 * scale — arithmetic and comparisons both require this. Bitwise, shift and
 * mod ops use the raw bit pattern, so no fixed<->int alignment is needed. */
static uint8_t op_needs_fixed_align(TOKEN op) MYCC {
    switch (op) {
        case tokPlus: case tokMinus: case tokStar: case tokDiv: case tokMod:
        case tokLt: case tokLeq: case tokGt: case tokGeq:
        case tokEq: case tokNeq:
            return 1;
        default:
            return 0;
    }
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
        /* Convert const fixed index to int by shifting right 4 */
        if (type_is_fixed(index_result.type_id))
            index_result.value = (uint16_t)((int16_t)index_result.value >> 4);
        index_result.value = (uint16_t)(index_result.value * scale);
    } else {
        /* Convert runtime fixed index to int before element scaling */
        if (type_is_fixed(index_result.type_id))
            emit_fixed_to_int();
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
            emit_instr("ld hl,("); emit_sname_id(sym->name_id); emit_ch('+'); 
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
    } else if (type_is_fixed(lvalue_type_id)) {
        step = 16; /* Q4: increment by 1.0 = 1 << 4 */
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
        
        emit_incdec_step(step, isdec);
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

        emit_incdec_step(step, isdec);
        
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
        error(errNotAStruct);
        return 0;
    }
    if (tok != tokIdent) {
        error(errExpected_s, "member name");
        return 0;
    }
    
    int sid = (int)type_get_struct_id(check_type_id) - 1;
    int fid = find_struct_field(sid, token);
    if (fid < 0) {
        error(errNotDefined_s, token);
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

            /* Compute pointer arithmetic scaling (only for arithmetic ops, not comparisons) */
            uint8_t is_cmp_op = (op == tokEq || op == tokNeq || op == tokLt || op == tokLeq || op == tokGt || op == tokGeq);
            if (!is_cmp_op) {
                /* Fixed-type offsets in pointer arithmetic are first converted to int (>> 4) */
                if ((type_is_pointer(left.type_id) || type_is_array(left.type_id)) &&
                    (type_is_int(r_result.type_id) || type_is_fixed(r_result.type_id))) {
                    uint8_t elem_id = type_get_element_type_id(left.type_id);
                    scaleR = type_size(elem_id);
                    /* If the offset is fixed, convert it to int BEFORE scaling. */
                    if (type_is_fixed(r_result.type_id)) {
                        if (type_is_const(r_result.type_id)) {
                            /* const fixed -> const int (shift right 4) */
                            r_result.value = (uint16_t)((int16_t)r_result.value >> 4);
                            r_result.type_id = type_make_int(1);
                        }
                        /* runtime fixed: type_id stays TK_FIXED so right_is_fixed stays 1;
                         * the tokPlus/tokMinus handlers will call emit_fixed_to_int() before scaling */
                    }
                }
                if ((type_is_pointer(r_result.type_id) || type_is_array(r_result.type_id)) &&
                    (type_is_int(left.type_id) || type_is_fixed(left.type_id))) {
                    uint8_t elem_id = type_get_element_type_id(r_result.type_id);
                    scaleL = type_size(elem_id);
                    if (type_is_const(left.type_id) && type_is_fixed(left.type_id))
                        left.value = (uint16_t)((int16_t)left.value >> 4);
                }
            }

            if (scaleL && scaleR) error(errIllegalOp);

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
                        error(errIllegalOp);
                        break;
                }
            }

            if (type_is_const(left.type_id) && type_is_const(r_result.type_id)) {
                uint8_t lf = type_is_fixed(left.type_id);
                uint8_t rf = type_is_fixed(r_result.type_id);
                /* For arithmetic/comparison ops mixing fixed and int/char constants,
                 * convert the non-fixed operand into Q4 format before folding. */
                if ((lf || rf) && op_needs_fixed_align(op)) {
                    if (lf && !rf) { r_result.value = r_result.value << 4; rf = 1; }
                    else if (rf && !lf) { left.value = left.value << 4; lf = 1; }
                }
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
                    case tokStar:
                        /* fixed * fixed in Q4: (a*b)>>4 */
                        /* Cast to int32_t before multiply to avoid overflow on 16-bit targets (SDCC). */
                        if (lf && rf) left.value = (uint16_t)(((int32_t)(int16_t)left.value * (int32_t)(int16_t)r_result.value) >> 4);
                        else left.value = left.value * r_result.value;
                        break;
                    case tokDiv:
                        /* fixed / fixed in Q4: (a<<4)/b */
                        /* Cast to int32_t before shift to avoid overflow on 16-bit targets (SDCC). */
                        if (lf && rf) left.value = (uint16_t)(((int32_t)(int16_t)left.value << 4) / (int32_t)(int16_t)r_result.value);
                        else left.value = left.value / r_result.value;
                        break;
                    case tokMod: left.value = left.value % r_result.value; break;
                    case tokShl: left.value = left.value << (rf ? (uint16_t)((int16_t)r_result.value >> 4) : r_result.value); break;
                    case tokShr: left.value = left.value >> (rf ? (uint16_t)((int16_t)r_result.value >> 4) : r_result.value); break;
                    case tokBitOr: left.value = left.value | r_result.value; break;
                    case tokBitAnd: left.value = left.value & r_result.value; break;
                    case tokBitXor: left.value = left.value ^ r_result.value; break;
                    default:
                        error(errIllegalOp);
                        break;
                }
                /* Result type: comparisons always produce int (0 or 1); arithmetic preserves fixed */
                switch (op) {
                    case tokEq: case tokNeq:
                    case tokLt: case tokLeq:
                    case tokGt: case tokGeq:
                        left.type_id = type_make_int(1);
                        break;
                    case tokShl: case tokShr:
                        /* Shift result follows the left operand only (right is the count) */
                        if (lf) left.type_id = type_make_fixed(1);
                        break;
                    default:
                        if (lf || rf) left.type_id = type_make_fixed(1);
                        break;
                }
                left.has_sym = 0;  /* Result is a folded value, not a direct symbol reference */
                continue;
            }

            /* Load constants with pre-applied scaling */
            uint8_t left_is_fixed = type_is_fixed(left.type_id);
            uint8_t right_is_fixed = type_is_fixed(r_result.type_id);
            uint8_t either_fixed = left_is_fixed || right_is_fixed;

            if (type_is_const(left.type_id)) {
                uint16_t val = left.value;
                if (scaleL) val = val * scaleL;
                /* If right is fixed and left is a plain int constant, pre-shift it */
                if (right_is_fixed && !type_is_fixed(left.type_id) && op_needs_fixed_align(op)) {
                    val = val << 4;
                    left_is_fixed = 1;
                }
                emit_ldde_immed(); emit_n(val); emit_nl();
                scaleL = 0;
            }
            else if (type_is_const(r_result.type_id)) {
                uint16_t val = r_result.value;
                if (scaleR) val = val * scaleR;
                /* If left is fixed and right is a plain int constant, pre-shift it */
                if (left_is_fixed && !type_is_fixed(r_result.type_id) && op_needs_fixed_align(op)) {
                    val = val << 4;
                    right_is_fixed = 1;
                }
                emit_ld_immed(); emit_n(val); emit_nl();
                scaleR = 0;
                emit_pop_de();
            }
            else {
                emit_pop_de();
            }
            either_fixed = left_is_fixed || right_is_fixed;

            /* When mixing fixed with int/char, convert the non-fixed side to fixed.
             * At this point: DE = left, HL = right.
             * Converting int->fixed = shift left 4 bits.
             * Skip for pointer arithmetic: tokPlus/tokMinus handlers call emit_fixed_to_int()
             * on the fixed offset themselves, before element-size scaling.
             */
            if (either_fixed && op_needs_fixed_align(op) && !pointer) {
                if (left_is_fixed && !right_is_fixed) {
                    /* HL (right) is int/char, convert to fixed by shifting left 4 */
                    emit_int_to_fixed();
                } else if (right_is_fixed && !left_is_fixed) {
                    /* DE (left) is int/char, convert to fixed by shifting left 4 */
                    emit_instrln("ex de,hl");
                    emit_int_to_fixed();
                    emit_instrln("ex de,hl");
                }
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
                    if (scaleR) { if (right_is_fixed) emit_fixed_to_int(); emit_scale_reg(scaleR, SCALE_HL); }
                    if (scaleL) { if (left_is_fixed) { emit_instrln("ex de,hl"); emit_fixed_to_int(); emit_instrln("ex de,hl"); } emit_scale_reg(scaleL, SCALE_DE); }
                    emit_add16();
                    break;
                case tokMinus:
                    if (scaleR) { if (right_is_fixed) emit_fixed_to_int(); emit_scale_reg(scaleR, SCALE_HL); }
                    if (scaleL) { if (left_is_fixed) { emit_instrln("ex de,hl"); emit_fixed_to_int(); emit_instrln("ex de,hl"); } emit_scale_reg(scaleL, SCALE_DE); }
                    emit_sub16();
                    break;
                case tokStar:
                    if (either_fixed)
                        emit_rtl("ccfxmul");
                    else
                        emit_rtl("ccmult");
                    break;
                case tokDiv:
                    if (either_fixed)
                        emit_rtl("ccfxdiv");
                    else
                        emit_rtl("ccdiv");
                    break;
                case tokMod:
                    emit_rtl("ccdiv");
                    emit_swap();
                    break;
                case tokShl:
                    if (right_is_fixed) emit_fixed_to_int();
                    emit_instrln("ld b,l");
                    emit_instrln("bsla de,b");
                    emit_swap();
                    break;
                case tokShr:
                    if (right_is_fixed) emit_fixed_to_int();
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
                    error(errIllegalOp);
                    break;
            }
            
            /* After emitting runtime code, result is no longer const or a simple symbol */
            if (type_is_const(left.type_id)) {
                left.type_id = TYPE_ID_INT;
            }
            /* Comparison/relational operators always produce an int (0 or 1) result */
            if (op == tokEq || op == tokNeq || op == tokLt || op == tokLeq || op == tokGt || op == tokGeq) {
                left.type_id = TYPE_ID_INT;
            }
            /* If either operand was fixed and this is an arithmetic op, result is fixed */
            if (either_fixed && (op == tokPlus || op == tokMinus || op == tokStar || op == tokDiv || op == tokMod)) {
                left.type_id = TYPE_ID_FIXED;
            }
            /* Shift result follows the left operand type only (right operand is the count) */
            if (left_is_fixed && (op == tokShl || op == tokShr)) {
                left.type_id = TYPE_ID_FIXED;
            }
            left.has_sym = 0;  /* Result is computed, not a direct symbol reference */
        }
    }
    return left;
}

/* Grammar: <expr> ::= <assignment>
 *          <assignment> ::= <ternary> [ ("=" | <assign_op>) <assignment> ]
 * Assignment is handled inside parse_factor when tokAssign/compound-assign
 * follows a resolved primary, giving it lower precedence than all binary ops.
 * This matches the corrected right-associative assignment rule. */
EXPR_RESULT far_parse_expr(uint8_t minprec, uint8_t expected_type_id) MYCC {
    EXPR_RESULT expr_result = parse_factor(0, expected_type_id);
    expr_result = parse_op_right(expr_result, minprec, expected_type_id);
    
    if (type_is_const(expr_result.type_id)) {
        /* Coerce constant to expected type at compile time */
        uint16_t val = expr_result.value;
        if (expected_type_id != 0 && type_is_fixed(expected_type_id) &&
            !type_is_fixed(expr_result.type_id) &&
            (type_is_int(expr_result.type_id) || type_is_char(expr_result.type_id))) {
            val = (uint16_t)((int16_t)val << 4);
        } else if (expected_type_id != 0 && !type_is_fixed(expected_type_id) &&
                   type_is_fixed(expr_result.type_id) &&
                   (type_is_int(expected_type_id) || type_is_char(expected_type_id))) {
            val = (uint16_t)((int16_t)val >> 4);
        }
        emit_ld_const(val);
    } else if (expr_result.has_sym && is_func_or_proto(&expr_result.sym)) {
        /* Function address: emit symbol load */
        emit_ld_immed(); emit_sname_id(expr_result.sym.name_id); emit_nl();
    } else if (expected_type_id != 0) {
        /* Emit runtime fixed <-> int/char conversion when expected type differs */
        if (type_is_fixed(expected_type_id) && !type_is_fixed(expr_result.type_id) &&
            (type_is_int(expr_result.type_id) || type_is_char(expr_result.type_id))) {
            emit_int_to_fixed();
        } else if (!type_is_fixed(expected_type_id) && type_is_fixed(expr_result.type_id) &&
                   (type_is_int(expected_type_id) || type_is_char(expected_type_id))) {
            emit_fixed_to_int();
        }
    }

    return expr_result;
}

EXPR_RESULT far_parse_expr_delayconst(uint8_t minprec, uint8_t expected_type_id) MYCC {
    EXPR_RESULT expr_result = parse_factor(0, expected_type_id);
    expr_result = parse_op_right(expr_result, minprec, expected_type_id);
    return expr_result;
}

/* Forward declarations for parse_factor helpers */
static EXPR_RESULT parse_factor_ampersand(void) MYCC;
static void parse_factor_postfix(EXPR_RESULT* result, uint8_t* dereference, uint8_t* addr_in_hl) MYCC;

/* Grammar: "&" <lvalue_expr>
 * Handles &ident, &ident[expr], &ident.field (chains of [] and . are supported).
 * Note: &(*ptr) is not explicitly handled here, but &(*ptr) == ptr, so callers
 * can use the pointer value directly. Dereference-then-address-of is a no-op. */
static EXPR_RESULT parse_factor_ampersand(void) MYCC {
    EXPR_RESULT result;
    result.type_id = 0;
    result.value = 0;
    result.has_sym = 0;

    get_token(); // skip '&'
    if (tok != tokIdent) error(errNotlvalue);

    SYMBOL sym = lookupIdent(token);
    if (not_defined(&sym)) {
        error(errNotDefined_s, token);
    }

    get_token(); // skip identifier
    result.type_id = sym.type_id;
    result.has_sym = 1;
    result.sym = sym;

    uint8_t addr_base_sym = 1;

    while (tok == tokLBrack || tok == tokMember) {
        if (tok == tokLBrack) {
            uint8_t elemtype_id = type_get_element_type_id(result.type_id);
            uint16_t scale = (uint16_t)type_size(elemtype_id);

            get_token(); // skip '['
            EXPR_RESULT index_result = far_parse_expr_delayconst(0, TYPE_ID_INT);
            expect(tokRBrack, ']');

            if (addr_base_sym) {
                if (type_is_const(index_result.type_id)) {
                    if (type_is_fixed(index_result.type_id))
                        index_result.value = (uint16_t)((int16_t)index_result.value >> 4);
                    uint16_t total_offset = index_result.value * scale;
                    emit_ld_symaddr_offset(&sym, total_offset);
                } else {
                    if (type_is_fixed(index_result.type_id)) emit_fixed_to_int();
                    if (scale > 1) emit_scale_reg(scale, 1);
                    emit_swap();
                    emit_ld_symaddr(&sym);
                    emit_add16();
                }
                addr_base_sym = 0;
            } else {
                emit_push();
                if (type_is_const(index_result.type_id)) {
                    if (type_is_fixed(index_result.type_id))
                        index_result.value = (uint16_t)((int16_t)index_result.value >> 4);
                    uint16_t offset = index_result.value * scale;
                    if (offset) {
                        emit_pop_de();
                        emit_ldde_immed_n(offset);
                        emit_add16();
                    } else {
                        emit_pop_hl();
                    }
                } else {
                    if (type_is_fixed(index_result.type_id)) emit_fixed_to_int();
                    if (scale > 1) emit_scale_reg(scale, SCALE_HL);
                    emit_pop_de();
                    emit_add16();
                }
            }
            result.type_id = elemtype_id;
        } else if (tok == tokMember) {
            get_token();
            uint8_t check_type_id = result.type_id;
            if (type_is_pointer(check_type_id))
                check_type_id = type_get_element_type_id(check_type_id);
            if (!type_is_struct(check_type_id)) { error(errNotAStruct); break; }

            FIELDINFO fi;
            uint16_t offset;
            if (!lookup_struct_member(check_type_id, &fi, &offset)) break;

            if (addr_base_sym) {
                emit_sym_address_with_offset(&sym, offset);
                addr_base_sym = 0;
            } else {
                if (offset) { emit_ldde_immed_n(offset); emit_add16(); }
            }
            result.type_id = fi.type_id;
        }
    }

    if (addr_base_sym) emit_ld_symaddr(&sym);

    result.type_id = type_make_pointer(result.type_id, 1);
    result.has_sym = 0;
    return result;
}

static void parse_factor_postfix(EXPR_RESULT* result, uint8_t* dereference, uint8_t* addr_in_hl) MYCC {
    while (tok == tokLBrack || tok == tokLParen || tok == tokMember || tok == tokInc || tok == tokDec) {

        if (tok == tokLBrack) {
            SYMBOL base_sym = result->has_sym ? result->sym : undefined_sym;
            uint8_t had_sym = result->has_sym;

            if (*addr_in_hl) {
                if (*dereference && type_is_pointer(result->type_id)) {
                    emit_load(result->type_id);
                    *dereference = 0;
                }
            } else if (result->has_sym) {
                if (!type_is_pointer(result->sym.type_id) && !type_is_array(result->sym.type_id)) {
                    error(errTypeError); break;
                }
                if (type_is_pointer(result->sym.type_id)) {
                    emit_ld_symval(&result->sym);
                    had_sym = 0;
                }
            } else {
                if (!type_is_pointer(result->type_id) && !type_is_array(result->type_id)) {
                    error(errTypeError); break;
                }
            }

            result->has_sym = 0;
            uint8_t elemtype_id = type_get_element_type_id(result->type_id);
            get_token(); // skip '['
            if (!had_sym) emit_push();

            EXPR_RESULT index_result = far_parse_expr_delayconst(0, TYPE_ID_INT);
            expect(tokRBrack, ']');
            uint16_t scale = (uint16_t)type_size(elemtype_id);

            if (type_is_const(index_result.type_id)) {
                if (type_is_fixed(index_result.type_id))
                    index_result.value = (uint16_t)((int16_t)index_result.value >> 4);
                uint16_t offset = index_result.value * scale;
                if (had_sym && type_is_array(base_sym.type_id)) {
                    if (offset) emit_ld_symaddr_offset(&base_sym, offset);
                    else emit_ld_symaddr(&base_sym);
                } else {
                    if (!had_sym) emit_pop_hl();
                    if (offset) { emit_ldde_immed_n(offset); emit_add16(); }
                }
            } else {
                if (type_is_fixed(index_result.type_id)) emit_fixed_to_int();
                if (scale > 1) emit_scale_reg(scale, 1);
                if (had_sym && type_is_array(base_sym.type_id)) {
                    emit_swap();
                    emit_ld_symaddr(&base_sym);
                } else {
                    emit_pop_de();
                }
                emit_add16();
            }

            result->type_id = elemtype_id;
            *dereference = 1;
            *addr_in_hl = 1;
            continue;
        }

        if (tok == tokLParen) {
            SYMBOL call_sym = result->has_sym ? result->sym : undefined_sym;
            uint8_t had_sym = result->has_sym;
            uint8_t callee_type_id = result->type_id;

            result->has_sym = 0;

            if (*addr_in_hl && *dereference) {
                emit_load(result->type_id);
                *addr_in_hl = 0;
                *dereference = 0;
            }

            PTR_LOCATION ptr_loc = had_sym ? PTR_IN_SYMBOL : PTR_IN_HL;
            parse_funccall(&call_sym, ptr_loc);

            result->type_id = TYPE_ID_VOID;
            if (had_sym && is_func_or_proto(&call_sym) && call_sym.fn.signature_id != 0xFF) {
                result->type_id = signature_get_return_type(call_sym.fn.signature_id);
            } else {
                uint8_t ftype_id = 0xFF;
                if (had_sym && call_sym.klass == VARIABLE && type_get_indirection(call_sym.type_id) == 1)
                    ftype_id = type_get_element_type_id(call_sym.type_id);
                else if (type_get_indirection(callee_type_id) == 1)
                    ftype_id = type_get_element_type_id(callee_type_id);
                if (ftype_id != 0xFF && type_is_function(ftype_id)) {
                    uint8_t sig = type_get_function_sig(ftype_id);
                    if (sig != 0xFF) result->type_id = signature_get_return_type(sig);
                }
            }

            *addr_in_hl = 0;
            *dereference = 0;
            continue;
        }

        if (tok == tokMember) {
            SYMBOL base_sym = result->has_sym ? result->sym : undefined_sym;
            uint8_t had_sym = result->has_sym;

            result->has_sym = 0;
            get_token();

            uint8_t check_type_id = result->type_id;
            if (type_is_pointer(check_type_id))
                check_type_id = type_get_element_type_id(check_type_id);
            if (!type_is_struct(check_type_id)) { error(errNotAStruct); return; }

            FIELDINFO fi;
            uint16_t offset;
            if (!lookup_struct_member(check_type_id, &fi, &offset)) return;

            if (!*addr_in_hl) {
                if (had_sym) {
                    if (type_is_pointer(result->type_id)) {
                        emit_ld_symval(&base_sym);
                        *addr_in_hl = 1;
                        if (offset) { emit_ldde_immed(); emit_n(offset); emit_nl(); emit_add16(); }
                    } else {
                        emit_sym_address_with_offset(&base_sym, offset);
                        *addr_in_hl = 1;
                    }
                } else { error(errNotlvalue); return; }
            } else {
                if (offset) { emit_ldde_immed(); emit_n(offset); emit_nl(); emit_add16(); }
            }

            result->type_id = fi.type_id;
            if (type_is_array(result->type_id)) {
                uint8_t elem_id = type_get_element_type_id(result->type_id);
                result->type_id = type_make_pointer(elem_id, 1);
                *dereference = 0;
            } else {
                *dereference = 1;
            }
            *addr_in_hl = 1;
            continue;
        }

        if (tok == tokInc || tok == tokDec) {
            if (result->has_sym || *addr_in_hl) {
                handle_incdec(0, result->has_sym ? &result->sym : NULL,
                              result->type_id, *addr_in_hl);
                *addr_in_hl = 0;
                *dereference = 0;
                result->has_sym = 0;
            } else {
                error(errNotlvalue);
                break;
            }
            continue;
        }
    }
}

#define is_compound_assign(t) ((t) >= tokAddAssign && (t) <= tokShrAssign)

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
        case tokLBrace: {
            if (!type_is_array(expected_type_id) 
             && !type_is_pointer(expected_type_id)) error(errTypeError);

            uint8_t element_type_id = type_get_element_type_id(expected_type_id);
            get_token(); // skip '{'
            uint16_t skiplbl = newlbl();
            emit_jp(skiplbl);
            uint16_t tmplbl = newlbl();
            emit_lbl(tmplbl);
                        
            parse_brace_initializer_elements(expected_type_id);
            
            expect_RBrace();
            emit_lbl(skiplbl);
            emit_ld_immed(); emit_lblref(tmplbl); emit_nl();
            factor_result.type_id = expected_type_id;
            break;
        }
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
            factor_result = parse_factor_ampersand();
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
                uint8_t elem_type_id = type_get_element_type_id(factor_result.type_id);
                parse_assign(1, factor_result.has_sym ? factor_result.sym : undefined_sym, 0, elem_type_id);
            } else if (is_compound_assign(tok)) {
                uint8_t elem_type_id = type_get_element_type_id(factor_result.type_id);
                SYMBOL csym = factor_result.has_sym ? factor_result.sym : undefined_sym;
                parse_compound_assign(tok, 1, csym, 0, elem_type_id);
            } else {
                uint8_t elem_type_id = type_get_element_type_id(factor_result.type_id);
                /* When dereferencing a plain scalar used as a raw address (e.g. int used
                 * as an address), elem_type_id is VOID. Use the expected (LHS) type to
                 * decide the load width so that:
                 *   byte* ball  = *udg_ptr  -> 16-bit word load (pointer expected)
                 *   byte  value = *udg_ptr  ->  8-bit byte load (byte expected)
                 */
                uint8_t load_type_id = elem_type_id;
                if (type_is_void(elem_type_id) && expected_type_id != 0) {
                    load_type_id = expected_type_id;
                }
                /* For reading (not assignment), load the value */
                if (!type_is_const(factor_result.type_id)) {
                    if (factor_result.has_sym) {
                        emit_ld_symval(&factor_result.sym);
                    }
                }
                /* For structs, we want the address, not the dereferenced value */
                if (!type_is_struct(factor_result.type_id)) {
                    emit_load(load_type_id);
                }
                /* After dereferencing, result is the load type, no longer the original symbol */
                factor_result.type_id = load_type_id;
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
            if (cmpl) {
                intval = ~intval;
                cmpl = 0;
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

        case tokFixedLit:
            /* intval already holds the 12.4 Q4 value from the scanner */
            if (neg) {
                intval = -intval;
                neg = 0;
            }
            get_token();
            if (dereference) {
                emit_ld_const(intval);
                emit_load(TYPE_ID_FIXED);
                factor_result.type_id = TYPE_ID_FIXED;
            } else {
                factor_result.type_id = type_make_fixed(1); /* const */
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
                factor_result.value = sym.stk.offset;
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
            error(errExpected_s, "expression");
            break;
    }

    /* Postfix operators loop - handles [], (), ., ++, -- */
    parse_factor_postfix(&factor_result, &dereference, &addr_in_hl);
    
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
    if ((tok == tokAssign || is_compound_assign(tok)) && !initial_deref) {
        if (tok == tokAssign) {
            if (addr_in_hl) {
                parse_assign(1, undefined_sym, 0, factor_result.type_id);
            } else if (factor_result.has_sym) {
                parse_assign(dereference, factor_result.sym, 0, factor_result.type_id);
            } else {
                error(errNotlvalue);
            }
        } else {
            if (addr_in_hl) {
                parse_compound_assign(tok, 1, undefined_sym, 1, factor_result.type_id);
            } else if (factor_result.has_sym) {
                parse_compound_assign(tok, dereference, factor_result.sym, 0, factor_result.type_id);
            } else {
                error(errNotlvalue);
            }
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
            emit_ld_immed(); emit_sname_id(factor_result.sym.name_id); emit_nl();
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

