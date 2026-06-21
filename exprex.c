#include "znc.h"
#include "struct.h"
#include "shared.h"
#include "initializer.h"
#include "expr.h"

/* parse_ternary, far_parse_assign_ex, and far_parse_compound_assign live here in BANK_46.
 * All recursive expression calls go through the main-bank stubs
 * (parse_expr / parse_expr_delayconst / parse_assign) so that
 * bank-switch is handled correctly on ZX Next. */

void far_parse_ternary(EXPR_RESULT *result, uint8_t prec, uint8_t expected_type_id) MYCC {
    uint16_t altlbl = newlbl();
    uint16_t donelbl = newlbl();

    if (type_is_const(result->type_id)) {
        emit_ld_const(result->value);
    }
    emit_jp_false(altlbl);
    EXPR_RESULT ptyp = parse_expr(0, expected_type_id);  // primary expression
    emit_jp(donelbl);
    expect(tokColon, ':' );
    emit_lbl(altlbl);
    EXPR_RESULT atyp = parse_expr(prec, expected_type_id); // alternate expression

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

    result->has_sym = 0;
    if (expected_type_id != 0) {
        result->type_id = expected_type_id;
    } else {
        /* Infer result type from the true-branch, but ternary always produces a
         * runtime value so the const qualifier must be stripped. */
        uint8_t inferred = ptyp.type_id;
        if (type_is_const(inferred)) {
            TypeKind k = type_get_kind(inferred);
            if (k == TK_CHAR)       inferred = type_make_char(0);
            else if (k == TK_FIXED) inferred = type_make_fixed(0);
            else                    inferred = type_make_int(0);
        }
        result->type_id = inferred;
    }
    return;
}

void far_parse_compound_assign(TOKEN op, uint8_t dereference, SYMBOL *sym, uint8_t addr_in_hl, uint8_t type_id) MYCC {
    get_token(); // skip op=

    /* If dereference: push address first, then load old value.
     * Stack layout before op: [address] [old_value] ... with old_value on top.
     * emit_store (for dereference path) pops address from below the result. */
    if (dereference) {
        if (!addr_in_hl) {
            emit_ld_symval(sym);
        }
        emit_push();         /* push address for store-back */
        emit_load(type_id);  /* HL = old value */
    } else {
        emit_ld_symval(sym); /* HL = old value */
    }
    emit_push();             /* push old value onto stack */

    /* Parse RHS — result in HL */
    EXPR_RESULT rhs = parse_expr_delayconst(0, type_id);
    if (type_is_const(rhs.type_id)) {
        /* Fold fixed <-> int/char conversion at compile time */
        if (type_is_fixed(type_id) && !type_is_fixed(rhs.type_id) &&
            type_is_integral(rhs.type_id)) {
            rhs.value = (uint16_t)((int16_t)rhs.value << 4);
        } else if (!type_is_fixed(type_id) && type_is_fixed(rhs.type_id) &&
                   type_is_integral(type_id)) {
            rhs.value = (uint16_t)((int16_t)rhs.value >> 4);
        }
        emit_ld_immed(); emit_n(rhs.value); emit_nl();
    } else {
        /* Emit runtime fixed <-> int/char conversion if types differ */
        if (type_is_fixed(type_id) && !type_is_fixed(rhs.type_id) &&
            type_is_integral(rhs.type_id)) {
            emit_int_to_fixed();
        } else if (!type_is_fixed(type_id) && type_is_fixed(rhs.type_id) &&
                   type_is_integral(type_id)) {
            emit_fixed_to_int();
        }
    }

    /* DE = old value (left), HL = rhs (right) */
    emit_pop_de();

    switch (op) {
        case tokAddAssign: emit_add16(); break;
        case tokSubAssign: emit_sub16(); break;
        case tokMulAssign:
            if (type_is_fixed(type_id)) emit_rtl("ccfxmul");
            else emit_rtl("ccmult");
            break;
        case tokDivAssign:
            if (type_is_fixed(type_id)) emit_rtl("ccfxdiv");
            else emit_rtl("ccdiv");
            break;
        case tokModAssign:
            emit_rtl("ccdiv");
            emit_swap();
            break;
        case tokOrAssign:  emit_rtl("ccor");  break;
        case tokXorAssign: emit_rtl("ccxor"); break;
        case tokAndAssign: emit_rtl("ccand"); break;
        case tokShlAssign:
            emit_instrln("ld b,l");
            emit_instrln("bsla de,b");
            emit_swap();
            break;
        case tokShrAssign:
            emit_instrln("ld b,l");
            emit_instrln("bsra de,b");
            emit_swap();
            break;
        default:
            error(errIllegalOp);
            return;
    }

    /* HL = result; store back */
    if (dereference) {
        /* emit_store: swap(HL<->DE), pop address into HL, store DE at [HL] */
        emit_store(type_id);
    } else {
        emit_store_sym(sym);
    }
}


void far_parse_assign_ex(uint8_t dereference, SYMBOL *sym, uint8_t indexed, uint8_t type_id) MYCC {
    get_token(); // skip '='

    if (sym->klass != CLASS_UNDEFINED && type_is_const(sym->type_id)) {
        EXPR_RESULT r = parse_expr_delayconst(0, 0);
        if (!type_is_const(r.type_id)) error(errConstExpected);
        /* Apply fixed <-> int/char conversion at compile time, same as the non-const path. */
        if (type_is_fixed(sym->type_id) && !type_is_fixed(r.type_id) &&
            type_is_integral(r.type_id)) {
            r.value = (uint16_t)((int16_t)r.value << 4);
        } else if (!type_is_fixed(sym->type_id) && type_is_fixed(r.type_id) &&
                   type_is_integral(sym->type_id)) {
            r.value = (uint16_t)((int16_t)r.value >> 4);
        }
        sym->stk.offset = r.value;
        updatesym(sym);
        return;
    }

    /* If assigning a brace-initializer to a global symbol, emit data
     * directly for that symbol using the correct directive width.
     */
    if (tok == tokLBrace) {
        get_token(); // skip '{'
        uint16_t elementcount = 0;
        uint16_t skiplbl = newlbl();
        uint16_t datalbl = newlbl();
        uint16_t datalen = NO_LABEL;

        if (sym->klass == CLASS_UNDEFINED && !dereference) error(errNotlvalue);

        if (!dereference && !type_is_array(type_id) && !type_is_struct(type_id)) {
            /* Pointer/scalar variable: store the address of the inline data into the variable */
            emit_ld_immed(); emit_lblref(datalbl); emit_nl();
            if (sym->klass != CLASS_UNDEFINED) {
                emit_store_sym(sym);
            }
            else {
                emit_store(type_id);
            }
        }
        else {
            datalen = newlbl();

            if (!dereference && (type_is_array(type_id) || type_is_struct(type_id))) {
                /* Array/struct variable: destination is the variable's own storage address */
                emit_ld_symaddr(sym);
            }
            else if (sym->klass != CLASS_UNDEFINED) {
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

        uint8_t element_type_id = type_id;
        if (type_is_array(type_id) || type_is_pointer(type_id)) {
            element_type_id = type_get_element_type_id(type_id);
        }

        elementcount = parse_brace_initializer_elements(element_type_id);

        if (datalen != NO_LABEL) {
            uint16_t data_size;
            if (type_is_struct(type_id)) {
                /* Struct: entire struct is emitted as one block; elementcount is always 1 */
                data_size = type_size(type_id);
            } else {
                data_size = elementcount * type_size(element_type_id);
            }
            emit_lblequ16(datalen, data_size);
        }
        expect_RBrace();

        /* place the skip label here so code continues after the data */
        emit_lbl(skiplbl);
        return;
    }

    /* Handle struct assignment */
    if (type_is_struct(type_id) && !type_is_pointer(type_id)) {
        EXPR_RESULT r = parse_expr(0, type_id);

        /* Verify types match */
        if (!type_is_struct(r.type_id) || type_get_struct_id(r.type_id) != type_get_struct_id(type_id)) {
            error(errTypeError);
            return;
        }

        /* HL now contains the source address (from parse_expr loading the struct address)
         * We need to set up dest address and copy */
        uint16_t struct_size = type_size(type_id);

        if (dereference) {
            /* Dereferenced pointer case: *p1 = p2 or *p1 = *p2
             * HL = source address (from parsing right side)
             * Need to get destination address from pointer value
             */
            emit_push();  /* Save source address on stack */

            if (sym->klass != CLASS_UNDEFINED) {
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

            if (sym->klass != CLASS_UNDEFINED) {
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

    if (sym->klass == CLASS_UNDEFINED) {
        // HL contains address to write to
        // TODO: Optimize for loading constants directly into memory without going through registers
        emit_push();
        parse_expr(0, type_id);
        emit_store(type_id);
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
    EXPR_RESULT r_result = parse_expr_delayconst(0, type_id);

    /* Reject cross-enum assignment: enum A <- enum B is always an error.
     * Same-enum is allowed; enum <- scalar and scalar <- enum are allowed. */
    if (!type_check_compatible(r_result.type_id, type_id)) {
        error(errTypeError);
    }

    if (type_is_const(r_result.type_id)) {
        /* Fold fixed <-> int/char conversion at compile time for constant RHS */
        if (type_is_fixed(type_id) && !type_is_fixed(r_result.type_id) &&
            type_is_integral(r_result.type_id)) {
            /* int/char constant -> fixed: pre-shift value into Q4 at compile time */
            r_result.value = (uint16_t)((int16_t)r_result.value << 4);
        } else if (!type_is_fixed(type_id) && type_is_fixed(r_result.type_id) &&
                   type_is_integral(type_id)) {
            /* fixed constant -> int/char: truncate Q4 value at compile time */
            r_result.value = (uint16_t)((int16_t)r_result.value >> 4);
        }
        emit_ld_const(r_result.value);
    } else if (r_result.has_sym && is_func_or_proto(&r_result.sym)) {
        emit_ld_immed(); emit_sname_id(r_result.sym.name_id); emit_nl();
    } else {
        /* Emit runtime fixed <-> int/char conversion if types differ */
        if (type_is_fixed(type_id) && !type_is_fixed(r_result.type_id) &&
            type_is_integral(r_result.type_id)) {
            /* int/char -> fixed: shift left 4 */
            emit_int_to_fixed();
        } else if (!type_is_fixed(type_id) && type_is_fixed(r_result.type_id) &&
                   type_is_integral(type_id)) {
            /* fixed -> int/char: shift right 4 (arithmetic) */
            emit_fixed_to_int();
        }
    }

    /* If left-hand side is a delegate type (function pointer), ensure
     * assigned function or function-pointer has a matching signature. */
    if (type_is_function(type_get_element_type_id(type_id)) && type_get_indirection(type_id) == 1) {
        if (r_result.has_sym && is_func_or_proto(&r_result.sym)) {
            uint8_t left_sig = type_get_function_sig(type_get_element_type_id(type_id));
            uint8_t right_sig = r_result.sym.fn.signature_id;
            if (right_sig == 0xFF || !signature_check(left_sig, right_sig)) {
                error(errTypeError);
            }
        } else if (!type_check_compatible(r_result.type_id, type_id)) {
            error(errTypeError);
        }
    }

    if (dereference) {
        emit_store(type_id);
    }
    else {
        emit_store_sym(sym);
    }
}

void far_parse_abs(EXPR_RESULT* result) MYCC {
    get_token(); // skip 'abs'
    expect_LParen();
    *result = parse_expr_delayconst(0, 0);
    expect_RParen();

    if (!type_is_integral(result->type_id) && !type_is_fixed(result->type_id)) {
        error(errTypeError);
    }

    if (type_is_const(result->type_id)) {
        if (result->value & 0x8000) result->value = -result->value;
        return;
    }
    else if (result->has_sym) {
        emit_ld_symval(&result->sym);
    }
    emit_rtl("ccabs");
}
