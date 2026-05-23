#include "znc.h"
#include "struct.h"
#include "shared.h"
#include "initializer.h"
#include "expr.h"

/* parse_ternary and far_parse_assign_ex live here in BANK_46.
 * All recursive expression calls go through the main-bank stubs
 * (parse_expr / parse_expr_delayconst / parse_assign) so that
 * bank-switch is handled correctly on ZX Next. */

EXPR_RESULT far_parse_ternary(EXPR_RESULT expr_result, uint8_t prec, uint8_t expected_type_id) MYCC {
    uint16_t altlbl = newlbl();
    uint16_t donelbl = newlbl();

    if (type_is_const(expr_result.type_id)) {
        emit_ld_const(expr_result.value);
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

    expr_result.has_sym = 0;
    if (expected_type_id != 0) {
        expr_result.type_id = expected_type_id;
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
        expr_result.type_id = inferred;
    }
    return expr_result;
}

void far_parse_assign_ex(uint8_t dereference, SYMBOL sym, uint8_t indexed, uint8_t type_id) MYCC {
    get_token(); // skip '='

    if (sym.klass != CLASS_UNDEFINED && type_is_const(sym.type_id)) {
        EXPR_RESULT r = parse_expr_delayconst(0, 0);
        if (!type_is_const(r.type_id)) error_expect_const();
        /* Apply fixed <-> int/char conversion at compile time, same as the non-const path. */
        if (type_is_fixed(sym.type_id) && !type_is_fixed(r.type_id) &&
            (type_is_int(r.type_id) || type_is_char(r.type_id))) {
            r.value = (uint16_t)((int16_t)r.value << 4);
        } else if (!type_is_fixed(sym.type_id) && type_is_fixed(r.type_id) &&
                   (type_is_int(sym.type_id) || type_is_char(sym.type_id))) {
            r.value = (uint16_t)((int16_t)r.value >> 4);
        }
        sym.stk.offset = r.value;
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
        parse_expr(0, type_id);
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
    EXPR_RESULT r_result = parse_expr_delayconst(0, type_id);

    if (type_is_const(r_result.type_id)) {
        /* Fold fixed <-> int/char conversion at compile time for constant RHS */
        if (type_is_fixed(type_id) && !type_is_fixed(r_result.type_id) &&
            (type_is_int(r_result.type_id) || type_is_char(r_result.type_id))) {
            /* int/char constant -> fixed: pre-shift value into Q4 at compile time */
            r_result.value = (uint16_t)((int16_t)r_result.value << 4);
        } else if (!type_is_fixed(type_id) && type_is_fixed(r_result.type_id) &&
                   (type_is_int(type_id) || type_is_char(type_id))) {
            /* fixed constant -> int/char: truncate Q4 value at compile time */
            r_result.value = (uint16_t)((int16_t)r_result.value >> 4);
        }
        emit_ld_const(r_result.value);
    } else if (r_result.has_sym && is_func_or_proto(&r_result.sym)) {
        emit_ld_immed(); emit_sname_id(r_result.sym.name_id); emit_nl();
    } else {
        /* Emit runtime fixed <-> int/char conversion if types differ */
        if (type_is_fixed(type_id) && !type_is_fixed(r_result.type_id) &&
            (type_is_int(r_result.type_id) || type_is_char(r_result.type_id))) {
            /* int/char -> fixed: shift left 4 */
            emit_int_to_fixed();
        } else if (!type_is_fixed(type_id) && type_is_fixed(r_result.type_id) &&
                   (type_is_int(type_id) || type_is_char(type_id))) {
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
        emit_store_sym(&sym);
    }
}
